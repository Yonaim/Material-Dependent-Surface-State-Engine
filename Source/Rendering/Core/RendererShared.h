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
#include "Rendering/Scene/WorldReferenceGeometry.h"
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
// ============================================================
// 00. Shader Variants and Frame Constants
// ============================================================
#pragma region 00_Shader_Variants_and_Frame_Constants

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

#pragma endregion

// ============================================================
// 01. Attachment Construction
// ============================================================
#pragma region 01_Attachment_Construction

        std::vector<std::unique_ptr<GPU::FGPUImage>> CreateDepthImages(
            VkPhysicalDevice PhysicalDevice, VkDevice Device, VkExtent2D Extent, VkFormat Format, std::size_t Count)
        {
            std::vector<std::unique_ptr<GPU::FGPUImage>> Images;
            Images.reserve(Count);
            for (std::size_t Index = 0; Index < Count; ++Index)
                Images.push_back(std::make_unique<GPU::FGPUImage>(PhysicalDevice,
                                                                  Device,
                                                                  Extent,
                                                                  Format,
                                                                  VK_IMAGE_TILING_OPTIMAL,
                                                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
            return Images;
        }

        std::vector<std::unique_ptr<GPU::FGPUImageView>> CreateDepthImageViews(
            VkDevice Device, const std::vector<std::unique_ptr<GPU::FGPUImage>>& Images, VkFormat Format)
        {
            std::vector<std::unique_ptr<GPU::FGPUImageView>> Views;
            Views.reserve(Images.size());
            for (const auto& Image : Images)
                Views.push_back(std::make_unique<GPU::FGPUImageView>(
                    Device, Image->GetHandle(), Format, VK_IMAGE_ASPECT_DEPTH_BIT));
            return Views;
        }

        std::vector<VkImageView> GetDepthImageViewHandles(const std::vector<std::unique_ptr<GPU::FGPUImageView>>& Views)
        {
            std::vector<VkImageView> Handles;
            Handles.reserve(Views.size());
            for (const auto& View : Views)
                Handles.push_back(View->GetHandle());
            return Handles;
        }

#pragma endregion

// ============================================================
// 02. Mesh and World Geometry
// ============================================================
#pragma region 02_Mesh_and_World_Geometry

        struct FStaticMeshPushConstants
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };

#pragma endregion

// ============================================================
// 03. Material GPU Interface
// ============================================================
#pragma region 03_Material_GPU_Interface

        struct alignas(16) FMaterialUniform
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
            glm::uvec4 ExtraDemoBindings{SurfaceState::InvalidStateId, 0, SurfaceState::InvalidStateId, 0};
            glm::uvec4 BurnScorchedBindings{SurfaceState::InvalidStateId, SurfaceState::InvalidStateId, 0, 0};
        };

        constexpr std::uint32_t RenderModeWireframeUniformWhite = 20U;

        const char* GetRenderViewModeName(ERenderViewMode Mode)
        {
            switch (Mode)
            {
                case ERenderViewMode::Lit:
                    return "Lit";
                case ERenderViewMode::Unlit:
                    return "Unlit";
                case ERenderViewMode::Wireframe:
                    return "Wireframe";
                case ERenderViewMode::VertexNormalWS:
                    return "Vertex Normal (World Space)";
                case ERenderViewMode::NormalTextureTS:
                    return "Normal Texture (Tangent Space)";
                case ERenderViewMode::MappedNormalWS:
                    return "Mapped Normal (World Space)";
                case ERenderViewMode::SurfaceStateHeatmap:
                    return "Surface State Heatmap";
                case ERenderViewMode::SurfaceValidity:
                    return "Surface Validity";
                case ERenderViewMode::SurfaceID:
                    return "Surface ID";
                case ERenderViewMode::NeighborCount:
                    return "Neighbor Count";
                case ERenderViewMode::SurfaceSeam:
                    return "Surface Seam";
                case ERenderViewMode::OutgoingFluxScale:
                    return "Outgoing Flux Scale";
                case ERenderViewMode::SolverTransferWeight:
                    return "Solver Transfer Weight";
                case ERenderViewMode::MesoHeight:
                    return "Meso Color";
                case ERenderViewMode::MesoOffset:
                    return "Meso Displacement";
                case ERenderViewMode::MacroGeometry:
                    return "Macro Geometry";
                case ERenderViewMode::SurfaceTexelGrid:
                    return "Texel Grid";
                case ERenderViewMode::SurfaceAccumulation:
                    return "Accumulation";
                case ERenderViewMode::SurfaceFinalGeometry:
                    return "Final Geometry";
                case ERenderViewMode::TotalSimulationHeight:
                    return "Total Simulation Height";
                case ERenderViewMode::SurfaceTexelArea:
                    return "Texel Area Heatmap";
            }
            return "Unknown";
        }

        static_assert(sizeof(FStaticMeshPushConstants) == 128,
                      "Static mesh push constants are expected to use Vulkan's guaranteed 128-byte minimum.");
        static_assert(sizeof(FMaterialUniform) == 368, "TMaterialUniform must match the std140 shader block layout.");

        GPU::FGraphicsPipelineConfig BuildStaticMeshPipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            GPU::FGraphicsPipelineConfig Config{};
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
            Binding.stride = sizeof(Asset::FVertex);
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

            AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::FVertex, Position)));
            AddAttribute(1, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::FVertex, Normal)));
            AddAttribute(2, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::FVertex, UV)));
            AddAttribute(
                3, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Asset::FVertex, Tangent)));

            VkPushConstantRange PushConstantRange{};
            PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            PushConstantRange.offset = 0;
            PushConstantRange.size = sizeof(FStaticMeshPushConstants);
            Config.PushConstantRanges.push_back(PushConstantRange);

            return Config;
        }

        GPU::FGraphicsPipelineConfig BuildSurfaceDebugPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                     VkDescriptorSetLayout SurfaceLayout)
        {
            GPU::FGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
            Config.ShaderStages[0].ShaderPath = Shaders::Path(EShader::Debug_SurfaceDebug_vert);
            Config.ShaderStages[1].ShaderPath = Shaders::Path(EShader::Debug_SurfaceDebug_frag);
            Config.PushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            Config.DescriptorSetLayouts.push_back(SurfaceLayout);
            return Config;
        }

        GPU::FGraphicsPipelineConfig BuildWireframePipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            GPU::FGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
            Config.CullMode = VK_CULL_MODE_BACK_BIT;
            Config.PolygonMode = VK_POLYGON_MODE_LINE;
            Config.bDynamicLineWidth = true;
            return Config;
        }

#pragma endregion

// ============================================================
// 04. Geometry Mode Selection
// ============================================================
#pragma region 04_Geometry_Mode_Selection

        bool UsesTexelGeometry(ERenderViewMode Mode)
        {
            return Mode == ERenderViewMode::MesoHeight || Mode == ERenderViewMode::MesoOffset ||
                   Mode == ERenderViewMode::SurfaceAccumulation || Mode == ERenderViewMode::SurfaceFinalGeometry ||
                   Mode == ERenderViewMode::TotalSimulationHeight;
        }

        void SetTexelMeshVertexLayout(GPU::FGraphicsPipelineConfig& Config)
        {
            using FMeshVertex = SurfaceState::FSurfaceTexelMeshVertex;
            Config.VertexBindings = {{0, sizeof(FMeshVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
            Config.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FMeshVertex, Position)},
                                       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FMeshVertex, Normal)},
                                       {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(FMeshVertex, UVSurface)},
                                       {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FMeshVertex, DisplacementNormal)},
                                       {4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(FMeshVertex, Samples)},
                                       {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(FMeshVertex, Weights)}};
        }

        GPU::FGraphicsPipelineConfig BuildTexelGeometryPipelineConfig(VkDescriptorSetLayout MaterialLayout,
                                                                      VkDescriptorSetLayout SurfaceLayout,
                                                                      VkDescriptorSetLayout OutputLayout)
        {
            auto Config = BuildSurfaceDebugPipelineConfig(MaterialLayout, SurfaceLayout);
            Config.ShaderStages[0].ShaderPath = Shaders::Path(EShader::Rendering_Overlay_Geometry_TexelGeometry_vert);
            SetTexelMeshVertexLayout(Config);
            Config.DescriptorSetLayouts.push_back(OutputLayout);
            return Config;
        }

        GPU::FGraphicsPipelineConfig
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

        GPU::FGraphicsPipelineConfig BuildBaseSurfaceLitPipelineConfig(VkDescriptorSetLayout MaterialLayout,
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

        GPU::FGraphicsPipelineConfig
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

        bool UsesAccumulationGeometry(const FStaticMeshInstance&                              Instance,
                                      const Asset::FAssetManager&                             Assets,
                                      const SurfaceState::FSurfaceDataManager&                SurfaceData,
                                      const SurfaceState::FSurfaceSharedGeometryGPUResources* Shared,
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

        GPU::FGraphicsPipelineConfig BuildGizmoPipelineConfig()
        {
            GPU::FGraphicsPipelineConfig Config{};
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
            Binding.stride = sizeof(FGizmoVertex);
            Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            Config.VertexBindings.push_back(Binding);
            VkVertexInputAttributeDescription Position{};
            Position.location = 0;
            Position.binding = 0;
            Position.format = VK_FORMAT_R32G32B32_SFLOAT;
            Position.offset = static_cast<std::uint32_t>(offsetof(FGizmoVertex, Position));
            Config.VertexAttributes.push_back(Position);
            VkVertexInputAttributeDescription Color{};
            Color.location = 1;
            Color.binding = 0;
            Color.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            Color.offset = static_cast<std::uint32_t>(offsetof(FGizmoVertex, Color));
            Config.VertexAttributes.push_back(Color);
            VkPushConstantRange Push{};
            Push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            Push.size = sizeof(FGizmoPushConstants);
            Config.PushConstantRanges.push_back(Push);
            return Config;
        }

        GPU::FGraphicsPipelineConfig BuildWorldReferencePipelineConfig()
        {
            GPU::FGraphicsPipelineConfig Config = BuildGizmoPipelineConfig();
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, Shaders::Path(EShader::Rendering_Scene_WorldReference_vert), "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, Shaders::Path(EShader::Rendering_Scene_WorldReference_frag), "main"},
            };
            VkVertexInputAttributeDescription EdgeCoordinate{};
            EdgeCoordinate.location = 2;
            EdgeCoordinate.binding = 0;
            EdgeCoordinate.format = VK_FORMAT_R32_SFLOAT;
            EdgeCoordinate.offset = static_cast<std::uint32_t>(offsetof(FGizmoVertex, EdgeCoordinate));
            Config.VertexAttributes.push_back(EdgeCoordinate);
            Config.bDepthTestEnabled = true;
            Config.bDepthWriteEnabled = false;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            Config.bBlendingEnabled = true;
            return Config;
        }

#pragma endregion

// ============================================================
// 05. Scope Cleanup
// ============================================================
#pragma region 05_Scope_Cleanup

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

#pragma endregion
