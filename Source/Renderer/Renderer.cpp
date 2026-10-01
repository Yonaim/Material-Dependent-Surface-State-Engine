/**
 * @file Renderer.cpp
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#include "Renderer/Renderer.h"

#include "Application/Window.h"
#include "AssetManager/Assets/MaterialAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "DebugUI/DebugUI.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS
{
    namespace
    {
        constexpr std::uint32_t FixedTimestampQueryCount = 21U;
        constexpr std::uint32_t OverlayTimestampQueriesPerLayer = 13U;

        std::vector<std::unique_ptr<TGPUImage>> CreateDepthImages(VkPhysicalDevice PhysicalDevice, VkDevice Device,
            VkExtent2D Extent, VkFormat Format, std::size_t Count)
        {
            std::vector<std::unique_ptr<TGPUImage>> Images;
            Images.reserve(Count);
            for (std::size_t Index = 0; Index < Count; ++Index)
                Images.push_back(std::make_unique<TGPUImage>(PhysicalDevice, Device, Extent, Format,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            return Images;
        }

        std::vector<std::unique_ptr<TGPUImageView>> CreateDepthImageViews(VkDevice Device,
            const std::vector<std::unique_ptr<TGPUImage>>& Images, VkFormat Format)
        {
            std::vector<std::unique_ptr<TGPUImageView>> Views;
            Views.reserve(Images.size());
            for (const auto& Image : Images)
                Views.push_back(std::make_unique<TGPUImageView>(Device, Image->GetHandle(), Format,
                    VK_IMAGE_ASPECT_DEPTH_BIT));
            return Views;
        }

        std::vector<VkImageView> GetDepthImageViewHandles(
            const std::vector<std::unique_ptr<TGPUImageView>>& Views)
        {
            std::vector<VkImageView> Handles;
            Handles.reserve(Views.size());
            for (const auto& View : Views) Handles.push_back(View->GetHandle());
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
            glm::mat4 ViewProjectionModel{1.0F};
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
            constexpr float GridExtent = 50.0F;
            constexpr float GridHalfWidth = 0.008F;
            constexpr float GridEdgeCoordinate = 1.25F;
            constexpr float AxisHalfWidth = 0.035F;
            const glm::vec4 GridColor(1.0F);
            auto AddQuad = [&](glm::vec3 A,
                               glm::vec3 B,
                               glm::vec3 C,
                               glm::vec3 D,
                               glm::vec4 Color,
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
            std::vector<TGizmoVertex> Vertices;
            constexpr int Segments = 16;
            constexpr float ShaftRadius = 0.025F;
            constexpr float HeadRadius = 0.075F;
            constexpr float ShaftEnd = 0.76F;
            const std::array<glm::vec3, 3> Axes = {glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)};
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};
            const auto& Colors = WorldAxisColors;
            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 9U);
            for (std::size_t Axis = 0; Axis < Axes.size(); ++Axis)
            {
                const glm::vec4 Color = Colors[Axis];
                auto PushTriangle = [&](glm::vec3 A, glm::vec3 B, glm::vec3 C)
                {
                    auto World = [&](glm::vec3 P) { return U[Axis] * P.x + V[Axis] * P.y + Axes[Axis] * P.z; };
                    Vertices.push_back({World(A), Color});
                    Vertices.push_back({World(B), Color});
                    Vertices.push_back({World(C), Color});
                };
                for (int I = 0; I < Segments; ++I)
                {
                    const float A0 = glm::two_pi<float>() * static_cast<float>(I) / Segments;
                    const float A1 = glm::two_pi<float>() * static_cast<float>(I + 1) / Segments;
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
            std::vector<TGizmoVertex> Vertices;
            constexpr int Segments = 96;
            constexpr float Radius = 0.9F;
            constexpr float HalfWidth = 0.012F;
            const std::array<glm::vec3, 3> U = {glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0)};
            const std::array<glm::vec3, 3> V = {glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0)};

            Vertices.reserve(3U * static_cast<std::size_t>(Segments) * 6U);
            for (std::size_t Axis = 0; Axis < U.size(); ++Axis)
            {
                const glm::vec4 Color = WorldAxisColors[Axis];
                for (int Segment = 0; Segment < Segments; ++Segment)
                {
                    const float A0 = glm::two_pi<float>() * static_cast<float>(Segment) / Segments;
                    const float A1 = glm::two_pi<float>() * static_cast<float>(Segment + 1) / Segments;
                    const glm::vec3 Radial0 = U[Axis] * std::cos(A0) + V[Axis] * std::sin(A0);
                    const glm::vec3 Radial1 = U[Axis] * std::cos(A1) + V[Axis] * std::sin(A1);
                    const glm::vec3 Inner0 = (Radius - HalfWidth) * Radial0;
                    const glm::vec3 Outer0 = (Radius + HalfWidth) * Radial0;
                    const glm::vec3 Inner1 = (Radius - HalfWidth) * Radial1;
                    const glm::vec3 Outer1 = (Radius + HalfWidth) * Radial1;
                    Vertices.insert(Vertices.end(), {{Inner0, Color}, {Outer0, Color}, {Outer1, Color},
                                                     {Inner0, Color}, {Outer1, Color}, {Inner1, Color}});
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
            glm::uvec4    DemoStateChannels{InvalidStateId, InvalidStateId, InvalidStateId, 1U};
            glm::vec4     DemoOptions{0.65F, 0.16F, 0.48F, 1.0F};
            glm::vec4     DemoEffectOptions{1.0F, 1.0F, 1.0F, 0.16F};
            glm::vec4     WetnessTint{0.44F, 0.56F, 0.68F, 1.0F};
            glm::vec4     WaterFilmTint{0.35F, 0.53F, 0.68F, 1.0F};
            glm::uvec4    DemoExtraStateChannels{InvalidStateId, 0U, 0U, 0U};
            glm::vec4     CameraPosition{0, 0, 1, 1};
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
        static_assert(sizeof(TMaterialUniform) == 192, "TMaterialUniform must match the std140 shader block layout.");

        TGraphicsPipelineConfig BuildStaticMeshPipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            TGraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/StaticMesh.vert.spv", "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/StaticMesh.frag.spv", "main"},
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
            Binding.stride = sizeof(TVertex);
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

            AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(TVertex, Position)));
            AddAttribute(1, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(TVertex, Normal)));
            AddAttribute(2, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(TVertex, UV)));
            AddAttribute(3, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(TVertex, Tangent)));

            VkPushConstantRange PushConstantRange{};
            PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            PushConstantRange.offset = 0;
            PushConstantRange.size = sizeof(TStaticMeshPushConstants);
            Config.PushConstantRanges.push_back(PushConstantRange);

        return Config;
    }

        TGraphicsPipelineConfig BuildSurfaceDebugPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                             VkDescriptorSetLayout SurfaceLayout)
    {
        TGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
        Config.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.vert.spv";
        Config.ShaderStages[1].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.frag.spv";
        Config.PushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        Config.DescriptorSetLayouts.push_back(SurfaceLayout);
            return Config;
        }

        TGraphicsPipelineConfig BuildWireframePipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            TGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
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

        void SetTexelMeshVertexLayout(TGraphicsPipelineConfig& Config)
        {
            using V = TSurfaceTexelMeshVertex;
            Config.VertexBindings = {{0, sizeof(V), VK_VERTEX_INPUT_RATE_VERTEX}};
            Config.VertexAttributes = {
                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Position)},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Normal)},
                {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, UVSurface)},
                {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, DisplacementNormal)},
                {4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(V, Samples)},
                {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, Weights)}};
        }

        TGraphicsPipelineConfig BuildTexelGeometryPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                  VkDescriptorSetLayout SurfaceLayout,
                                                                  VkDescriptorSetLayout OutputLayout)
        {
            auto Config = BuildSurfaceDebugPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Debug/TexelGeometry.vert.spv";
            SetTexelMeshVertexLayout(Config);
            Config.DescriptorSetLayouts.push_back(OutputLayout);
            return Config;
        }

        TGraphicsPipelineConfig BuildSurfaceLitPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                               VkDescriptorSetLayout SurfaceLayout,
                                                               VkDescriptorSetLayout OutputLayout = VK_NULL_HANDLE)
        {
            auto Config = BuildSurfaceDebugPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Rendering/SurfaceLit.vert.spv";
            Config.ShaderStages[1].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Rendering/SurfaceLit.frag.spv";
            if (OutputLayout != VK_NULL_HANDLE)
            {
                Config.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Rendering/TexelSurfaceLit.vert.spv";
                Config.ShaderStages[1].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Rendering/TexelSurfaceLit.frag.spv";
                SetTexelMeshVertexLayout(Config);
                Config.DescriptorSetLayouts.push_back(OutputLayout);
            }
            return Config;
        }

        TGraphicsPipelineConfig BuildBaseSurfaceLitPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                   VkDescriptorSetLayout SurfaceLayout)
        {
            auto Config = BuildSurfaceLitPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[1].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Rendering/BaseSurfaceLit.frag.spv";
            return Config;
        }

        TGraphicsPipelineConfig BuildOverlayPipelineConfig(VkDescriptorSetLayout MaterialLayout,
            VkDescriptorSetLayout SurfaceLayout, VkDescriptorSetLayout ComputedLayout,
            VkDescriptorSetLayout SideLayout, bool bWater, bool bSide, bool bLava = false)
        {
            auto Config = BuildSurfaceLitPipelineConfig(MaterialLayout, SurfaceLayout, ComputedLayout);
            Config.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) +
                (bSide ? "/Rendering/OverlaySide.vert.spv" : "/Rendering/OverlayTop.vert.spv");
            Config.ShaderStages[1].ShaderPath = std::string(MDSS_SHADER_DIR) +
                (bLava ? "/Rendering/OverlayLava.frag.spv" :
                 bWater ? "/Rendering/OverlayWater.frag.spv" : "/Rendering/OverlayMud.frag.spv");
            Config.CullMode = VK_CULL_MODE_NONE;
            Config.bDepthWriteEnabled = !bWater;
            Config.bBlendingEnabled = bWater;
            Config.DescriptorSetLayouts.push_back(SideLayout);
            if (bSide)
            {
                Config.VertexBindings.clear();
                Config.VertexAttributes.clear();
            }
            return Config;
        }

        bool UsesAccumulationGeometry(const TStaticMeshInstance& Instance, const TAssetManager& Assets,
                                      const TSurfaceSharedGeometryGPUResources* Shared, const std::string& StateName)
        {
            if (!Shared || !Shared->GetTexelMeshIndexBuffer() || Instance.GetMesh() == InvalidAssetHandle ||
                !Assets.HasSurfaceData(Instance.GetSurfaceData())) return false;
            bool bHasConnectedTriangles = false;
            for (const auto& Section : Assets.GetMesh(Instance.GetMesh()).GetSections())
                if (Section.Surface < Shared->GetTexelMeshRanges().size() &&
                    Shared->GetTexelMeshRanges()[Section.Surface].IndexCount > 0)
                    bHasConnectedTriangles = true;
            if (!bHasConnectedTriangles) return false;
            for (const auto Profile : Assets.GetSurfaceProfileTable(Instance.GetSurfaceData()))
                if (Assets.GetSRProfile(Profile).GetData().States.contains(StateName)) return true;
            return false;
        }

        TGraphicsPipelineConfig BuildGizmoPipelineConfig()
        {
            TGraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/Gizmo.vert.spv", "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/Gizmo.frag.spv", "main"},
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

        TGraphicsPipelineConfig BuildWorldReferencePipelineConfig()
        {
            TGraphicsPipelineConfig Config = BuildGizmoPipelineConfig();
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/WorldReference.vert.spv", "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/WorldReference.frag.spv", "main"},
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
    } // 내부 네임스페이스

    TRenderer::TRenderer(const TVulkanContext& Context,
                         TWindow& TWindow,
                         TAssetManager& Assets,
                         const TScene& Scene,
                         TSurfaceStateSystem& SurfaceStates)
        : Context(Context), TargetWindow(TWindow), Assets(Assets), SurfaceStates(SurfaceStates),
          SwapchainData(Context, TWindow),
          DepthFormat(FindDepthFormat(Context.GetPhysicalDevice())),
          DepthImages(CreateDepthImages(Context.GetPhysicalDevice(), Context.GetDevice(),
              SwapchainData.GetExtent(), DepthFormat, SwapchainData.GetImageViews().size())),
          DepthImageViews(CreateDepthImageViews(Context.GetDevice(), DepthImages, DepthFormat)),
          MainRenderPass(Context.GetDevice(), SwapchainData.GetImageFormat(), DepthFormat),
          MaterialDescriptorSetLayout(CreateMaterialDescriptorSetLayout(Context.GetDevice())),
          StaticMeshPipeline(Context.GetDevice(),
                             MainRenderPass.GetHandle(),
                             BuildStaticMeshPipelineConfig(MaterialDescriptorSetLayout)),
          WireframePipeline(Context.GetDevice(),
                            MainRenderPass.GetHandle(),
                            BuildWireframePipelineConfig(MaterialDescriptorSetLayout)),
          GizmoPipeline(Context.GetDevice(), MainRenderPass.GetHandle(), BuildGizmoPipelineConfig()),
          WorldReferencePipeline(Context.GetDevice(), MainRenderPass.GetHandle(), BuildWorldReferencePipelineConfig()),
          MainFramebuffers(Context.GetDevice(),
                           MainRenderPass.GetHandle(),
                           SwapchainData.GetExtent(),
                           SwapchainData.GetImageViews(),
                           GetDepthImageViewHandles(DepthImageViews)),
          FrameContext(Context)
    {
        VkPhysicalDeviceFeatures DeviceFeatures{};
        vkGetPhysicalDeviceFeatures(Context.GetPhysicalDevice(), &DeviceFeatures);
        VkPhysicalDeviceDriverProperties DriverProperties{};
        DriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        VkPhysicalDeviceProperties2 DevicePropertiesWithDriver{};
        DevicePropertiesWithDriver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        DevicePropertiesWithDriver.pNext = &DriverProperties;
        vkGetPhysicalDeviceProperties2(Context.GetPhysicalDevice(), &DevicePropertiesWithDriver);
        const VkPhysicalDeviceProperties& DeviceProperties = DevicePropertiesWithDriver.properties;
        const VkDeviceSize UniformAlignment = std::max<VkDeviceSize>(
            DeviceProperties.limits.minUniformBufferOffsetAlignment, 1U);
        MaterialUniformStride = (sizeof(TMaterialUniform) + UniformAlignment - 1U) / UniformAlignment * UniformAlignment;
        bRenderPassSubstageTimingsReliable =
            DriverProperties.driverID != VK_DRIVER_ID_MOLTENVK || DeviceProperties.vendorID != 0x106BU;
        bSupportsWireframeLineWidth = DeviceFeatures.wideLines == VK_TRUE;
        if (bSupportsWireframeLineWidth)
        {
            WireframeLineWidthMin = std::max(1.0F, DeviceProperties.limits.lineWidthRange[0]);
            WireframeLineWidthMax = std::max(WireframeLineWidthMin, DeviceProperties.limits.lineWidthRange[1]);
            WireframeLineWidth = std::clamp(2.0F, WireframeLineWidthMin, WireframeLineWidthMax);
        }

        std::vector<TGizmoVertex> GizmoVertices = BuildWorldReferenceVertices(WorldGridVertexCount,
                                                                               WorldAxisVertexCount);
        const std::vector<TGizmoVertex> TranslateGizmoVertices = BuildTranslateGizmoVertices();
        TranslateGizmoVertexCount = static_cast<std::uint32_t>(TranslateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), TranslateGizmoVertices.begin(), TranslateGizmoVertices.end());
        const std::vector<TGizmoVertex> RotateGizmoVertices = BuildRotateGizmoVertices();
        RotateGizmoVertexCount = static_cast<std::uint32_t>(RotateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), RotateGizmoVertices.begin(), RotateGizmoVertices.end());
        GizmoVertexBuffer =
            std::make_unique<TGPUBuffer>(Context.GetPhysicalDevice(),
                                                        Context.GetDevice(),
                                                        static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)),
                                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        GizmoVertexBuffer->Upload(GizmoVertices.data(),
                                  static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)));
        CreateMaterialDescriptorResources();
        if (const TSurfaceStateDescriptorResources* Descriptors =
                SurfaceStates.GetGPUResources().GetAnyInstanceDescriptors())
        {
            SurfaceDebugPipeline = std::make_unique<TGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
            TexelInspector = std::make_unique<TTexelInspector>(Context.GetPhysicalDevice(),
                                                               Context.GetDevice(),
                                                               Descriptors->GetLayout(),
                                                               TRenderContext::MaxFramesInFlight);
            TexelGeometryPreview = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                Context.GetDevice(), Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size());
            TexelGeometryPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(),
                MainRenderPass.GetHandle(), BuildTexelGeometryPipelineConfig(MaterialDescriptorSetLayout,
                    Descriptors->GetLayout(), TexelGeometryPreview->GetOutputLayout()));
            SurfaceLitPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
            BaseSurfaceLitPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
            MudLayerGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(), Context.GetDevice(),
                Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
            WaterLayerGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(), Context.GetDevice(),
                Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
            LavaLayerGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(), Context.GetDevice(),
                Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
            HeightFieldSmoothing = std::make_unique<THeightFieldSmoothing>(Context.GetPhysicalDevice(),
                Context.GetDevice(), Descriptors->GetLayout(), MudLayerGeometry->GetOutputLayout(),
                Scene.GetStaticMeshInstances().size());
            OverlaySides = std::make_unique<TAccumulationOverlaySides>(Context.GetPhysicalDevice(), Context.GetDevice(),
                Descriptors->GetLayout(), MudLayerGeometry->GetOutputLayout(), Scene.GetStaticMeshInstances().size());
            MudOverlayTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    MudLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), false, false));
            MudOverlaySidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    MudLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), false, true));
            WaterOverlayTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    WaterLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), true, false));
            WaterOverlaySidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    WaterLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), true, true));
            LavaOverlayTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    LavaLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), false, false, true));
            LavaOverlaySidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                    LavaLayerGeometry->GetOutputLayout(), OverlaySides->GetLayout(), false, true, true));
        }
        CreateRenderFinishedSemaphores();
        VkPhysicalDeviceProperties PhysicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(Context.GetPhysicalDevice(), &PhysicalDeviceProperties);
        TimestampPeriodNanoseconds = PhysicalDeviceProperties.limits.timestampPeriod;
        std::uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(Context.GetPhysicalDevice(), &QueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(Context.GetPhysicalDevice(), &QueueFamilyCount, QueueFamilies.data());
        const std::uint32_t GraphicsQueueFamily = Context.GetQueues().GetFamilyIndices().GraphicsFamily.value();
        if (GraphicsQueueFamily < QueueFamilies.size())
        {
            TimestampValidBits = QueueFamilies[GraphicsQueueFamily].timestampValidBits;
        }
        if (TimestampValidBits > 0)
        {
            CreateTimestampQueryPool(SurfaceStates.GetSolverTimestampSlotCount());
        }
        else
        {
            TLogger::Info("TRenderer", "Graphics queue does not support timestamp queries.");
        }
        Assets.SetSimulationResolution(Scene.GetSimulationResolution());
        TLogger::Info("TRenderer", "Static mesh pipeline ready with MTL base-color and tangent-space normal mapping.");
        TLogger::Debug("TRenderer",
                      "Depth format=" + std::to_string(static_cast<int>(DepthFormat)) +
                          ", material descriptor count=" + std::to_string(MaterialResources.size()) + ".");
    }

    TRenderer::~TRenderer()
    {
        if (Context.GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Context.GetDevice());
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(Context.GetDevice(), TimestampQueryPool, nullptr);
            TimestampQueryPool = VK_NULL_HANDLE;
        }
        DestroyRenderFinishedSemaphores();

        SurfaceDebugPipeline.reset();
        SurfaceLitPipeline.reset();
        BaseSurfaceLitPipeline.reset();
        MudOverlayTopPipeline.reset();
        MudOverlaySidePipeline.reset();
        WaterOverlayTopPipeline.reset();
        WaterOverlaySidePipeline.reset();
        LavaOverlayTopPipeline.reset();
        LavaOverlaySidePipeline.reset();
        TexelGeometryPipeline.reset();
        TexelGeometryPreview.reset();
        OverlaySides.reset();
        HeightFieldSmoothing.reset();
        MudLayerGeometry.reset();
        WaterLayerGeometry.reset();
        LavaLayerGeometry.reset();
        TexelInspector.reset();
        if (MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), MaterialDescriptorPool, nullptr);
            MaterialDescriptorPool = VK_NULL_HANDLE;
        }
        MaterialResources.clear();
        if (MaterialDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(Context.GetDevice(), MaterialDescriptorSetLayout, nullptr);
            MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    void TRenderer::RenderFrame(const TScene& SceneData, TDebugUI& DebugInterface, float DeltaTime,
                                bool bSuspendSimulationClock)
    {
        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        using TClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](TClock::duration Duration)
        {
            return std::chrono::duration<float, std::milli>(Duration).count();
        };
        auto CpuStageStart = TClock::now();
        FrameContext.WaitForCurrentFrame();
        if (DebugInterface.GetViewportCount() > MaterialViewportCapacity)
        {
            if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
                throw std::runtime_error("Failed to wait for GPU before growing viewport uniforms.");
            MaterialViewportCapacity = std::max(DebugInterface.GetViewportCount(), MaterialViewportCapacity * 2U);
            if (MaterialUniformStride * (MaterialViewportCapacity - 1U) >
                std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("Too many viewports for dynamic material uniform offsets.");
            CreateMaterialDescriptorResources();
        }
        ProfilingStats.FrameFenceWaitCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        if (TexelInspector)
            TexelInspector->CompleteFrame(FrameIndex);
        if (OverlaySides)
        {
            const auto Activity = OverlaySides->CompleteFrame(FrameIndex);
            ProfilingStats.OverlayActiveTopTriangles = Activity.Active;
            ProfilingStats.OverlayTotalTopTriangles = Activity.Total;
        }
        if (TimestampQueryPool != VK_NULL_HANDLE && bTimestampQueriesSubmitted[FrameIndex])
        {
            const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
            const std::uint32_t OverlayQueryOffset =
                FixedTimestampQueryCount + SolverTimestampStepsSubmitted[FrameIndex] * SolverTimestampSlotCount * 8U;
            const std::uint32_t QueryCount = OverlayQueryOffset +
                OverlayTimestampLayersSubmitted[FrameIndex] * OverlayTimestampQueriesPerLayer;
            std::vector<std::uint64_t> Timestamps(QueryCount, 0U);
            const VkResult QueryResult = vkGetQueryPoolResults(Context.GetDevice(),
                                                               TimestampQueryPool,
                                                               QueryBase,
                                                               QueryCount,
                                                               Timestamps.size() * sizeof(std::uint64_t),
                                                               Timestamps.data(),
                                                               sizeof(std::uint64_t),
                                                               VK_QUERY_RESULT_64_BIT);
            if (QueryResult == VK_SUCCESS && TimestampPeriodNanoseconds > 0.0F)
            {
                ProfilingStats.OverlayDrawMode = OverlayDrawModesSubmitted[FrameIndex];
                const auto ToMilliseconds = [this](std::uint64_t Start, std::uint64_t End)
                {
                    std::uint64_t ElapsedTicks = End - Start;
                    if (TimestampValidBits < 64U)
                    {
                        ElapsedTicks &= (std::uint64_t{1} << TimestampValidBits) - 1U;
                    }
                    return static_cast<float>(static_cast<double>(ElapsedTicks) * TimestampPeriodNanoseconds / 1.0e6);
                };
                ProfilingStats.RenderPreparationGpuMilliseconds = ToMilliseconds(Timestamps[0], Timestamps[1]);
                ProfilingStats.SceneDrawGpuMilliseconds = ToMilliseconds(Timestamps[2],
                    Timestamps[bRenderPassSubstageTimingsReliable ? 3U : 20U]);
                ProfilingStats.TexelInspectorGpuMilliseconds = ToMilliseconds(Timestamps[4], Timestamps[5]);
                ProfilingStats.BaseMeshDrawGpuMilliseconds = ToMilliseconds(Timestamps[6], Timestamps[7]);
                ProfilingStats.MudOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[8], Timestamps[9]);
                ProfilingStats.WaterFilmOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[10], Timestamps[11]);
                ProfilingStats.OverlayPreparationGpuMilliseconds = ToMilliseconds(Timestamps[12], Timestamps[13]);
                ProfilingStats.SceneSetupGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[6]);
                ProfilingStats.RenderPassBeginGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[14]);
                ProfilingStats.RenderPassColorStageGpuMilliseconds = ToMilliseconds(Timestamps[15], Timestamps[16]);
                ProfilingStats.RenderPassDepthStageGpuMilliseconds = ToMilliseconds(Timestamps[17], Timestamps[18]);
                ProfilingStats.ViewportSetupGpuMilliseconds = ToMilliseconds(Timestamps[14], Timestamps[6]);
                ProfilingStats.UIDrawGpuMilliseconds = ToMilliseconds(Timestamps[3], Timestamps[19]);
                ProfilingStats.RenderPassEndGpuMilliseconds = ToMilliseconds(Timestamps[19], Timestamps[20]);
                ProfilingStats.SceneBetweenDrawsGpuMilliseconds =
                    ToMilliseconds(Timestamps[7], Timestamps[8]) +
                    ToMilliseconds(Timestamps[9], Timestamps[10]);
                ProfilingStats.SceneTailGpuMilliseconds = ToMilliseconds(Timestamps[11], Timestamps[3]);
                ProfilingStats.OverlayGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayHeightGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayNormalVertexGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySmoothingGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesDispatchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayBoundarySearchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayTopCommandGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayCoverageSampleGpuMilliseconds = 0.0F;
                for (std::uint32_t Layer = 0; Layer < OverlayTimestampLayersSubmitted[FrameIndex]; ++Layer)
                {
                    const std::uint32_t LayerQuery = OverlayQueryOffset +
                        Layer * OverlayTimestampQueriesPerLayer;
                    ProfilingStats.OverlayGeometryGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlayHeightGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 12U]);
                    ProfilingStats.OverlayNormalVertexGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 12U], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlaySmoothingGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 2U], Timestamps[LayerQuery + 3U]);
                    ProfilingStats.OverlaySidesGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 11U]);
                    ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 5U]);
                    ProfilingStats.OverlayCoverageSampleGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]);
                    ProfilingStats.OverlayBoundarySearchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]);
                    ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 6U], Timestamps[LayerQuery + 7U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 8U], Timestamps[LayerQuery + 9U]);
                    ProfilingStats.OverlayTopCommandGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesDispatchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 10U], Timestamps[LayerQuery + 11U]);
                }
                LastRenderGpuMilliseconds = ProfilingStats.RenderPreparationGpuMilliseconds +
                    ProfilingStats.SceneDrawGpuMilliseconds;
                if (bRenderPassSubstageTimingsReliable)
                    LastRenderGpuMilliseconds += ProfilingStats.UIDrawGpuMilliseconds +
                        ProfilingStats.RenderPassEndGpuMilliseconds;
                LastSolverPass1GpuMilliseconds = 0.0F;
                LastSolverPass2GpuMilliseconds = 0.0F;
                ProfilingStats.AccumulationGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.TransferWeightGpuMilliseconds = 0.0F;
                if (SolverTimestampStepsSubmitted[FrameIndex] > 0U)
                {
                    for (std::uint32_t Slot = 0; Slot < SolverTimestampSlotCount * SolverTimestampStepsSubmitted[FrameIndex]; ++Slot)
                    {
                        const std::uint32_t SlotQuery = FixedTimestampQueryCount + Slot * 8U;
                        ProfilingStats.AccumulationGeometryGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery], Timestamps[SlotQuery + 1U]);
                        ProfilingStats.TransferWeightGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 2U], Timestamps[SlotQuery + 3U]);
                        LastSolverPass1GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 4U], Timestamps[SlotQuery + 5U]);
                        LastSolverPass2GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 6U], Timestamps[SlotQuery + 7U]);
                    }
                }
                ProfilingStats.SolverPass1GpuMilliseconds = LastSolverPass1GpuMilliseconds;
                ProfilingStats.SolverPass2GpuMilliseconds = LastSolverPass2GpuMilliseconds;
                ProfilingStats.SolverGpuMilliseconds = LastSolverPass1GpuMilliseconds +
                    LastSolverPass2GpuMilliseconds + ProfilingStats.AccumulationGeometryGpuMilliseconds +
                    ProfilingStats.TransferWeightGpuMilliseconds;
                LastSolverGpuMilliseconds = ProfilingStats.SolverGpuMilliseconds;
            }
            bTimestampQueriesSubmitted[FrameIndex] = false;
            SolverTimestampStepsSubmitted[FrameIndex] = 0;
            OverlayTimestampLayersSubmitted[FrameIndex] = 0;
        }

        const bool bResetSolverState = DebugInterface.ConsumeSolverResetRequest();
        const bool bRequestSolverStep = DebugInterface.ConsumeSolverStepRequest();
        if (bResetSolverState)
        {
            SurfaceStates.ResetState();
            SimulationClock.Reset();
            SimulationStepSerial = 0;
            TotalHeightCacheEntries.clear();
            bTotalHeightCacheHit = false;
            if (TexelInspector)
                TexelInspector->Invalidate();
        }
        if (!bSuspendSimulationClock)
            SimulationClock.Accumulate(DeltaTime, DebugInterface.GetSimulationTimeScale(),
                                       bResetSolverState || DebugInterface.IsSimulationPaused());
        // A changed setting starts with no old backlog. Other long frames
        // retain a bounded catch-up budget, so playback stays responsive.
        const double PendingTickBudget = std::max(MaxRealtimePendingTicks,
            std::ceil(static_cast<double>(DebugInterface.GetSimulationTimeScale())));
        ProfilingStats.DroppedSimulationSeconds = static_cast<float>(SimulationClock.LimitPendingSeconds(
            bSuspendSimulationClock ? 0.0 : PendingTickBudget * FixedSimulationStepSeconds));
        if (bSuspendSimulationClock)
            SurfaceStates.PrepareTransferWeightCachesForSettingChange();

        if (TargetWindow.WasFramebufferResized())
        {
            RecreateSwapchain(DebugInterface);
            return;
        }

        std::uint32_t  ImageIndex = 0;
        CpuStageStart = TClock::now();
        const VkResult AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);
        ProfilingStats.AcquireCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);

        if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            TLogger::Debug("TRenderer", "TSwapchain became out of date while acquiring; recreating it.");
            RecreateSwapchain(DebugInterface);
            return;
        }
        if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire a Vulkan swapchain image.");
        }

        FrameContext.ResetCurrentFence();

        const VkCommandBuffer CommandBuffer = FrameContext.GetCurrentCommandBuffer();
        if (vkResetCommandBuffer(CommandBuffer, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset Vulkan command buffer.");
        }

        MaximumSimulationStep = DebugInterface.IsAutoSubsteppingEnabled() ?
            SurfaceStates.GetMaximumStableDeltaTime() : FixedSimulationStepSeconds;
        const auto SimulationSteps = bResetSolverState ? std::vector<float>{} : SimulationClock.Consume(
            MaximumSimulationStep, DebugInterface.IsFixedSimulationTimestep(),
            DebugInterface.IsAutoSubsteppingEnabled(), DebugInterface.IsSimulationPaused(), bRequestSolverStep);
        if (!DebugInterface.IsFixedSimulationTimestep() && !DebugInterface.IsAutoSubsteppingEnabled() &&
            !SimulationSteps.empty())
            MaximumSimulationStep = SimulationSteps.front();
        LastSimulationStepCount = static_cast<std::uint32_t>(SimulationSteps.size());
        ProfilingStats.SimulationSteps = LastSimulationStepCount;
        ProfilingStats.SimulatedThisFrameSeconds = 0.0F;
        for (const float StepSeconds : SimulationSteps)
            ProfilingStats.SimulatedThisFrameSeconds += StepSeconds;
        ProfilingStats.PendingSimulationSeconds = static_cast<float>(SimulationClock.GetPendingSeconds());
        ProfilingStats.SimulationInstances = static_cast<std::uint32_t>(SurfaceStates.GetSolverTimestampSlotCount());
        ProfilingStats.SimulationTexels = 0;
        ProfilingStats.StateChannels = 0;
        ProfilingStats.SimulationResolution = GetSimulationResolution();
        const auto& GPUResources = SurfaceStates.GetGPUResources();
        for (std::size_t Instance = 0; Instance < GPUResources.GetSceneInstanceCount(); ++Instance)
        {
            if (GPUResources.GetInstanceDescriptors(Instance) == nullptr) continue;
            ProfilingStats.SimulationTexels += GPUResources.GetInstanceTexelCount(Instance);
            ProfilingStats.StateChannels = static_cast<std::uint32_t>(GPUResources.GetInstanceChannelCount(Instance));
        }
        if (SimulationSteps.empty())
            LastSolverGpuMilliseconds = LastSolverPass1GpuMilliseconds = LastSolverPass2GpuMilliseconds = 0.0F;
        CpuStageStart = TClock::now();
        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData, DebugInterface, SimulationSteps);
        OverlayDrawModesSubmitted[FrameIndex] = OverlayDrawProfilingMode;
        ProfilingStats.CommandRecordCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            bTimestampQueriesSubmitted[FrameIndex] = true;
            SolverTimestampStepsSubmitted[FrameIndex] = LastSimulationStepCount;
        }

        const VkSemaphore          WaitSemaphore = FrameContext.GetImageAvailableSemaphore();
        if (ImageIndex >= RenderFinishedSemaphores.size())
        {
            throw std::runtime_error("Acquired swapchain image has no render-finished semaphore.");
        }
        const VkSemaphore          SignalSemaphore = RenderFinishedSemaphores[ImageIndex];
        const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = &WaitSemaphore;
        SubmitInfo.pWaitDstStageMask = &WaitStage;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = &SignalSemaphore;

        CpuStageStart = TClock::now();
        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }
        ProfilingStats.QueueSubmitCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);

        const VkSwapchainKHR SwapchainHandle = SwapchainData.GetHandle();
        VkPresentInfoKHR     PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = &SignalSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &SwapchainHandle;
        PresentInfo.pImageIndices = &ImageIndex;

        CpuStageStart = TClock::now();
        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
        ProfilingStats.PresentCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        const bool     bSwapchainNeedsRecreation =
            AcquireResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
            PresentResult == VK_SUBOPTIMAL_KHR || TargetWindow.WasFramebufferResized();

        if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR &&
            PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present Vulkan swapchain image.");
        }

        FrameContext.AdvanceFrame();

        if (bSwapchainNeedsRecreation)
        {
            TLogger::Debug("TRenderer", "Presentation requires swapchain recreation.");
            RecreateSwapchain(DebugInterface);
        }
    }

    void TRenderer::SetDebugProfileParameters(TSRProfileAssetHandle Profile,
                                              TStateId State,
                                              const TSurfaceStateParameters& Parameters,
                                              bool bKeepRuntimeOverride)
    {
        SurfaceStates.SetDebugProfileParameters(Profile, State, Parameters, bKeepRuntimeOverride);
        ++TotalHeightProfileRevision;
        if (TexelInspector)
            TexelInspector->Invalidate();
        const auto Key = std::make_pair(Profile, State);
        if (bKeepRuntimeOverride)
        {
            DebugProfileParameterOverrides[Key] = Parameters;
        }
        else
        {
            DebugProfileParameterOverrides.erase(Key);
        }
    }

    void TRenderer::ReloadSceneResources(const TScene& Scene, bool bResetStateSettings)
    {
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for GPU before reloading Scene resources.");
        }
        // Scene loading can append new MTL materials to AssetManager. Rebuild their
        // descriptors before command recording; otherwise new sections are skipped.
        if (MaterialResources.size() != Assets.GetMaterialCount())
        {
            CreateMaterialDescriptorResources();
        }
        auto PreviousRegistry = Assets.ExchangeSurfaceStateRegistry(Assets.BuildSurfaceStateRegistry(Scene));
        std::unique_ptr<TSurfaceStateSystem> Replacement;
        std::unique_ptr<TGraphicsPipeline> ReplacementDebugPipeline;
        std::unique_ptr<TTexelInspector>     ReplacementInspector;
        std::unique_ptr<TTexelGeometryPreview> ReplacementGeometryPreview;
        std::unique_ptr<TGraphicsPipeline> ReplacementGeometryPipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementLitPipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementBaseLitPipeline;
        std::unique_ptr<TTexelGeometryPreview> ReplacementMudGeometry;
        std::unique_ptr<TTexelGeometryPreview> ReplacementWaterGeometry;
        std::unique_ptr<TTexelGeometryPreview> ReplacementLavaGeometry;
        std::unique_ptr<THeightFieldSmoothing> ReplacementHeightFieldSmoothing;
        std::unique_ptr<TAccumulationOverlaySides> ReplacementOverlaySides;
        std::unique_ptr<TGraphicsPipeline> ReplacementMudTopPipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementMudSidePipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementWaterTopPipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementWaterSidePipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementLavaTopPipeline;
        std::unique_ptr<TGraphicsPipeline> ReplacementLavaSidePipeline;
        try
        {
            Replacement = std::make_unique<TSurfaceStateSystem>(Context, Assets, Scene);
            Replacement->SetRawFluxCacheEnabled(DebugSolverSettings.bRawFluxCacheEnabled);
            Replacement->SetAccumulationFeedbackEnabled(DebugSolverSettings.bAccumulationFeedbackEnabled);
            for (std::size_t Index = 0; Index < DebugSolverSettings.Enabled.size(); ++Index)
            {
                Replacement->SetDebugSolverTermEnabled(
                    static_cast<TSurfaceSolverTerm>(Index), DebugSolverSettings.Enabled[Index]);
            }
            if (!bResetStateSettings)
            {
                for (const auto& [Key, Parameters] : DebugProfileParameterOverrides)
                {
                    Replacement->SetDebugProfileParameters(Key.first, Key.second, Parameters);
                }
            }
            if (const TSurfaceStateDescriptorResources* Descriptors =
                    Replacement->GetGPUResources().GetAnyInstanceDescriptors())
            {
                ReplacementDebugPipeline = std::make_unique<TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
                ReplacementInspector = std::make_unique<TTexelInspector>(Context.GetPhysicalDevice(),
                                                                         Context.GetDevice(),
                                                                         Descriptors->GetLayout(),
                                                                         TRenderContext::MaxFramesInFlight);
                ReplacementGeometryPreview = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size());
                ReplacementGeometryPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(),
                    MainRenderPass.GetHandle(), BuildTexelGeometryPipelineConfig(MaterialDescriptorSetLayout,
                        Descriptors->GetLayout(), ReplacementGeometryPreview->GetOutputLayout()));
                ReplacementLitPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
                ReplacementBaseLitPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
                ReplacementMudGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
                ReplacementWaterGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
                ReplacementLavaGeometry = std::make_unique<TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), Scene.GetStaticMeshInstances().size(), true);
                ReplacementMudGeometry->SetOccupancyTileSize(static_cast<std::uint32_t>(OverlayOccupancyTileSize));
                ReplacementWaterGeometry->SetOccupancyTileSize(static_cast<std::uint32_t>(OverlayOccupancyTileSize));
                ReplacementLavaGeometry->SetOccupancyTileSize(static_cast<std::uint32_t>(OverlayOccupancyTileSize));
                ReplacementHeightFieldSmoothing = std::make_unique<THeightFieldSmoothing>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), ReplacementMudGeometry->GetOutputLayout(),
                    Scene.GetStaticMeshInstances().size());
                ReplacementOverlaySides = std::make_unique<TAccumulationOverlaySides>(Context.GetPhysicalDevice(),
                    Context.GetDevice(), Descriptors->GetLayout(), ReplacementMudGeometry->GetOutputLayout(),
                    Scene.GetStaticMeshInstances().size());
                ReplacementMudTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementMudGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), false, false));
                ReplacementMudSidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementMudGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), false, true));
                ReplacementWaterTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementWaterGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), true, false));
                ReplacementWaterSidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementWaterGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), true, true));
                ReplacementLavaTopPipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementLavaGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), false, false, true));
                ReplacementLavaSidePipeline = std::make_unique<TGraphicsPipeline>(Context.GetDevice(), MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout(),
                        ReplacementLavaGeometry->GetOutputLayout(), ReplacementOverlaySides->GetLayout(), false, true, true));
            }
        }
        catch (...)
        {
            Assets.ExchangeSurfaceStateRegistry(std::move(PreviousRegistry));
            throw;
        }
        SurfaceDebugPipeline.reset();
        SurfaceLitPipeline.reset();
        BaseSurfaceLitPipeline.reset();
        MudOverlayTopPipeline.reset();
        MudOverlaySidePipeline.reset();
        WaterOverlayTopPipeline.reset();
        WaterOverlaySidePipeline.reset();
        LavaOverlayTopPipeline.reset();
        LavaOverlaySidePipeline.reset();
        TexelGeometryPipeline.reset();
        TexelGeometryPreview.reset();
        OverlaySides.reset();
        HeightFieldSmoothing.reset();
        MudLayerGeometry.reset();
        WaterLayerGeometry.reset();
        LavaLayerGeometry.reset();
        TexelInspector.reset();
        InspectedTexel.reset();
        SimulationStepSerial = 0;
        TotalHeightCacheEntries.clear();
        bTotalHeightCacheHit = false;
        SurfaceStates.ReplaceSceneResources(std::move(*Replacement));
        SimulationClock.Reset();
        LastSimulationStepCount = 0;
        SurfaceDebugPipeline = std::move(ReplacementDebugPipeline);
        TexelInspector = std::move(ReplacementInspector);
        TexelGeometryPreview = std::move(ReplacementGeometryPreview);
        TexelGeometryPipeline = std::move(ReplacementGeometryPipeline);
        SurfaceLitPipeline = std::move(ReplacementLitPipeline);
        BaseSurfaceLitPipeline = std::move(ReplacementBaseLitPipeline);
        MudLayerGeometry = std::move(ReplacementMudGeometry);
        WaterLayerGeometry = std::move(ReplacementWaterGeometry);
        LavaLayerGeometry = std::move(ReplacementLavaGeometry);
        HeightFieldSmoothing = std::move(ReplacementHeightFieldSmoothing);
        OverlaySides = std::move(ReplacementOverlaySides);
        MudOverlayTopPipeline = std::move(ReplacementMudTopPipeline);
        MudOverlaySidePipeline = std::move(ReplacementMudSidePipeline);
        WaterOverlayTopPipeline = std::move(ReplacementWaterTopPipeline);
        WaterOverlaySidePipeline = std::move(ReplacementWaterSidePipeline);
        LavaOverlayTopPipeline = std::move(ReplacementLavaTopPipeline);
        LavaOverlaySidePipeline = std::move(ReplacementLavaSidePipeline);
        Assets.SetSimulationResolution(Scene.GetSimulationResolution());
        if (bResetStateSettings)
        {
            DebugProfileParameterOverrides.clear();
            DebugStateChannel = 0;
        }
        CreateTimestampQueryPool(SurfaceStates.GetSolverTimestampSlotCount());
        LastRenderGpuMilliseconds = -1.0F;
        LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = -1.0F;
        LastSolverPass2GpuMilliseconds = -1.0F;
    }

    void TRenderer::RestartSimulationState()
    {
        SurfaceStates.RestartState();
        SimulationClock.Reset();
        SimulationStepSerial = 0;
        TotalHeightCacheEntries.clear();
        bTotalHeightCacheHit = false;
        LastSimulationStepCount = 0;
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    std::uint32_t TRenderer::GetSimulationResolution() const noexcept
    {
        return Assets.GetSimulationResolution();
    }

    void TRenderer::SetSimulationResolution(TScene& Scene, std::uint32_t Resolution)
    {
        if (!IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        if (Resolution == GetSimulationResolution()) return;

        const std::uint32_t PreviousResolution = Scene.GetSimulationResolution();
        auto& Instances = Scene.GetStaticMeshInstances();
        std::vector<TSurfaceRuntimeDataHandle> PreviousHandles;
        PreviousHandles.reserve(Instances.size());
        for (const auto& Instance : Instances) PreviousHandles.push_back(Instance.GetSurfaceData());
        auto ReplacementHandles = PreviousHandles;
        try
        {
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
            {
                if (Assets.HasSurfaceData(PreviousHandles[Index]))
                    ReplacementHandles[Index] = Assets.LoadSurfaceDataAtResolution(PreviousHandles[Index], Resolution);
            }
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
                Instances[Index].SetSurfaceData(ReplacementHandles[Index]);
            Scene.SetSimulationResolution(Resolution);
            // The solver must keep referencing the persistent Scene, rather than a temporary copy.
            ReloadSceneResources(Scene, false);
        }
        catch (...)
        {
            Scene.SetSimulationResolution(PreviousResolution);
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
                Instances[Index].SetSurfaceData(PreviousHandles[Index]);
            Assets.ReleaseUnusedSurfaceData(PreviousHandles);
            throw;
        }
        Assets.ReleaseUnusedSurfaceData(ReplacementHandles);
        TLogger::Info("TRenderer", "Simulation resolution changed to " + std::to_string(Resolution) +
                      " x " + std::to_string(Resolution) + "; State reset.");
    }

    void TRenderer::RecreateSwapchain(TDebugUI& DebugInterface)
    {
        TargetWindow.WaitForNonZeroFramebuffer();
        if (TargetWindow.ShouldClose())
        {
            return;
        }

        std::uint32_t FramebufferWidth = 0;
        std::uint32_t FramebufferHeight = 0;
        TargetWindow.GetFramebufferSize(FramebufferWidth, FramebufferHeight);

        TLogger::Info("TRenderer",
                     "Recreating swapchain for framebuffer " + std::to_string(FramebufferWidth) + "x" +
                         std::to_string(FramebufferHeight) + ".");

        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for Vulkan device before swapchain recreation.");
        }

        DestroyRenderFinishedSemaphores();

        // 프레임버퍼가 스왑체인 이미지 뷰와 깊이 이미지 뷰를 참조하므로,
        // 이미지 뷰를 다시 만들기 전에 프레임버퍼를 먼저 해제한다.
        MainFramebuffers.Reset();
        DepthImageViews.clear();
        DepthImages.clear();

        const VkFormat PreviousColorFormat = SwapchainData.GetImageFormat();
        SwapchainData.Recreate(Context, TargetWindow);
        CreateRenderFinishedSemaphores();

        if (PreviousColorFormat != SwapchainData.GetImageFormat())
        {
            throw std::runtime_error(
                "TSwapchain color format changed during resize. TRenderPass/Pipeline recreation is required.");
        }

        DepthImages = CreateDepthImages(Context.GetPhysicalDevice(), Context.GetDevice(),
            SwapchainData.GetExtent(), DepthFormat, SwapchainData.GetImageViews().size());
        DepthImageViews = CreateDepthImageViews(Context.GetDevice(), DepthImages, DepthFormat);
        MainFramebuffers.Recreate(MainRenderPass.GetHandle(),
                                  SwapchainData.GetExtent(),
                                  SwapchainData.GetImageViews(),
                                  GetDepthImageViewHandles(DepthImageViews));

        TargetWindow.ResetFramebufferResized();
        DebugInterface.OnSwapchainRecreated(Context, *this);

        const VkExtent2D NewExtent = SwapchainData.GetExtent();
        TLogger::Info("TRenderer",
                     "TSwapchain recreation complete: " + std::to_string(NewExtent.width) + "x" +
                         std::to_string(NewExtent.height) + ".");
    }

    const TSwapchain& TRenderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
    }

    VkRenderPass TRenderer::GetRenderPassHandle() const noexcept
    {
        return MainRenderPass.GetHandle();
    }

    const TSurfaceGPUResourceManager& TRenderer::GetSurfaceGPUResources() const noexcept
    {
        return SurfaceStates.GetGPUResources();
    }

    float TRenderer::GetLastSolverGpuMilliseconds() const noexcept
    {
        return LastSolverGpuMilliseconds;
    }

    float TRenderer::GetLastRenderGpuMilliseconds() const noexcept
    {
        return LastRenderGpuMilliseconds;
    }

    float TRenderer::GetLastSolverPass1GpuMilliseconds() const noexcept
    {
        return LastSolverPass1GpuMilliseconds;
    }

    float TRenderer::GetLastSolverPass2GpuMilliseconds() const noexcept
    {
        return LastSolverPass2GpuMilliseconds;
    }

    TRenderViewMode TRenderer::GetRenderViewMode() const noexcept
    {
        return ViewMode;
    }

    void TRenderer::SetWireframeLineWidth(float Width) noexcept
    {
        if (std::isfinite(Width))
        {
            WireframeLineWidth = std::clamp(Width, WireframeLineWidthMin, WireframeLineWidthMax);
        }
    }

    bool TRenderer::IsWorldGridVisible() const noexcept
    {
        return bWorldGridVisible;
    }

    void TRenderer::SetWorldGridVisible(bool bVisible) noexcept
    {
        bWorldGridVisible = bVisible;
    }

    bool TRenderer::IsWorldAxisVisible() const noexcept
    {
        return bWorldAxisVisible;
    }

    void TRenderer::SetWorldAxisVisible(bool bVisible) noexcept
    {
        bWorldAxisVisible = bVisible;
    }

    void TRenderer::SetRenderViewMode(TRenderViewMode Mode)
    {
        if (ViewMode == Mode)
        {
            return;
        }

        ViewMode = Mode;
        bTotalHeightCacheHit = false;
        TLogger::Info("TRenderer", std::string("Render view mode changed to ") + GetRenderViewModeName(ViewMode) + ".");
    }

    std::uint32_t TRenderer::GetDebugStateChannel() const noexcept
    {
        return DebugStateChannel;
    }

    void TRenderer::SetDebugStateChannel(std::uint32_t Channel)
    {
        if (Channel >= Assets.GetSurfaceStateRegistry().GetStateCount())
        {
            throw std::out_of_range("Debug State channel is outside the registered State range.");
        }
        if (DebugStateChannel == Channel)
        {
            return;
        }
        DebugStateChannel = Channel;
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    bool TRenderer::IsStateHeatmapReliefShadingEnabled() const noexcept
    {
        return bStateHeatmapReliefShadingEnabled;
    }

    void TRenderer::SetSurfaceDebugDisplaySettings(const TSurfaceDebugDisplaySettings& Settings)
    {
        const auto Positive = [](float Value) { return std::isfinite(Value) && Value >= 1e-8F && Value <= 1e6F; };
        if (!Positive(Settings.RawStateMax) || !Positive(Settings.HeightMax) ||
            !Positive(Settings.DisplacementScale) ||
            Settings.AccumulationComponent > 3 ||
            Settings.HeightGridMode > 2 || Settings.HeightGridBlockSize < 1 || Settings.HeightGridBlockSize > 256)
            throw std::invalid_argument("Invalid Surface debug range, display scale or component.");
        SurfaceDebugSettings = Settings;
    }

    TDemoSurfaceStateBindings TRenderer::GetDemoSurfaceStateBindings() const
    {
        return ResolveDemoSurfaceStates(Assets.GetSurfaceStateRegistry());
    }

    void TRenderer::SetDemoSurfaceEffectSettings(const TDemoSurfaceEffectSettings& Settings)
    {
        const auto Roughness = [](float V) { return std::isfinite(V) && V >= 0.05F && V <= 1.0F; };
        const auto Unit = [](float V) { return std::isfinite(V) && V >= 0.0F && V <= 1.0F; };
        const auto Color = [&](const glm::vec3& V) { return Unit(V.r) && Unit(V.g) && Unit(V.b); };
        if (!Roughness(Settings.DryRoughness) || !Roughness(Settings.WetRoughness) ||
            !Roughness(Settings.MudRoughness) || !Roughness(Settings.WaterFilmRoughness) ||
            !Unit(Settings.WetnessStrength) || !Unit(Settings.WetnessSpecularStrength) ||
            !Unit(Settings.WaterFilmOpacity) || !Color(Settings.WetnessTint) || !Color(Settings.WaterFilmTint))
            throw std::invalid_argument("Invalid demo surface effect settings.");
        DemoEffects = Settings;
    }

    void TRenderer::SetSceneLitHeightDisplayScale(TScene& Scene, float Scale)
    {
        const bool bChanged = Scene.GetLitHeightDisplayScale() != Scale;
        Scene.SetLitHeightDisplayScale(Scale);
        if (bChanged && TexelInspector)
        {
            TexelInspector->Invalidate();
        }
    }

    bool TRenderer::InspectTexel(const TScene& Scene, std::size_t InstanceIndex, std::uint32_t Triangle, glm::vec2 UV)
    {
        ClearInspectedTexel();
        const auto& Instances = Scene.GetStaticMeshInstances();
        if (!TexelInspector || InstanceIndex >= Instances.size() || !std::isfinite(UV.x) || !std::isfinite(UV.y) ||
            UV.x < 0 || UV.x > 1 || UV.y < 0 || UV.y > 1)
            return false;
        const auto& Instance = Instances[InstanceIndex];
        if (!Assets.HasSurfaceData(Instance.GetSurfaceData()) ||
            !SurfaceStates.GetGPUResources().GetInstanceDescriptors(InstanceIndex))
            return false;
        const auto& Triangles = Assets.GetMesh(Instance.GetMesh()).GetTriangles();
        if (Triangle >= Triangles.size())
            return false;
        const auto& Geometry = *Assets.GetSurfaceData(Instance.GetSurfaceData()).GetSharedGeometry();
        const auto  Surface = Triangles[Triangle].Surface;
        if (Surface >= Geometry.GetSurfaces().size())
            return false;
        const auto& Range = Geometry.GetSurface(Surface);
        const auto X = std::min(static_cast<std::uint32_t>(UV.x * Range.Resolution.Width), Range.Resolution.Width - 1U);
        const auto Y =
            std::min(static_cast<std::uint32_t>(UV.y * Range.Resolution.Height), Range.Resolution.Height - 1U);
        const auto  Texel = Range.FirstTexel + Y * Range.Resolution.Width + X;
        const auto  ProfileIndex = Geometry.GetProfileIndex(Texel);
        const auto& Profiles = Assets.GetSurfaceProfileTable(Instance.GetSurfaceData());
        std::string Profile = "Unassigned";
        if (ProfileIndex < Profiles.size())
            Profile = Assets.GetSRProfile(Profiles[ProfileIndex]).GetName();
        InspectedTexel = TSurfaceTexelSelection{InstanceIndex, Surface, Texel, Triangle, {X, Y}, std::move(Profile)};
        return true;
    }

    void TRenderer::ClearInspectedTexel() noexcept
    {
        InspectedTexel.reset();
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    const std::optional<TSurfaceTexelSnapshot>& TRenderer::GetTexelSnapshot() const noexcept
    {
        static const std::optional<TSurfaceTexelSnapshot> Empty;
        return TexelInspector ? TexelInspector->GetSnapshot() : Empty;
    }

    void TRenderer::SetStateHeatmapReliefShadingEnabled(bool bEnabled)
    {
        if (bStateHeatmapReliefShadingEnabled == bEnabled)
        {
            return;
        }
        bStateHeatmapReliefShadingEnabled = bEnabled;
    }

    TSolverTransferWeightView TRenderer::GetSolverTransferWeightView() const noexcept
    {
        return SolverTransferWeightView;
    }

    void TRenderer::SetSolverTransferWeightView(TSolverTransferWeightView View)
    {
        if (SolverTransferWeightView == View)
        {
            return;
        }
        SolverTransferWeightView = View;
    }

    std::uint32_t TRenderer::GetTexelGridBlockSize() const noexcept
    {
        return TexelGridBlockSize;
    }

    void TRenderer::SetTexelGridBlockSize(std::uint32_t Size)
    {
        if (Size != 8U && Size != 16U)
        {
            throw std::invalid_argument("Texel grid block size must be 8 or 16.");
        }
        if (TexelGridBlockSize == Size)
            return;
        TexelGridBlockSize = Size;
    }

    float TRenderer::GetTexelAreaReference() const noexcept
    {
        return TexelAreaReference;
    }

    void TRenderer::SetTexelAreaReference(float Area)
    {
        if (!std::isfinite(Area) || Area < 1.0e-12F || Area > 1.0e12F)
        {
            throw std::invalid_argument("Texel reference area must be finite and in [1e-12, 1e12].");
        }
        if (TexelAreaReference == Area)
            return;
        TexelAreaReference = Area;
    }

    float TRenderer::GetDebugViewParameter() const noexcept
    {
        switch (ViewMode)
        {
            case TRenderViewMode::SurfaceTexelGrid:
                return static_cast<float>(TexelGridBlockSize);
            case TRenderViewMode::SurfaceTexelArea:
                return TexelAreaReference;
            default:
                return static_cast<float>(SolverTransferWeightView);
        }
    }

    bool TRenderer::IsDebugGeometryDriveEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(TSurfaceSolverTerm::GeometryDrive);
    }

    void TRenderer::SetDebugGeometryDriveEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    bool TRenderer::IsDebugSolverTermEnabled(TSurfaceSolverTerm Term) const noexcept
    {
        return DebugSolverSettings.IsEnabled(Term);
    }

    void TRenderer::SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled)
    {
        DebugSolverSettings.SetEnabled(Term, bEnabled);
        SurfaceStates.SetDebugSolverTermEnabled(Term, bEnabled);
    }

    bool TRenderer::IsRawFluxCacheEnabled() const noexcept
    {
        return DebugSolverSettings.bRawFluxCacheEnabled;
    }

    void TRenderer::SetRawFluxCacheEnabled(bool bEnabled)
    {
        if (IsRawFluxCacheEnabled() == bEnabled) return;
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for GPU before changing RawFlux cache mode.");
        }
        DebugSolverSettings.bRawFluxCacheEnabled = bEnabled;
        SurfaceStates.SetRawFluxCacheEnabled(bEnabled);
        bTimestampQueriesSubmitted.fill(false);
        SolverTimestampStepsSubmitted.fill(0);
        OverlayTimestampLayersSubmitted.fill(0);
        LastRenderGpuMilliseconds = LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = LastSolverPass2GpuMilliseconds = -1.0F;
    }

    void TRenderer::SetOverlayOccupancyTileSize(TOverlayOccupancyTileSize Size)
    {
        if (Size != TOverlayOccupancyTileSize::Tile16 && Size != TOverlayOccupancyTileSize::Tile32)
            throw std::invalid_argument("Overlay occupancy tile size must be 16 or 32.");
        if (OverlayOccupancyTileSize == Size) return;
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for GPU before changing overlay occupancy tile size.");
        OverlayOccupancyTileSize = Size;
        const std::uint32_t Tile = static_cast<std::uint32_t>(Size);
        if (MudLayerGeometry) MudLayerGeometry->SetOccupancyTileSize(Tile);
        if (WaterLayerGeometry) WaterLayerGeometry->SetOccupancyTileSize(Tile);
        if (LavaLayerGeometry) LavaLayerGeometry->SetOccupancyTileSize(Tile);
    }

    bool TRenderer::IsAccumulationFeedbackEnabled() const noexcept
    {
        return DebugSolverSettings.bAccumulationFeedbackEnabled;
    }

    void TRenderer::SetAccumulationFeedbackEnabled(bool bEnabled)
    {
        DebugSolverSettings.bAccumulationFeedbackEnabled = bEnabled;
        SurfaceStates.SetAccumulationFeedbackEnabled(bEnabled);
    }

    bool TRenderer::IsDebugNormalWeightEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(TSurfaceSolverTerm::NormalWeight);
    }

    void TRenderer::SetDebugNormalWeightEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::NormalWeight, bEnabled);
    }

    bool TRenderer::GetFlipNormalY() const noexcept
    {
        return bFlipNormalY;
    }

    void TRenderer::SetFlipNormalY(bool bEnabled)
    {
        if (bFlipNormalY == bEnabled)
        {
            return;
        }

        bFlipNormalY = bEnabled;
        TLogger::Info("TRenderer", std::string("Normal-map Y flip ") + (bFlipNormalY ? "enabled." : "disabled."));
    }

    float TRenderer::GetNormalStrength() const noexcept
    {
        return NormalStrength;
    }

    void TRenderer::SetNormalStrength(float Strength)
    {
        NormalStrength = std::clamp(Strength, 0.0F, 4.0F);
    }

    float TRenderer::GetAmbientLight() const noexcept
    {
        return AmbientLight;
    }

    void TRenderer::SetAmbientLight(float Intensity)
    {
        AmbientLight = std::clamp(Intensity, 0.0F, 1.0F);
    }

    VkDescriptorSetLayout TRenderer::CreateMaterialDescriptorSetLayout(VkDevice Device)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> Bindings{};

        Bindings[0].binding = 0;
        Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Bindings[0].descriptorCount = 1;
        Bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        Bindings[1].binding = 1;
        Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Bindings[1].descriptorCount = 1;
        Bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        Bindings[2].binding = 2;
        Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        Bindings[2].descriptorCount = 1;
        // Meso Offset은 정점 셰이더도 렌더 모드 값을 읽어 변위 여부를 결정한다.
        Bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo Info{};
        Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        Info.bindingCount = static_cast<std::uint32_t>(Bindings.size());
        Info.pBindings = Bindings.data();

        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create static mesh material descriptor set layout.");
        }
        return Layout;
    }

    void TRenderer::CreateRenderFinishedSemaphores()
    {
        if (!RenderFinishedSemaphores.empty())
        {
            throw std::logic_error("Render-finished semaphores already exist for the current swapchain.");
        }

        RenderFinishedSemaphores.resize(SwapchainData.GetImages().size(), VK_NULL_HANDLE);
        VkSemaphoreCreateInfo SemaphoreInfo{};
        SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (VkSemaphore& Semaphore : RenderFinishedSemaphores)
        {
            if (vkCreateSemaphore(Context.GetDevice(), &SemaphoreInfo, nullptr, &Semaphore) != VK_SUCCESS)
            {
                DestroyRenderFinishedSemaphores();
                throw std::runtime_error("Failed to create swapchain image render-finished semaphore.");
            }
        }
    }

    void TRenderer::DestroyRenderFinishedSemaphores() noexcept
    {
        for (VkSemaphore Semaphore : RenderFinishedSemaphores)
        {
            if (Semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(Context.GetDevice(), Semaphore, nullptr);
            }
        }
        RenderFinishedSemaphores.clear();
    }

    void TRenderer::CreateTimestampQueryPool(std::size_t SolverInstanceCount)
    {
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(Context.GetDevice(), TimestampQueryPool, nullptr);
            TimestampQueryPool = VK_NULL_HANDLE;
        }
        if (TimestampValidBits == 0U)
        {
            return;
        }
        if (SolverInstanceCount > (std::numeric_limits<std::uint32_t>::max() /
                                   TRenderContext::MaxFramesInFlight - FixedTimestampQueryCount) /
                                      (8U * MaxSimulationStepsPerFrame + 3U * OverlayTimestampQueriesPerLayer))
        {
            TLogger::Warning("TRenderer", "GPU timing query count exceeds the supported range.");
            return;
        }

        SolverTimestampSlotCount = static_cast<std::uint32_t>(SolverInstanceCount);
        TimestampQueriesPerFrame = FixedTimestampQueryCount + SolverTimestampSlotCount *
            (8U * MaxSimulationStepsPerFrame + 3U * OverlayTimestampQueriesPerLayer);
        VkQueryPoolCreateInfo QueryPoolInfo{};
        QueryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        QueryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        QueryPoolInfo.queryCount =
            static_cast<std::uint32_t>(TRenderContext::MaxFramesInFlight) * TimestampQueriesPerFrame;
        if (vkCreateQueryPool(Context.GetDevice(), &QueryPoolInfo, nullptr, &TimestampQueryPool) != VK_SUCCESS)
        {
            TimestampQueryPool = VK_NULL_HANDLE;
            LastRenderGpuMilliseconds = -1.0F;
            LastSolverGpuMilliseconds = -1.0F;
            LastSolverPass1GpuMilliseconds = -1.0F;
            LastSolverPass2GpuMilliseconds = -1.0F;
            TLogger::Warning("TRenderer", "GPU timestamp queries unavailable; profiling timings will be hidden.");
        }
        bTimestampQueriesSubmitted.fill(false);
        SolverTimestampStepsSubmitted.fill(0);
        OverlayTimestampLayersSubmitted.fill(0);
    }

    void TRenderer::CreateMaterialDescriptorResources()
    {
        const std::size_t MaterialCount = Assets.GetMaterialCount();
        if (MaterialCount == 0)
        {
            TLogger::Warning("TRenderer",
                             "No materials are registered; material descriptor resources were not created.");
            return;
        }

        TLogger::Debug("TRenderer",
                       "Creating descriptor resources for " + std::to_string(MaterialCount) + " material(s).");

        std::array<VkDescriptorPoolSize, 2> PoolSizes{};
        PoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        PoolSizes[0].descriptorCount =
            static_cast<std::uint32_t>(MaterialCount * TRenderContext::MaxFramesInFlight * 2U);
        PoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        PoolSizes[1].descriptorCount = static_cast<std::uint32_t>(MaterialCount * TRenderContext::MaxFramesInFlight);

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = static_cast<std::uint32_t>(MaterialCount * TRenderContext::MaxFramesInFlight);

        VkDescriptorPool NewPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(Context.GetDevice(), &PoolInfo, nullptr, &NewPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create material descriptor pool.");
        }

        std::vector<TMaterialRenderResource> NewResources;
        try
        {
            NewResources.resize(MaterialCount);
            std::vector<VkDescriptorSetLayout> Layouts(MaterialCount * TRenderContext::MaxFramesInFlight,
                                                       MaterialDescriptorSetLayout);
            std::vector<VkDescriptorSet>       Sets(MaterialCount * TRenderContext::MaxFramesInFlight, VK_NULL_HANDLE);

            VkDescriptorSetAllocateInfo AllocateInfo{};
            AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            AllocateInfo.descriptorPool = NewPool;
            AllocateInfo.descriptorSetCount =
                static_cast<std::uint32_t>(MaterialCount * TRenderContext::MaxFramesInFlight);
            AllocateInfo.pSetLayouts = Layouts.data();

            if (vkAllocateDescriptorSets(Context.GetDevice(), &AllocateInfo, Sets.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate material descriptor sets.");
            }

            for (std::size_t Index = 0; Index < MaterialCount; ++Index)
            {
                const TMaterialAssetHandle Handle = static_cast<TMaterialAssetHandle>(Index);
                const TMaterialAsset&      Material = Assets.GetMaterial(Handle);
                const TextureAsset&        BaseTexture = Assets.GetTexture(Material.GetBaseColorTexture());
                const TextureAsset&        NormalTexture = Assets.GetTexture(Material.GetNormalTexture());

                for (std::uint32_t Frame = 0; Frame < TRenderContext::MaxFramesInFlight; ++Frame)
                {
                    const std::size_t SetIndex = Index * TRenderContext::MaxFramesInFlight + Frame;
                    NewResources[Index].UniformBuffers[Frame] = std::make_unique<TGPUBuffer>(
                        Context.GetPhysicalDevice(),
                        Context.GetDevice(),
                        MaterialUniformStride * MaterialViewportCapacity,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                    NewResources[Index].DescriptorSets[Frame] = Sets[SetIndex];

                    VkDescriptorImageInfo BaseImage{};
                    BaseImage.sampler = BaseTexture.GetSampler();
                    BaseImage.imageView = BaseTexture.GetImageView();
                    BaseImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    VkDescriptorImageInfo NormalImage{};
                    NormalImage.sampler = NormalTexture.GetSampler();
                    NormalImage.imageView = NormalTexture.GetImageView();
                    NormalImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    VkDescriptorBufferInfo MaterialBuffer{};
                    MaterialBuffer.buffer = NewResources[Index].UniformBuffers[Frame]->GetHandle();
                    MaterialBuffer.offset = 0;
                    MaterialBuffer.range = sizeof(TMaterialUniform);

                    std::array<VkWriteDescriptorSet, 3> Writes{};
                    for (VkWriteDescriptorSet& Write : Writes)
                    {
                        Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        Write.dstSet = Sets[SetIndex];
                        Write.descriptorCount = 1;
                    }
                    Writes[0].dstBinding = 0;
                    Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Writes[0].pImageInfo = &BaseImage;
                    Writes[1].dstBinding = 1;
                    Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Writes[1].pImageInfo = &NormalImage;
                    Writes[2].dstBinding = 2;
                    Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    Writes[2].pBufferInfo = &MaterialBuffer;

                    vkUpdateDescriptorSets(
                        Context.GetDevice(), static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
                }
            }
        }
        catch (...)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), NewPool, nullptr);
            throw;
        }

        if (MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), MaterialDescriptorPool, nullptr);
        }
        MaterialDescriptorPool = NewPool;
        MaterialResources = std::move(NewResources);

        TLogger::Info("TRenderer",
                      "Material descriptor sets created: " + std::to_string(MaterialResources.size()) + ".");
    }

    void TRenderer::UploadMaterialUniforms(std::uint32_t Frame,
                                           const TScene& SceneData,
                                           const TDebugUI& DebugInterface,
                                           float LitHeightDisplayScale)
    {
        const auto Bindings = GetDemoSurfaceStateBindings();
        const std::size_t MaterialCount = std::min(Assets.GetMaterialCount(), MaterialResources.size());
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            if (!MaterialResources[Index].UniformBuffers[Frame])
            {
                continue;
            }

            const TMaterialAsset&  Material = Assets.GetMaterial(static_cast<TMaterialAssetHandle>(Index));
            TMaterialUniform Uniform{
                Material.GetBaseColor(),
                ViewMode == TRenderViewMode::Wireframe && bWireframeUniformWhite
                    ? RenderModeWireframeUniformWhite
                    : static_cast<std::uint32_t>(ViewMode),
                bFlipNormalY ? 1U : 0U,
                NormalStrength,
                AmbientLight,
                DebugStateChannel,
                static_cast<std::uint32_t>(Assets.GetSurfaceStateRegistry().GetStateCount()),
                GetDebugViewParameter(),
                bStateHeatmapReliefShadingEnabled ? 1.0F : 0.0F,
                {SurfaceDebugSettings.RawStateMax,
                 SurfaceDebugSettings.HeightMax,
                 LitHeightDisplayScale,
                 SurfaceDebugSettings.DisplacementScale},
                {SurfaceDebugSettings.bRawState ? 1U : 0U, SurfaceDebugSettings.AccumulationComponent,
                 SurfaceDebugSettings.HeightGridMode, SurfaceDebugSettings.HeightGridBlockSize},
                {Bindings.Wetness, Bindings.Mud, Bindings.WaterFilm, DemoEffects.bEnabled ? 1U : 0U},
                {DemoEffects.DryRoughness, DemoEffects.WetRoughness, DemoEffects.MudRoughness, LitHeightDisplayScale},
                {DemoEffects.WetnessStrength, DemoEffects.WetnessSpecularStrength,
                 DemoEffects.WaterFilmOpacity, DemoEffects.WaterFilmRoughness},
                {DemoEffects.WetnessTint, 1.0F},
                {DemoEffects.WaterFilmTint, 1.0F},
                {Bindings.Lava, 0U, 0U, 0U},
                glm::vec4(0.0F)};
            for (std::size_t ViewportIndex = 0; ViewportIndex < DebugInterface.GetViewportCount(); ++ViewportIndex)
            {
                Uniform.CameraPosition = glm::vec4(
                    DebugInterface.GetViewportCamera(SceneData, ViewportIndex).GetPosition(), 1.0F);
                MaterialResources[Index].UniformBuffers[Frame]->Upload(
                    &Uniform, sizeof(Uniform), MaterialUniformStride * ViewportIndex);
            }
        }
    }

    void TRenderer::RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                       std::uint32_t   ImageIndex,
                                       const TScene&    SceneData,
                                       const TDebugUI&  DebugInterface,
                                       std::span<const float> SimulationSteps)
    {
        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin Vulkan command buffer.");
        }

        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        const float LitHeightDisplayScale = SceneData.GetLitHeightDisplayScale();
        UploadMaterialUniforms(FrameIndex, SceneData, DebugInterface, LitHeightDisplayScale);
        const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
        const std::uint32_t OverlayQueryBase = QueryBase + FixedTimestampQueryCount +
            static_cast<std::uint32_t>(SimulationSteps.size()) * SolverTimestampSlotCount * 8U;
        OverlayTimestampLayersSubmitted[FrameIndex] = 0;
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdResetQueryPool(CommandBuffer, TimestampQueryPool, QueryBase, TimestampQueriesPerFrame);
        }
        for (std::size_t Step = 0; Step < SimulationSteps.size(); ++Step)
        {
            SurfaceStates.RecordStep(CommandBuffer, SimulationSteps[Step], TimestampQueryPool,
                QueryBase + FixedTimestampQueryCount + static_cast<std::uint32_t>(Step) * SolverTimestampSlotCount * 8U);
        }

        SimulationStepSerial += SimulationSteps.size();
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 4U);
        if (TexelInspector && InspectedTexel && DebugStateChannel < Assets.GetSurfaceStateRegistry().GetStateCount())
        {
            const auto& Resources = SurfaceStates.GetGPUResources();
            if (const auto* Descriptors = Resources.GetInstanceDescriptors(InspectedTexel->Instance))
                TexelInspector->Record(
                    CommandBuffer,
                    FrameIndex,
                    *Descriptors,
                    *InspectedTexel,
                    DebugStateChannel,
                    static_cast<std::uint32_t>(Resources.GetInstanceChannelCount(InspectedTexel->Instance)),
                    LitHeightDisplayScale,
                    SceneData.GetStaticMeshInstances()[InspectedTexel->Instance].GetTransform().GetMatrix(),
                    Resources.IsCurrentStateAB(InspectedTexel->Instance),
                    SimulationStepSerial);
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 5U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase);
        }

        const bool bTexelGeometry = UsesTexelGeometry(ViewMode) && TexelGeometryPreview && TexelGeometryPipeline;
        const bool bTotalHeight = bTexelGeometry && ViewMode == TRenderViewMode::TotalSimulationHeight;
        bool bAnyTotalHeightGeometry = false;
        bool bAllTotalHeightCachesHit = true;
        bTotalHeightCacheHit = false;
        if (bTotalHeight)
            TotalHeightCacheEntries.resize(SceneData.GetStaticMeshInstances().size());
        const auto DemoBindings = GetDemoSurfaceStateBindings();
        const bool bSurfaceLit = ViewMode == TRenderViewMode::Lit && DemoEffects.bEnabled && SurfaceLitPipeline;
        const bool bOverlayRendering = bSurfaceLit && BaseSurfaceLitPipeline && OverlaySides && HeightFieldSmoothing &&
            MudLayerGeometry && WaterLayerGeometry && LavaLayerGeometry &&
            MudOverlayTopPipeline && MudOverlaySidePipeline && WaterOverlayTopPipeline &&
            WaterOverlaySidePipeline && LavaOverlayTopPipeline && LavaOverlaySidePipeline;
        std::vector<std::array<bool, 3>> OverlayActive(SceneData.GetStaticMeshInstances().size(), {false, false, false});
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 12U);
        if (bTexelGeometry || bOverlayRendering)
        {
            const auto& Resources = SurfaceStates.GetGPUResources();
            const bool bAccumulation = ViewMode == TRenderViewMode::SurfaceAccumulation ||
                                       ViewMode == TRenderViewMode::SurfaceFinalGeometry || bTotalHeight;
            for (std::size_t Instance = 0; Instance < SceneData.GetStaticMeshInstances().size(); ++Instance)
            {
                const auto* Shared = Resources.GetInstanceSharedGeometry(Instance);
                const auto* Descriptors = Resources.GetInstanceDescriptors(Instance);
                if (!Shared || !Descriptors || !Shared->GetTexelMeshIndexBuffer()) continue;
                const auto& MeshInstance = SceneData.GetStaticMeshInstances()[Instance];
                const auto ChannelCount = static_cast<std::uint32_t>(Resources.GetInstanceChannelCount(Instance));
                const auto Model = MeshInstance.GetTransform().GetMatrix();
                const bool bStateAB = Resources.IsCurrentStateAB(Instance);
                if (bTotalHeight)
                {
                    bAnyTotalHeightGeometry = true;
                    auto& Cache = TotalHeightCacheEntries[Instance];
                    bool bSameModelLinear = true;
                    for (int Column = 0; Column < 3; ++Column)
                        bSameModelLinear &= glm::all(glm::equal(glm::vec3(Cache.Model[Column]),
                                                                glm::vec3(Model[Column])));
                    const bool bHit = Cache.bValid && Cache.StepSerial == SimulationStepSerial &&
                        Cache.ProfileRevision == TotalHeightProfileRevision && bSameModelLinear;
                    bAllTotalHeightCachesHit &= bHit;
                    if (!bHit)
                    {
                        SurfaceStates.RecordCurrentAccumulationHeight(CommandBuffer, Instance);
                        Cache = {Model, SimulationStepSerial, TotalHeightProfileRevision, true};
                    }
                }
                if (bTexelGeometry)
                    TexelGeometryPreview->Record(CommandBuffer, Instance, *Descriptors,
                        static_cast<std::uint32_t>(Shared->GetTexelCount()), DebugStateChannel, ChannelCount,
                        LitHeightDisplayScale,
                        bAccumulation ? SurfaceDebugSettings.DisplacementScale : 1.0F,
                        Model, bStateAB, bAccumulation, VK_NULL_HANDLE, 0U, bTotalHeight);
                if (!bOverlayRendering) continue;
                const bool bMud = DemoEffects.bMudDisplacement && DemoBindings.Mud != InvalidStateId &&
                    UsesAccumulationGeometry(MeshInstance, Assets, Shared, "mud");
                const bool bWater = DemoEffects.bWaterFilmDisplacement && DemoBindings.WaterFilm != InvalidStateId &&
                    UsesAccumulationGeometry(MeshInstance, Assets, Shared, "waterfilm");
                const bool bLava = DemoEffects.bLavaDisplacement && DemoBindings.Lava != InvalidStateId &&
                    UsesAccumulationGeometry(MeshInstance, Assets, Shared, "lava");
                OverlayActive[Instance] = {bMud, bWater, bLava};
                const auto RecordLayer = [&](TTexelGeometryPreview& Preview, TStateId Channel,
                                             bool bSmoothLayer)
                {
                    const std::uint32_t LayerQuery = OverlayQueryBase +
                        OverlayTimestampLayersSubmitted[FrameIndex] * OverlayTimestampQueriesPerLayer;
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                        vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                            TimestampQueryPool, LayerQuery);
                    Preview.Record(CommandBuffer, Instance, *Descriptors,
                        static_cast<std::uint32_t>(Shared->GetTexelCount()), Channel, ChannelCount,
                        LitHeightDisplayScale, 1.0F, Model, bStateAB, true,
                        TimestampQueryPool, LayerQuery + 12U);
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                    {
                        vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                            TimestampQueryPool, LayerQuery + 1U);
                        vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                            TimestampQueryPool, LayerQuery + 2U);
                    }
                    VkDescriptorSet GeometrySet = Preview.GetOutputSet(Instance);
                    if (DemoEffects.bHeightFieldSmoothing || bSmoothLayer)
                    {
                        HeightFieldSmoothing->Record(CommandBuffer, Instance, Channel, ChannelCount,
                            static_cast<std::uint32_t>(Shared->GetTexelCount()), *Descriptors,
                            GeometrySet, Preview.GetOutputBuffer(Instance), bStateAB);
                        GeometrySet = HeightFieldSmoothing->GetOutputSet(Instance, Channel);
                    }
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                    {
                        vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                            TimestampQueryPool, LayerQuery + 3U);
                        vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                            TimestampQueryPool, LayerQuery + 4U);
                    }
                    OverlaySides->Record(CommandBuffer, Instance, Channel, ChannelCount, *Shared, *Descriptors,
                        GeometrySet, bStateAB, FrameIndex, TimestampQueryPool, LayerQuery + 4U,
                        static_cast<std::uint32_t>(OverlayOccupancyTileSize), bSmoothLayer);
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                        ++OverlayTimestampLayersSubmitted[FrameIndex];
                };
                if (bMud) RecordLayer(*MudLayerGeometry, DemoBindings.Mud, DemoEffects.bMudSmoothing);
                if (bWater) RecordLayer(*WaterLayerGeometry, DemoBindings.WaterFilm,
                                        DemoEffects.bWaterFilmSmoothing);
                if (bLava) RecordLayer(*LavaLayerGeometry, DemoBindings.Lava, DemoEffects.bLavaSmoothing);
            }
        }
        bTotalHeightCacheHit = bAnyTotalHeightGeometry && bAllTotalHeightCachesHit;
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 13U);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 1U);

        std::array<VkClearValue, 2> ClearValues{};
        ClearValues[0].color = {{0.03F, 0.04F, 0.06F, 1.0F}};
        ClearValues[1].depthStencil = {1.0F, 0};

        VkRenderPassBeginInfo RenderPassInfo{};
        RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.renderPass = MainRenderPass.GetHandle();
        RenderPassInfo.framebuffer = MainFramebuffers.Get(ImageIndex);
        RenderPassInfo.renderArea.offset = {0, 0};
        RenderPassInfo.renderArea.extent = SwapchainData.GetExtent();
        RenderPassInfo.clearValueCount = static_cast<std::uint32_t>(ClearValues.size());
        RenderPassInfo.pClearValues = ClearValues.data();

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 2U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                TimestampQueryPool, QueryBase + 15U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                TimestampQueryPool, QueryBase + 17U);
        }
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                TimestampQueryPool, QueryBase + 16U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                TimestampQueryPool, QueryBase + 18U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 14U);
        }
        const bool bShowSurfaceDebug = ViewMode >= TRenderViewMode::SurfaceStateHeatmap;
        const bool bCanShowSurfaceDebug = bShowSurfaceDebug && SurfaceDebugPipeline != nullptr;
        const bool bWireframe = ViewMode == TRenderViewMode::Wireframe;
        vkCmdBindPipeline(CommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bTexelGeometry ? TexelGeometryPipeline->GetHandle() :
                          bCanShowSurfaceDebug ? SurfaceDebugPipeline->GetHandle()
                                               : (bWireframe ? WireframePipeline.GetHandle()
                                                             : StaticMeshPipeline.GetHandle()));
        if (bWireframe)
        {
            // Dynamic line width must be set before drawing with the wireframe pipeline.
            // Without wideLines support, Vulkan only permits the default width of 1.0.
            vkCmdSetLineWidth(CommandBuffer, bSupportsWireframeLineWidth ? WireframeLineWidth : 1.0F);
        }

        const VkExtent2D Extent = SwapchainData.GetExtent();
        for (std::size_t ViewportIndex = 0; ViewportIndex < DebugInterface.GetViewportCount(); ++ViewportIndex)
        {
            const glm::vec4 NormalizedViewport = DebugInterface.GetViewportRectNormalized(ViewportIndex);
            if (NormalizedViewport.z <= 0.0F || NormalizedViewport.w <= 0.0F) continue;
            const TCamera& ViewCamera = DebugInterface.GetViewportCamera(SceneData, ViewportIndex);
            const std::uint32_t MaterialUniformOffset =
                static_cast<std::uint32_t>(MaterialUniformStride * ViewportIndex);
            const std::uint32_t ViewportX = std::min(
                static_cast<std::uint32_t>(std::max(NormalizedViewport.x, 0.0F) * static_cast<float>(Extent.width)),
                Extent.width - 1U);
            const std::uint32_t ViewportY = std::min(
                static_cast<std::uint32_t>(std::max(NormalizedViewport.y, 0.0F) * static_cast<float>(Extent.height)),
                Extent.height - 1U);
            const std::uint32_t ViewportWidth = std::clamp(
                static_cast<std::uint32_t>(std::max(NormalizedViewport.z, 0.0F) * static_cast<float>(Extent.width)),
                1U,
                Extent.width - ViewportX);
            const std::uint32_t ViewportHeight = std::clamp(
                static_cast<std::uint32_t>(std::max(NormalizedViewport.w, 0.0F) * static_cast<float>(Extent.height)),
                1U,
                Extent.height - ViewportY);

            VkViewport Viewport{};
            Viewport.x = static_cast<float>(ViewportX);
            Viewport.y = static_cast<float>(ViewportY);
            Viewport.width = static_cast<float>(ViewportWidth);
            Viewport.height = static_cast<float>(ViewportHeight);
            Viewport.minDepth = 0.0F;
            Viewport.maxDepth = 1.0F;
            vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

            VkRect2D Scissor{};
            Scissor.offset = {static_cast<std::int32_t>(ViewportX), static_cast<std::int32_t>(ViewportY)};
            Scissor.extent = {ViewportWidth, ViewportHeight};
            vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

            const float     AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
            const glm::mat4 ViewProjection = ViewCamera.GetViewProjectionMatrix(AspectRatio);

            const TSurfaceGPUResourceManager& SurfaceGPU = SurfaceStates.GetGPUResources();
            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 6U);
            std::size_t                       SceneIndex = 0;
            for (const TStaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
            {
                const std::size_t CurrentSceneIndex = SceneIndex++;
                if (Instance.GetMesh() == InvalidAssetHandle)
                {
                    continue;
                }

                const TMeshAsset&                       Mesh = Assets.GetMesh(Instance.GetMesh());
                const TSurfaceStateDescriptorResources* SurfaceDescriptors =
                    SurfaceGPU.GetInstanceDescriptors(CurrentSceneIndex);
                if (bShowSurfaceDebug && (!bCanShowSurfaceDebug || SurfaceDescriptors == nullptr))
                {
                    continue;
                }
                const auto* Shared = SurfaceGPU.GetInstanceSharedGeometry(CurrentSceneIndex);
                if (bTexelGeometry)
                {
                    if (!Shared || !Shared->GetTexelMeshIndexBuffer()) continue;
                    const auto& Pipeline = TexelGeometryPipeline;
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->GetHandle());
                    const auto Layout = Pipeline->GetLayout();
                    const TStaticMeshPushConstants Push{Instance.GetTransform().GetMatrix(), ViewProjection};
                    vkCmdPushConstants(CommandBuffer, Layout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(Push), &Push);
                    const VkBuffer TexelVertices = Shared->GetTexelMeshVertexBuffer()->GetHandle();
                    const VkDeviceSize TexelOffset = 0;
                    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &TexelVertices, &TexelOffset);
                    vkCmdBindIndexBuffer(CommandBuffer, Shared->GetTexelMeshIndexBuffer()->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
                    const std::array<VkDescriptorSet, 2> Sets{
                        SurfaceGPU.IsCurrentStateAB(CurrentSceneIndex) ? SurfaceDescriptors->GetABSet() : SurfaceDescriptors->GetBASet(),
                        TexelGeometryPreview->GetOutputSet(CurrentSceneIndex)};
                    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 1, Sets.size(), Sets.data(), 0, nullptr);
                    for (const auto& Section : Mesh.GetSections())
                    {
                        if (Section.Material >= MaterialResources.size() || Section.Surface >= Shared->GetTexelMeshRanges().size()) continue;
                        const auto& Range = Shared->GetTexelMeshRanges()[Section.Surface];
                        if (Range.IndexCount == 0) continue;
                        const auto MaterialSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 0, 1,
                                                &MaterialSet, 1, &MaterialUniformOffset);
                        vkCmdDrawIndexed(CommandBuffer, Range.IndexCount, 1, Range.FirstIndex, 0, 0);
                    }
                    continue;
                }
                const bool bInstanceSurfaceLit = bSurfaceLit && SurfaceDescriptors;
                const bool bInstanceOverlay = bOverlayRendering &&
                    (OverlayActive[CurrentSceneIndex][0] || OverlayActive[CurrentSceneIndex][1] ||
                     OverlayActive[CurrentSceneIndex][2]);
                const auto& Pipeline = bInstanceSurfaceLit
                    ? (bInstanceOverlay ? *BaseSurfaceLitPipeline : *SurfaceLitPipeline) :
                                       (bCanShowSurfaceDebug ? *SurfaceDebugPipeline :
                                       (bWireframe ? WireframePipeline : StaticMeshPipeline));
                const auto Layout = Pipeline.GetLayout();
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetHandle());
                const VkBuffer     VertexBuffer = Mesh.GetVertexBuffer().GetHandle();
                const VkDeviceSize VertexOffset = 0;
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &VertexOffset);
                vkCmdBindIndexBuffer(CommandBuffer, Mesh.GetIndexBuffer().GetHandle(), 0, VK_INDEX_TYPE_UINT32);

                const TStaticMeshPushConstants PushConstants{Instance.GetTransform().GetMatrix(), ViewProjection};
                vkCmdPushConstants(CommandBuffer,
                                   Layout,
                                   (bInstanceSurfaceLit || bCanShowSurfaceDebug)
                                       ? (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                       : VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(PushConstants),
                                   &PushConstants);

                for (const TMeshSection& Section : Mesh.GetSections())
                {
                    if (Section.Material >= MaterialResources.size())
                    {
                        continue;
                    }

                    const VkDescriptorSet DescriptorSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                    vkCmdBindDescriptorSets(CommandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            Layout,
                                            0,
                                            1,
                                            &DescriptorSet,
                                            1,
                                            &MaterialUniformOffset);

                    if (bCanShowSurfaceDebug || bInstanceSurfaceLit)
                    {
                        const VkDescriptorSet StateSet = SurfaceGPU.IsCurrentStateAB(CurrentSceneIndex)
                                                             ? SurfaceDescriptors->GetABSet()
                                                             : SurfaceDescriptors->GetBASet();
                        vkCmdBindDescriptorSets(CommandBuffer,
                                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                Layout,
                                                1,
                                                1,
                                                &StateSet,
                                                0,
                                                nullptr);
                    }

                    vkCmdDrawIndexed(CommandBuffer,
                                     Section.IndexCount,
                                     1,
                                     Section.FirstIndex,
                                     0,
                                     (bCanShowSurfaceDebug || bInstanceSurfaceLit) ? Section.Surface : 0U);
                }
            }

            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 7U);

            if (bOverlayRendering)
            {
                const auto DrawLayer = [&](std::size_t Layer)
                {
                    const bool bWater = Layer == 1;
                    const bool bLava = Layer == 2;
                    const TStateId Channel = bWater ? DemoBindings.WaterFilm :
                                             bLava ? DemoBindings.Lava : DemoBindings.Mud;
                    if (Channel == InvalidStateId) return;
                    const auto& Preview = bWater ? WaterLayerGeometry : bLava ? LavaLayerGeometry : MudLayerGeometry;
                    const auto& TopPipeline = bWater ? WaterOverlayTopPipeline :
                                              bLava ? LavaOverlayTopPipeline : MudOverlayTopPipeline;
                    const auto& SidePipeline = bWater ? WaterOverlaySidePipeline :
                                               bLava ? LavaOverlaySidePipeline : MudOverlaySidePipeline;
                    std::vector<std::size_t> Order;
                    for (std::size_t I = 0; I < OverlayActive.size(); ++I)
                        if (OverlayActive[I][Layer]) Order.push_back(I);
                    if (bWater)
                    {
                        const auto Camera = ViewCamera.GetPosition();
                        std::sort(Order.begin(), Order.end(), [&](std::size_t A, std::size_t B)
                        {
                            const auto PA = SceneData.GetStaticMeshInstances()[A].GetTransform().Position;
                            const auto PB = SceneData.GetStaticMeshInstances()[B].GetTransform().Position;
                            return glm::dot(PA - Camera, PA - Camera) > glm::dot(PB - Camera, PB - Camera);
                        });
                    }
                    for (const std::size_t I : Order)
                    {
                        const auto& Instance = SceneData.GetStaticMeshInstances()[I];
                        const auto& Mesh = Assets.GetMesh(Instance.GetMesh());
                        const auto* Shared = SurfaceGPU.GetInstanceSharedGeometry(I);
                        const auto* Descriptors = SurfaceGPU.GetInstanceDescriptors(I);
                        if (!Shared || !Descriptors || !Shared->GetTexelMeshIndexBuffer()) continue;
                        const VkDescriptorSet StateSet = SurfaceGPU.IsCurrentStateAB(I)
                            ? Descriptors->GetABSet() : Descriptors->GetBASet();
                        const VkDescriptorSet ComputedSet = DemoEffects.bHeightFieldSmoothing
                            ? HeightFieldSmoothing->GetOutputSet(I, Channel) : Preview->GetOutputSet(I);
                        const TStaticMeshPushConstants Push{Instance.GetTransform().GetMatrix(), ViewProjection};

                        const auto TopLayout = TopPipeline->GetLayout();
                        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TopPipeline->GetHandle());
                        vkCmdPushConstants(CommandBuffer, TopLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                           0, sizeof(Push), &Push);
                        const VkBuffer Vertices = Shared->GetTexelMeshVertexBuffer()->GetHandle();
                        const VkDeviceSize Offset = 0;
                        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Vertices, &Offset);
                        vkCmdBindIndexBuffer(CommandBuffer, Shared->GetTexelMeshIndexBuffer()->GetHandle(), 0,
                                             VK_INDEX_TYPE_UINT32);
                        const std::array<VkDescriptorSet, 3> TopSets{StateSet, ComputedSet,
                            OverlaySides->GetSet(I, Channel)};
                        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TopLayout, 1,
                            static_cast<std::uint32_t>(TopSets.size()), TopSets.data(), 0, nullptr);
                        const VkBuffer TopDrawBuffer = OverlaySides->GetTopDrawBuffer(I, Channel);
                        if (OverlayDrawProfilingMode != TOverlayDrawProfilingMode::SidesOnly)
                        {
                            for (const auto& Section : Mesh.GetSections())
                            {
                                if (Section.Material >= MaterialResources.size() ||
                                    Section.Surface >= Shared->GetTexelMeshRanges().size()) continue;
                                const auto& Range = Shared->GetTexelMeshRanges()[Section.Surface];
                                if (Range.IndexCount == 0) continue;
                                const VkDescriptorSet MaterialSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TopLayout, 0, 1,
                                                         &MaterialSet, 1, &MaterialUniformOffset);
                                vkCmdDrawIndexedIndirect(CommandBuffer, TopDrawBuffer,
                                    static_cast<VkDeviceSize>(Section.Surface) * sizeof(VkDrawIndexedIndirectCommand),
                                    1, 0);
                            }
                        }

                        const auto SideLayout = SidePipeline->GetLayout();
                        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SidePipeline->GetHandle());
                        vkCmdPushConstants(CommandBuffer, SideLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                           0, sizeof(Push), &Push);
                        const std::array<VkDescriptorSet, 3> SideSets{StateSet, ComputedSet,
                            OverlaySides->GetSet(I, Channel)};
                        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SideLayout, 1,
                            static_cast<std::uint32_t>(SideSets.size()), SideSets.data(), 0, nullptr);
                        const VkBuffer SideDrawBuffer = OverlaySides->GetDrawBuffer(I, Channel);
                        if (OverlayDrawProfilingMode != TOverlayDrawProfilingMode::TopOnly)
                        {
                            for (const auto& Section : Mesh.GetSections())
                            {
                                if (Section.Material >= MaterialResources.size() ||
                                    Section.Surface >= Shared->GetTexelMeshRanges().size()) continue;
                                const auto& Range = Shared->GetTexelMeshRanges()[Section.Surface];
                                if (Range.IndexCount == 0) continue;
                                const VkDescriptorSet MaterialSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SideLayout, 0, 1,
                                                         &MaterialSet, 1, &MaterialUniformOffset);
                                vkCmdDrawIndirect(CommandBuffer, SideDrawBuffer,
                                    static_cast<VkDeviceSize>(Section.Surface) * sizeof(VkDrawIndirectCommand), 1, 0);
                            }
                        }
                        if (OverlayDrawProfilingMode != TOverlayDrawProfilingMode::TopOnly &&
                            OverlaySides->GetBoundaryCount(I, Channel) > 0 && !Mesh.GetSections().empty())
                        {
                            const auto Material = Mesh.GetSections().front().Material;
                            if (Material < MaterialResources.size())
                            {
                                const VkDescriptorSet MaterialSet = MaterialResources[Material].DescriptorSets[FrameIndex];
                                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SideLayout, 0, 1,
                                                         &MaterialSet, 1, &MaterialUniformOffset);
                                vkCmdDrawIndirect(CommandBuffer, SideDrawBuffer,
                                    static_cast<VkDeviceSize>(Shared->GetTexelMeshRanges().size()) *
                                        sizeof(VkDrawIndirectCommand), 1, 0);
                            }
                        }
                    }
                };
                if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                    vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 8U);
                DrawLayer(0);
                if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                    vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 9U);
                if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                    vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 10U);
                DrawLayer(1);
                if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                    vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 11U);
                DrawLayer(2);
            }
            else if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
            {
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 8U);
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 9U);
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 10U);
                vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 11U);
            }

            if (GizmoVertexBuffer != nullptr && ((bWorldGridVisible && WorldGridVertexCount > 0) ||
                                                 (bWorldAxisVisible && WorldAxisVertexCount > 0)))
            {
                const TGizmoPushConstants Constants{ViewProjection};
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, WorldReferencePipeline.GetHandle());
                const VkBuffer VertexBuffer = GizmoVertexBuffer->GetHandle();
                const VkDeviceSize Offset = 0;
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &Offset);
                vkCmdPushConstants(CommandBuffer,
                                   WorldReferencePipeline.GetLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0,
                                   sizeof(Constants),
                                   &Constants);
                if (bWorldGridVisible)
                {
                    vkCmdDraw(CommandBuffer, WorldGridVertexCount, 1, 0, 0);
                }
                if (bWorldAxisVisible)
                {
                    vkCmdDraw(CommandBuffer, WorldAxisVertexCount, 1, WorldGridVertexCount, 0);
                }
            }

            if (!DebugInterface.IsInjectModeEnabled())
            {
                if (const std::optional<std::size_t> Selected = DebugInterface.GetSelectedObject();
                    Selected && *Selected < SceneData.GetStaticMeshInstances().size() && GizmoVertexBuffer != nullptr)
                {
                    const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*Selected].GetTransform().Position;
                    const float GizmoScale = glm::length(ViewCamera.GetPosition() - Position) * 0.18F;
                    if (GizmoScale > 0.01F)
                    {
                        const glm::mat4 Model = glm::scale(glm::translate(glm::mat4(1.0F), Position), glm::vec3(GizmoScale));
                        const TGizmoPushConstants Constants{ViewProjection * Model, DebugInterface.GetHoveredGizmoAxis()};
                        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GizmoPipeline.GetHandle());
                        const VkBuffer VertexBuffer = GizmoVertexBuffer->GetHandle();
                        const VkDeviceSize Offset = 0;
                        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &Offset);
                        vkCmdPushConstants(CommandBuffer,
                                           GizmoPipeline.GetLayout(),
                                           VK_SHADER_STAGE_VERTEX_BIT,
                                           0,
                                           sizeof(Constants),
                                           &Constants);
                        const bool bRotationGizmo = DebugInterface.IsRotationGizmoMode();
                        const std::uint32_t FirstVertex = WorldGridVertexCount + WorldAxisVertexCount +
                            (bRotationGizmo ? TranslateGizmoVertexCount : 0U);
                        const std::uint32_t VertexCount =
                            bRotationGizmo ? RotateGizmoVertexCount : TranslateGizmoVertexCount;
                        vkCmdDraw(CommandBuffer, VertexCount, 1, FirstVertex, 0);
                    }
                }
            }

        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 3U);
        DebugInterface.Render(CommandBuffer);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 19U);

        vkCmdEndRenderPass(CommandBuffer);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 20U);
        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record Vulkan command buffer.");
        }
    }

    VkFormat TRenderer::FindDepthFormat(VkPhysicalDevice PhysicalDevice)
    {
        constexpr std::array<VkFormat, 3> Candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };
        return FindSupportedFormat(PhysicalDevice,
                                   Candidates.data(),
                                   static_cast<std::uint32_t>(Candidates.size()),
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat TRenderer::FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
                                           const VkFormat*      Candidates,
                                           std::uint32_t        CandidateCount,
                                           VkImageTiling        Tiling,
                                           VkFormatFeatureFlags Features)
    {
        for (std::uint32_t Index = 0; Index < CandidateCount; ++Index)
        {
            VkFormatProperties Properties{};
            vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Candidates[Index], &Properties);

            const VkFormatFeatureFlags AvailableFeatures =
                Tiling == VK_IMAGE_TILING_LINEAR ? Properties.linearTilingFeatures : Properties.optimalTilingFeatures;
            if ((AvailableFeatures & Features) == Features)
            {
                return Candidates[Index];
            }
        }
        throw std::runtime_error("Failed to find a supported Vulkan format.");
    }
} // MDSS 네임스페이스
