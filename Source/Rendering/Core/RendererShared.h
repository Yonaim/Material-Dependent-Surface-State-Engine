#pragma once
/**
 * @file Renderer.cpp
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#include "Application/Window.h"
#include "AssetManager/Assets/MaterialAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "DebugUI/Core/DebugUI.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Rendering/Core/Renderer.h"
#include "Rendering/Scene/FrustumCulling.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "ShaderCatalog.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::Rendering
{
    namespace
    {
        using Shaders::EShader;
        const std::array<EShader, 3> SurfaceLitVariants{EShader::Rendering_Surface_Lighting_SurfaceLit_frag_m0,
                                                        EShader::Rendering_Surface_Lighting_SurfaceLit_frag_m1,
                                                        EShader::Rendering_Surface_Lighting_SurfaceLit_frag_m2};
        const std::array<EShader, 3> TexelSurfaceLitVariants{
            EShader::Rendering_Surface_Lighting_TexelSurfaceLit_frag_m0,
            EShader::Rendering_Surface_Lighting_TexelSurfaceLit_frag_m1,
            EShader::Rendering_Surface_Lighting_TexelSurfaceLit_frag_m2};
        const std::array<EShader, 3> BaseSurfaceLitVariants{EShader::Rendering_Surface_Lighting_BaseSurfaceLit_frag_m0,
                                                            EShader::Rendering_Surface_Lighting_BaseSurfaceLit_frag_m1,
                                                            EShader::Rendering_Surface_Lighting_BaseSurfaceLit_frag_m2};
        const std::array<EShader, 3> OverlayCombinedVariants{
            EShader::Rendering_Overlay_Shading_OverlayCombined_frag_m0,
            EShader::Rendering_Overlay_Shading_OverlayCombined_frag_m1,
            EShader::Rendering_Overlay_Shading_OverlayCombined_frag_m2};
        constexpr std::uint32_t FixedTimestampQueryCount = 24U;
        constexpr std::uint32_t OverlayTimestampQueriesPerLayer = 13U;

        std::vector<std::unique_ptr<GPU::TGPUImage>> CreateDepthImages(
            VkPhysicalDevice PhysicalDevice, VkDevice Device, VkExtent2D Extent, VkFormat Format, std::size_t Count)
        {
            std::vector<std::unique_ptr<GPU::TGPUImage>> Images;
            Images.reserve(Count);
            for (std::size_t Index = 0; Index < Count; ++Index)
                Images.push_back(std::make_unique<GPU::TGPUImage>(PhysicalDevice,
                                                                  Device,
                                                                  Extent,
                                                                  Format,
                                                                  VK_IMAGE_TILING_OPTIMAL,
                                                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            return Images;
        }

        std::vector<std::unique_ptr<GPU::TGPUImageView>> CreateDepthImageViews(
            VkDevice Device, const std::vector<std::unique_ptr<GPU::TGPUImage>>& Images, VkFormat Format)
        {
            std::vector<std::unique_ptr<GPU::TGPUImageView>> Views;
            Views.reserve(Images.size());
            for (const auto& Image : Images)
                Views.push_back(std::make_unique<GPU::TGPUImageView>(
                    Device, Image->GetHandle(), Format, VK_IMAGE_ASPECT_DEPTH_BIT));
            return Views;
        }

        std::vector<VkImageView> GetDepthImageViewHandles(const std::vector<std::unique_ptr<GPU::TGPUImageView>>& Views)
        {
            std::vector<VkImageView> Handles;
            Handles.reserve(Views.size());
            for (const auto& View : Views)
                Handles.push_back(View->GetHandle());
            return Handles;
        }

        struct TStaticMeshPushConstants
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };

        struct TGizmoVertex
        {
            glm::vec3 Position;
            glm::vec4 Color;
            float     EdgeCoordinate = -2.0F;
        };

        struct TGizmoPushConstants
        {
            glm::mat4    ViewProjectionModel{1.0F};
            std::int32_t HighlightAxis = -1;
            std::int32_t Padding0 = 0;
            std::int32_t Padding1 = 0;
            std::int32_t Padding2 = 0;
        };

        constexpr std::array<glm::vec4, 3> WorldAxisColors = {glm::vec4(1.0F, 0.10F, 0.10F, 1.0F),
                                                              glm::vec4(0.10F, 0.95F, 0.20F, 1.0F),
                                                              glm::vec4(0.12F, 0.35F, 1.0F, 1.0F)};

        std::vector<TGizmoVertex> BuildWorldReferenceVertices(std::uint32_t& GridVertexCount,
                                                              std::uint32_t& AxisVertexCount)
        {
            std::vector<TGizmoVertex> Vertices;
            constexpr float           GridExtent = 50.0F;
            constexpr float           GridHalfWidth = 0.008F;
            constexpr float           GridEdgeCoordinate = 1.25F;
            constexpr float           AxisHalfWidth = 0.035F;
            const glm::vec4           GridColor(1.0F);
            auto                      AddQuad = [&](glm::vec3            A,
                                                    glm::vec3            B,
                                                    glm::vec3            C,
                                                    glm::vec3            D,
                                                    glm::vec4            Color,
                                                    std::array<float, 4> EdgeCoordinates)
            {
                Vertices.insert(Vertices.end(),
                                {{A, Color, EdgeCoordinates[0]},
                                 {B, Color, EdgeCoordinates[1]},
                                 {C, Color, EdgeCoordinates[2]},
                                 {A, Color, EdgeCoordinates[0]},
                                 {C, Color, EdgeCoordinates[2]},
                                 {D, Color, EdgeCoordinates[3]}});
            };

            for (int I = -static_cast<int>(GridExtent); I <= static_cast<int>(GridExtent); ++I)
            {
                const float Coordinate = static_cast<float>(I);
                const float ExpandedHalfWidth = GridHalfWidth * GridEdgeCoordinate;
                AddQuad({Coordinate - ExpandedHalfWidth, -GridExtent, 0.0F},
                        {Coordinate + ExpandedHalfWidth, -GridExtent, 0.0F},
                        {Coordinate + ExpandedHalfWidth, GridExtent, 0.0F},
                        {Coordinate - ExpandedHalfWidth, GridExtent, 0.0F},
                        GridColor,
                        {-GridEdgeCoordinate, GridEdgeCoordinate, GridEdgeCoordinate, -GridEdgeCoordinate});
                AddQuad({-GridExtent, Coordinate - ExpandedHalfWidth, 0.0F},
                        {GridExtent, Coordinate - ExpandedHalfWidth, 0.0F},
                        {GridExtent, Coordinate + ExpandedHalfWidth, 0.0F},
                        {-GridExtent, Coordinate + ExpandedHalfWidth, 0.0F},
                        GridColor,
                        {-GridEdgeCoordinate, -GridEdgeCoordinate, GridEdgeCoordinate, GridEdgeCoordinate});
            }

            GridVertexCount = static_cast<std::uint32_t>(Vertices.size());
            constexpr std::array<float, 4> NoEdgeFade{-2.0F, -2.0F, -2.0F, -2.0F};
            AddQuad({-GridExtent, -AxisHalfWidth, 0.002F},
                    {GridExtent, -AxisHalfWidth, 0.002F},
                    {GridExtent, AxisHalfWidth, 0.002F},
                    {-GridExtent, AxisHalfWidth, 0.002F},
                    WorldAxisColors[0],
                    NoEdgeFade);
            AddQuad({-AxisHalfWidth, -GridExtent, 0.002F},
                    {AxisHalfWidth, -GridExtent, 0.002F},
                    {AxisHalfWidth, GridExtent, 0.002F},
                    {-AxisHalfWidth, GridExtent, 0.002F},
                    WorldAxisColors[1],
                    NoEdgeFade);
            AddQuad({-AxisHalfWidth, 0.0F, -GridExtent},
                    {AxisHalfWidth, 0.0F, -GridExtent},
                    {AxisHalfWidth, 0.0F, GridExtent},
                    {-AxisHalfWidth, 0.0F, GridExtent},
                    WorldAxisColors[2],
                    NoEdgeFade);
            AxisVertexCount = static_cast<std::uint32_t>(Vertices.size()) - GridVertexCount;
            return Vertices;
        }

        std::vector<TGizmoVertex> BuildTranslateGizmoVertices()
        {
            std::vector<TGizmoVertex>      Vertices;
            constexpr int                  Segments = 16;
            constexpr float                ShaftRadius = 0.025F;
            constexpr float                HeadRadius = 0.075F;
            constexpr float                ShaftEnd = 0.76F;
            const std::array<glm::vec3, 3> Axes = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};
            const auto&                    Colors = WorldAxisColors;
            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 9U);
            for (std::size_t Axis = 0; Axis < Axes.size(); ++Axis)
            {
                const glm::vec4 Color = Colors[Axis];
                auto            PushTriangle = [&](glm::vec3 A, glm::vec3 B, glm::vec3 C)
                {
                    auto World = [&](glm::vec3 P) { return U[Axis] * P.x + V[Axis] * P.y + Axes[Axis] * P.z; };
                    Vertices.push_back({World(A), Color});
                    Vertices.push_back({World(B), Color});
                    Vertices.push_back({World(C), Color});
                };
                for (int I = 0; I < Segments; ++I)
                {
                    const float     A0 = glm::two_pi<float>() * static_cast<float>(I) / Segments;
                    const float     A1 = glm::two_pi<float>() * static_cast<float>(I + 1) / Segments;
                    const glm::vec3 S0{ShaftRadius * std::cos(A0), ShaftRadius * std::sin(A0), 0.0F};
                    const glm::vec3 S1{ShaftRadius * std::cos(A1), ShaftRadius * std::sin(A1), 0.0F};
                    const glm::vec3 E0{S0.x, S0.y, ShaftEnd};
                    const glm::vec3 E1{S1.x, S1.y, ShaftEnd};
                    PushTriangle(S0, S1, E1);
                    PushTriangle(S0, E1, E0);
                    const glm::vec3 H0{HeadRadius * std::cos(A0), HeadRadius * std::sin(A0), ShaftEnd};
                    const glm::vec3 H1{HeadRadius * std::cos(A1), HeadRadius * std::sin(A1), ShaftEnd};
                    PushTriangle(H0, H1, {0.0F, 0.0F, 1.0F});
                }
            }
            return Vertices;
        }

        std::vector<TGizmoVertex> BuildRotateGizmoVertices()
        {
            std::vector<TGizmoVertex>      Vertices;
            constexpr int                  Segments = 96;
            constexpr float                Radius = 0.9F;
            constexpr float                HalfWidth = 0.012F;
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};

            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 6U);
            for (std::size_t Axis = 0; Axis < U.size(); ++Axis)
            {
                const glm::vec4 Color = WorldAxisColors[Axis];
                for (int Segment = 0; Segment < Segments; ++Segment)
                {
                    const float     A0 = glm::two_pi<float>() * static_cast<float>(Segment) / Segments;
                    const float     A1 = glm::two_pi<float>() * static_cast<float>(Segment + 1) / Segments;
                    const glm::vec3 Radial0 = U[Axis] * std::cos(A0) + V[Axis] * std::sin(A0);
                    const glm::vec3 Radial1 = U[Axis] * std::cos(A1) + V[Axis] * std::sin(A1);
                    const glm::vec3 Inner0 = (Radius - HalfWidth) * Radial0;
                    const glm::vec3 Outer0 = (Radius + HalfWidth) * Radial0;
                    const glm::vec3 Inner1 = (Radius - HalfWidth) * Radial1;
                    const glm::vec3 Outer1 = (Radius + HalfWidth) * Radial1;
                    Vertices.insert(Vertices.end(),
                                    {{Inner0, Color},
                                     {Outer0, Color},
                                     {Outer1, Color},
                                     {Inner0, Color},
                                     {Outer1, Color},
                                     {Inner1, Color}});
                }
            }
            return Vertices;
        }

        struct alignas(16) TMaterialUniform
        {
            glm::vec4     BaseColor{1.0F};
            std::uint32_t RenderMode = 0;
            std::uint32_t FlipNormalY = 1;
            float         NormalStrength = 1.0F;
            float         AmbientLight = 0.25F;
            std::uint32_t DebugStateChannel = 0;
            std::uint32_t StateChannelCount = 0;
            float         DebugViewParameter = 0.0F;
            float         ReliefShadingEnabled = 1.0F;
            glm::vec4     DebugOptions{4.0F, 0.01F, 1.0F, 1.0F};
            glm::uvec4    DebugFlags{0};
            glm::uvec4    DemoStateChannels{
                SurfaceState::InvalidStateId, SurfaceState::InvalidStateId, SurfaceState::InvalidStateId, 1U};
            glm::vec4                DemoOptions{0.65F, 0.0F, 0.48F, 1.0F};
            glm::vec4                DemoEffectOptions{1.0F, 0.35F, 1.0F, 0.16F};
            glm::uvec4               DemoExtraStateChannels{SurfaceState::InvalidStateId, 0U, 0U, 0U};
            glm::vec4                EffectColorRampStarts{0.0F};
            glm::vec4                EffectColorRampEnds{1.0F};
            std::array<glm::vec4, 4> EffectLowSaturationColors{};
            std::array<glm::vec4, 4> EffectHighSaturationColors{};
            glm::vec4                CameraPosition{0, 0, 1, 1};
            glm::vec4                WaterAppearance{0, 1, 0.22F, 100};
            glm::uvec4               ExtraDemoBindings{SurfaceState::InvalidStateId, 0, 0, 0};
        };

        constexpr std::uint32_t RenderModeWireframeUniformWhite = 20U;

        const char* GetRenderViewModeName(TRenderViewMode Mode)
        {
            switch (Mode)
            {
                case TRenderViewMode::Lit:
                    return "Lit";
                case TRenderViewMode::Unlit:
                    return "Unlit";
                case TRenderViewMode::Wireframe:
                    return "Wireframe";
                case TRenderViewMode::VertexNormalWS:
                    return "Vertex Normal (World Space)";
                case TRenderViewMode::NormalTextureTS:
                    return "Normal Texture (Tangent Space)";
                case TRenderViewMode::MappedNormalWS:
                    return "Mapped Normal (World Space)";
                case TRenderViewMode::SurfaceStateHeatmap:
                    return "Surface State Heatmap";
                case TRenderViewMode::SurfaceValidity:
                    return "Surface Validity";
                case TRenderViewMode::SurfaceID:
                    return "Surface ID";
                case TRenderViewMode::NeighborCount:
                    return "Neighbor Count";
                case TRenderViewMode::SurfaceSeam:
                    return "Surface Seam";
                case TRenderViewMode::OutgoingFluxScale:
                    return "Outgoing Flux Scale";
                case TRenderViewMode::SolverTransferWeight:
                    return "Solver Transfer Weight";
                case TRenderViewMode::MesoHeight:
                    return "Meso Color";
                case TRenderViewMode::MesoOffset:
                    return "Meso Displacement";
                case TRenderViewMode::MacroGeometry:
                    return "Macro Geometry";
                case TRenderViewMode::SurfaceTexelGrid:
                    return "Texel Grid";
                case TRenderViewMode::SurfaceAccumulation:
                    return "Accumulation";
                case TRenderViewMode::SurfaceFinalGeometry:
                    return "Final Geometry";
                case TRenderViewMode::TotalSimulationHeight:
                    return "Total Simulation Height";
                case TRenderViewMode::SurfaceTexelArea:
                    return "Texel Area Heatmap";
            }
            return "Unknown";
        }

        static_assert(sizeof(TStaticMeshPushConstants) == 128,
                      "Static mesh push constants are expected to use Vulkan's guaranteed 128-byte minimum.");
        static_assert(sizeof(TMaterialUniform) == 352, "TMaterialUniform must match the std140 shader block layout.");

        GPU::TGraphicsPipelineConfig BuildStaticMeshPipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            GPU::TGraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, Shaders::Path(EShader::Rendering_Scene_StaticMesh_vert), "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, Shaders::Path(EShader::Rendering_Scene_StaticMesh_frag), "main"},
            };
            Config.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            // Demo meshes can turn fully upside down; both sides must remain visible.
            Config.CullMode = VK_CULL_MODE_NONE;
            Config.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            Config.bDepthTestEnabled = true;
            Config.bDepthWriteEnabled = true;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS;
            Config.bBlendingEnabled = false;
            Config.DescriptorSetLayouts.push_back(MaterialLayout);

            VkVertexInputBindingDescription Binding{};
            Binding.binding = 0;
            Binding.stride = sizeof(Asset::TVertex);
            Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            Config.VertexBindings.push_back(Binding);

            auto AddAttribute = [&](std::uint32_t Location, VkFormat Format, std::uint32_t Offset)
            {
                VkVertexInputAttributeDescription Attribute{};
                Attribute.location = Location;
                Attribute.binding = 0;
                Attribute.format = Format;
                Attribute.offset = Offset;
                Config.VertexAttributes.push_back(Attribute);
            };

            AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::TVertex, Position)));
            AddAttribute(1, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::TVertex, Normal)));
            AddAttribute(2, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::TVertex, UV)));
            AddAttribute(
                3, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::TVertex, Tangent)));

            VkPushConstantRange PushConstantRange{};
            PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            PushConstantRange.offset = 0;
            PushConstantRange.size = sizeof(TStaticMeshPushConstants);
            Config.PushConstantRanges.push_back(PushConstantRange);

            return Config;
        }

        GPU::TGraphicsPipelineConfig BuildSurfaceDebugPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                     VkDescriptorSetLayout SurfaceLayout)
        {
            GPU::TGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
            Config.ShaderStages[0].ShaderPath = Shaders::Path(EShader::Debug_SurfaceDebug_vert);
            Config.ShaderStages[1].ShaderPath = Shaders::Path(EShader::Debug_SurfaceDebug_frag);
            Config.PushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            Config.DescriptorSetLayouts.push_back(SurfaceLayout);
            return Config;
        }

        GPU::TGraphicsPipelineConfig BuildWireframePipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            GPU::TGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
            Config.CullMode = VK_CULL_MODE_BACK_BIT;
            Config.PolygonMode = VK_POLYGON_MODE_LINE;
            Config.bDynamicLineWidth = true;
            return Config;
        }

        bool UsesTexelGeometry(TRenderViewMode Mode)
        {
            return Mode == TRenderViewMode::MesoHeight || Mode == TRenderViewMode::MesoOffset ||
                   Mode == TRenderViewMode::SurfaceAccumulation || Mode == TRenderViewMode::SurfaceFinalGeometry ||
                   Mode == TRenderViewMode::TotalSimulationHeight;
        }

        void SetTexelMeshVertexLayout(GPU::TGraphicsPipelineConfig& Config)
        {
            using V = SurfaceState::TSurfaceTexelMeshVertex;
            Config.VertexBindings = {{0, sizeof(V), VK_VERTEX_INPUT_RATE_VERTEX}};
            Config.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Position)},
                                       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Normal)},
                                       {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, UVSurface)},
                                       {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, DisplacementNormal)},
                                       {4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(V, Samples)},
                                       {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, Weights)}};
        }

        GPU::TGraphicsPipelineConfig BuildTexelGeometryPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                      VkDescriptorSetLayout SurfaceLayout,
                                                                      VkDescriptorSetLayout OutputLayout)
        {
            auto Config = BuildSurfaceDebugPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[0].ShaderPath = Shaders::Path(EShader::Rendering_Overlay_Geometry_TexelGeometry_vert);
            SetTexelMeshVertexLayout(Config);
            Config.DescriptorSetLayouts.push_back(OutputLayout);
            return Config;
        }

        GPU::TGraphicsPipelineConfig
        BuildSurfaceLitPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                      VkDescriptorSetLayout SurfaceLayout,
                                      VkDescriptorSetLayout OutputLayout = VK_NULL_HANDLE,
                                      VkDescriptorSetLayout StateTextureLayout = VK_NULL_HANDLE,
                                      std::uint32_t         Mode = 0)
        {
            auto Config = BuildSurfaceDebugPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[0].ShaderPath = Shaders::Path(EShader::Rendering_Surface_Lighting_SurfaceLit_vert);
            Config.ShaderStages[1].ShaderPath = Shaders::Path(SurfaceLitVariants.at(Mode));
            if (OutputLayout != VK_NULL_HANDLE)
            {
                Config.ShaderStages[0].ShaderPath =
                    Shaders::Path(EShader::Rendering_Surface_Lighting_TexelSurfaceLit_vert);
                Config.ShaderStages[1].ShaderPath = Shaders::Path(TexelSurfaceLitVariants.at(Mode));
                SetTexelMeshVertexLayout(Config);
                Config.DescriptorSetLayouts.push_back(OutputLayout);
            }
            if (StateTextureLayout != VK_NULL_HANDLE)
                Config.DescriptorSetLayouts.push_back(StateTextureLayout);
            return Config;
        }

        GPU::TGraphicsPipelineConfig BuildBaseSurfaceLitPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                       VkDescriptorSetLayout SurfaceLayout,
                                                                       VkDescriptorSetLayout OutputLayout,
                                                                       VkDescriptorSetLayout StateTextureLayout,
                                                                       std::uint32_t         Mode)
        {
            auto Config =
                BuildSurfaceLitPipelineConfig(MaterialLayout, SurfaceLayout, OutputLayout, StateTextureLayout, Mode);
            Config.ShaderStages[1].ShaderPath = Shaders::Path(BaseSurfaceLitVariants.at(Mode));
            return Config;
        }

        GPU::TGraphicsPipelineConfig
        BuildOverlayPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                   VkDescriptorSetLayout SurfaceLayout,
                                   VkDescriptorSetLayout ComputedLayout,
                                   VkDescriptorSetLayout SideLayout,
                                   bool                  bSide,
                                   bool                  bCombined = false,
                                   VkDescriptorSetLayout StateTextureLayout = VK_NULL_HANDLE,
                                   std::uint32_t         Mode = 0)
        {
            auto Config = BuildSurfaceLitPipelineConfig(MaterialLayout, SurfaceLayout, ComputedLayout);
            Config.ShaderStages[0].ShaderPath =
                Shaders::Path(bSide ? EShader::Rendering_Overlay_Geometry_OverlaySide_vert
                                    : EShader::Rendering_Overlay_Geometry_OverlayTop_vert);
            Config.ShaderStages[1].ShaderPath = Shaders::Path(
                bCombined ? OverlayCombinedVariants.at(Mode) : EShader::Rendering_Overlay_Shading_OverlayMud_frag);
            Config.CullMode = VK_CULL_MODE_NONE;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            Config.bDepthWriteEnabled = true;
            Config.bBlendingEnabled = bCombined;
            Config.DescriptorSetLayouts.push_back(SideLayout);
            if (bCombined && StateTextureLayout != VK_NULL_HANDLE)
                Config.DescriptorSetLayouts.push_back(StateTextureLayout);
            if (bSide)
            {
                Config.VertexBindings.clear();
                Config.VertexAttributes.clear();
            }
            return Config;
        }

        bool UsesAccumulationGeometry(const TStaticMeshInstance&                              Instance,
                                      const Asset::TAssetManager&                             Assets,
                                      const SurfaceState::TSurfaceDataManager&                SurfaceData,
                                      const SurfaceState::TSurfaceSharedGeometryGPUResources* Shared,
                                      const std::string&                                      StateName)
        {
            if (!Shared || !Shared->GetTexelMeshIndexBuffer() || Instance.GetMesh() == Asset::InvalidAssetHandle ||
                !SurfaceData.HasSurfaceData(Instance.GetSurfaceData()))
                return false;
            bool bHasConnectedTriangles = false;
            for (const auto& Section : Assets.GetMesh(Instance.GetMesh()).GetSections())
                if (Section.Surface < Shared->GetTexelMeshRanges().size() &&
                    Shared->GetTexelMeshRanges()[Section.Surface].IndexCount > 0)
                    bHasConnectedTriangles = true;
            if (!bHasConnectedTriangles)
                return false;
            for (const auto Profile : SurfaceData.GetSurfaceProfileTable(Instance.GetSurfaceData()))
                if (Assets.GetSRProfile(Profile).GetData().States.contains(StateName))
                    return true;
            return false;
        }

        GPU::TGraphicsPipelineConfig BuildGizmoPipelineConfig()
        {
            GPU::TGraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, Shaders::Path(EShader::Rendering_Scene_Gizmo_vert), "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, Shaders::Path(EShader::Rendering_Scene_Gizmo_frag), "main"},
            };
            Config.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            Config.CullMode = VK_CULL_MODE_NONE;
            Config.bDepthTestEnabled = false;
            Config.bDepthWriteEnabled = false;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            VkVertexInputBindingDescription Binding{};
            Binding.binding = 0;
            Binding.stride = sizeof(TGizmoVertex);
            Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            Config.VertexBindings.push_back(Binding);
            VkVertexInputAttributeDescription Position{};
            Position.location = 0;
            Position.binding = 0;
            Position.format = VK_FORMAT_R32G32B32_SFLOAT;
            Position.offset = static_cast<std::uint32_t>(offsetof(TGizmoVertex, Position));
            Config.VertexAttributes.push_back(Position);
            VkVertexInputAttributeDescription Color{};
            Color.location = 1;
            Color.binding = 0;
            Color.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            Color.offset = static_cast<std::uint32_t>(offsetof(TGizmoVertex, Color));
            Config.VertexAttributes.push_back(Color);
            VkPushConstantRange Push{};
            Push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            Push.size = sizeof(TGizmoPushConstants);
            Config.PushConstantRanges.push_back(Push);
            return Config;
        }

        GPU::TGraphicsPipelineConfig BuildWorldReferencePipelineConfig()
        {
            GPU::TGraphicsPipelineConfig Config = BuildGizmoPipelineConfig();
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, Shaders::Path(EShader::Rendering_Scene_WorldReference_vert), "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, Shaders::Path(EShader::Rendering_Scene_WorldReference_frag), "main"},
            };
            VkVertexInputAttributeDescription EdgeCoordinate{};
            EdgeCoordinate.location = 2;
            EdgeCoordinate.binding = 0;
            EdgeCoordinate.format = VK_FORMAT_R32_SFLOAT;
            EdgeCoordinate.offset = static_cast<std::uint32_t>(offsetof(TGizmoVertex, EdgeCoordinate));
            Config.VertexAttributes.push_back(EdgeCoordinate);
            Config.bDepthTestEnabled = true;
            Config.bDepthWriteEnabled = false;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            Config.bBlendingEnabled = true;
            return Config;
        }

        template <typename TCallback> class TScopeExit final
        {
        public:
            explicit TScopeExit(TCallback Callback) : Callback(std::move(Callback))
            {
            }
            ~TScopeExit()
            {
                if (bActive)
                    Callback();
            }
            void Release() noexcept
            {
                bActive = false;
            }

        private:
            TCallback Callback;
            bool      bActive = true;
        };
    } // 내부 네임스페이스

}