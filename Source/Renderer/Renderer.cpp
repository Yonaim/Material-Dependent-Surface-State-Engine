/**
 * @file Renderer.cpp
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#include "Renderer/Renderer.h"

#include "Application/Window.h"
#include "AssetManager/Assets/MaterialAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "DebugUI/DebugUI.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <array>
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
        };

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
                case TRenderViewMode::SurfaceTexelArea:
                    return "Texel Area Heatmap";
            }
            return "Unknown";
        }

        static_assert(sizeof(TStaticMeshPushConstants) == 128,
                      "Static mesh push constants are expected to use Vulkan's guaranteed 128-byte minimum.");
        static_assert(sizeof(TMaterialUniform) == 48, "TMaterialUniform must match the std140 shader block layout.");

        TGraphicsPipelineConfig BuildStaticMeshPipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            TGraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/StaticMesh.vert.spv", "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Rendering/StaticMesh.frag.spv", "main"},
            };
            Config.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            Config.CullMode = VK_CULL_MODE_BACK_BIT;
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
        Config.DescriptorSetLayouts.push_back(SurfaceLayout);
            return Config;
        }

        TGraphicsPipelineConfig BuildWireframePipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            TGraphicsPipelineConfig Config = BuildStaticMeshPipelineConfig(MaterialLayout);
            Config.PolygonMode = VK_POLYGON_MODE_LINE;
            return Config;
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
            VkVertexInputAttributeDescription EdgeCoordinate{};
            EdgeCoordinate.location = 2;
            EdgeCoordinate.binding = 0;
            EdgeCoordinate.format = VK_FORMAT_R32_SFLOAT;
            EdgeCoordinate.offset = static_cast<std::uint32_t>(offsetof(TGizmoVertex, EdgeCoordinate));
            Config.VertexAttributes.push_back(EdgeCoordinate);
            VkPushConstantRange Push{};
            Push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            Push.size = sizeof(TGizmoPushConstants);
            Config.PushConstantRanges.push_back(Push);
            return Config;
        }

        TGraphicsPipelineConfig BuildWorldReferencePipelineConfig()
        {
            TGraphicsPipelineConfig Config = BuildGizmoPipelineConfig();
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
                         const TScene& Scene)
        : Context(Context), TargetWindow(TWindow), Assets(Assets), SwapchainData(Context, TWindow),
          DepthFormat(FindDepthFormat(Context.GetPhysicalDevice())),
          DepthImage(Context.GetPhysicalDevice(),
                     Context.GetDevice(),
                     SwapchainData.GetExtent(),
                     DepthFormat,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          DepthImageView(Context.GetDevice(), DepthImage.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT),
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
                           DepthImageView.GetHandle()),
          FrameContext(Context)
    {
        Assets.ExchangeSurfaceStateRegistry(Assets.BuildSurfaceStateRegistry(Scene));
        std::vector<TGizmoVertex> GizmoVertices = BuildWorldReferenceVertices(WorldGridVertexCount,
                                                                               WorldAxisVertexCount);
        const std::vector<TGizmoVertex> TranslateGizmoVertices = BuildTranslateGizmoVertices();
        GizmoVertices.insert(GizmoVertices.end(), TranslateGizmoVertices.begin(), TranslateGizmoVertices.end());
        GizmoVertexCount = static_cast<std::uint32_t>(GizmoVertices.size());
        GizmoVertexBuffer =
            std::make_unique<TGPUBuffer>(Context.GetPhysicalDevice(),
                                                        Context.GetDevice(),
                                                        static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)),
                                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        GizmoVertexBuffer->Upload(GizmoVertices.data(),
                                  static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)));
        CreateMaterialDescriptorResources();
        SurfaceStates = std::make_unique<TSurfaceStateSystem>(Context, Assets, Scene);
        if (const TSurfaceStateDescriptorResources* Descriptors =
                SurfaceStates->GetGPUResources().GetAnyInstanceDescriptors())
        {
            SurfaceDebugPipeline = std::make_unique<TGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
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
            CreateTimestampQueryPool(SurfaceStates->GetSolverTimestampSlotCount());
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
        SurfaceStates.reset();

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

    void TRenderer::RenderFrame(const TScene& SceneData, TDebugUI& DebugInterface, float DeltaTime)
    {
        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        FrameContext.WaitForCurrentFrame();
        if (TimestampQueryPool != VK_NULL_HANDLE && bTimestampQueriesSubmitted[FrameIndex])
        {
            const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
            const std::uint32_t QueryCount = bSolverTimestampQueriesSubmitted[FrameIndex]
                                                 ? TimestampQueriesPerFrame
                                                 : 2U;
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
                const auto ToMilliseconds = [this](std::uint64_t Start, std::uint64_t End)
                {
                    std::uint64_t ElapsedTicks = End - Start;
                    if (TimestampValidBits < 64U)
                    {
                        ElapsedTicks &= (std::uint64_t{1} << TimestampValidBits) - 1U;
                    }
                    return static_cast<float>(static_cast<double>(ElapsedTicks) * TimestampPeriodNanoseconds / 1.0e6);
                };
                LastRenderGpuMilliseconds = ToMilliseconds(Timestamps[0], Timestamps[1]);
                if (bSolverTimestampQueriesSubmitted[FrameIndex])
                {
                    LastSolverPass1GpuMilliseconds = 0.0F;
                    LastSolverPass2GpuMilliseconds = 0.0F;
                    for (std::uint32_t Slot = 0; Slot < SolverTimestampSlotCount; ++Slot)
                    {
                        const std::uint32_t SlotQuery = 2U + Slot * 4U;
                        LastSolverPass1GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery], Timestamps[SlotQuery + 1U]);
                        LastSolverPass2GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 2U], Timestamps[SlotQuery + 3U]);
                    }
                    LastSolverGpuMilliseconds =
                        LastSolverPass1GpuMilliseconds + LastSolverPass2GpuMilliseconds;
                }
            }
            bTimestampQueriesSubmitted[FrameIndex] = false;
            bSolverTimestampQueriesSubmitted[FrameIndex] = false;
        }

        const bool bResetSolverState = DebugInterface.ConsumeSolverResetRequest();
        const bool bRequestSolverStep = DebugInterface.ConsumeSolverStepRequest();
        if (bResetSolverState)
        {
            SurfaceStates->ResetState();
        }
        const bool bRunSolverStep = !bResetSolverState &&
                                    (!DebugInterface.IsSimulationPaused() || bRequestSolverStep);

        if (TargetWindow.WasFramebufferResized())
        {
            RecreateSwapchain(DebugInterface);
            return;
        }

        std::uint32_t  ImageIndex = 0;
        const VkResult AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);

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

        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData, DebugInterface, DeltaTime, bRunSolverStep);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            bTimestampQueriesSubmitted[FrameIndex] = true;
            bSolverTimestampQueriesSubmitted[FrameIndex] = bRunSolverStep;
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

        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }

        const VkSwapchainKHR SwapchainHandle = SwapchainData.GetHandle();
        VkPresentInfoKHR     PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = &SignalSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &SwapchainHandle;
        PresentInfo.pImageIndices = &ImageIndex;

        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
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

    void TRenderer::SubmitContact(TSurfaceContactInput Contact)
    {
        if (SurfaceStates)
        {
            SurfaceStates->SubmitContact(std::move(Contact));
        }
    }

    void TRenderer::SetDebugProfileParameters(TSRProfileAssetHandle Profile,
                                              TStateId State,
                                              const TSurfaceStateParameters& Parameters,
                                              bool bKeepRuntimeOverride)
    {
        SurfaceStates->SetDebugProfileParameters(Profile, State, Parameters, bKeepRuntimeOverride);
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
        auto PreviousRegistry = Assets.ExchangeSurfaceStateRegistry(Assets.BuildSurfaceStateRegistry(Scene));
        std::unique_ptr<TSurfaceStateSystem> Replacement;
        std::unique_ptr<TGraphicsPipeline> ReplacementDebugPipeline;
        try
        {
            Replacement = std::make_unique<TSurfaceStateSystem>(Context, Assets, Scene);
            Replacement->SetRawFluxCacheEnabled(DebugSolverSettings.bRawFluxCacheEnabled);
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
            }
        }
        catch (...)
        {
            Assets.ExchangeSurfaceStateRegistry(std::move(PreviousRegistry));
            throw;
        }
        SurfaceDebugPipeline.reset();
        SurfaceStates = std::move(Replacement);
        SurfaceDebugPipeline = std::move(ReplacementDebugPipeline);
        Assets.SetSimulationResolution(Scene.GetSimulationResolution());
        if (bResetStateSettings)
        {
            DebugProfileParameterOverrides.clear();
            DebugStateChannel = 0;
        }
        CreateTimestampQueryPool(SurfaceStates->GetSolverTimestampSlotCount());
        UpdateMaterialUniforms();
        LastRenderGpuMilliseconds = -1.0F;
        LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = -1.0F;
        LastSolverPass2GpuMilliseconds = -1.0F;
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
        // 두 이미지 뷰를 다시 만들기 전에 프레임버퍼를 먼저 해제한다.
        MainFramebuffers.Reset();
        DepthImageView.Reset();
        DepthImage.Reset();

        const VkFormat PreviousColorFormat = SwapchainData.GetImageFormat();
        SwapchainData.Recreate(Context, TargetWindow);
        CreateRenderFinishedSemaphores();

        if (PreviousColorFormat != SwapchainData.GetImageFormat())
        {
            throw std::runtime_error(
                "TSwapchain color format changed during resize. TRenderPass/Pipeline recreation is required.");
        }

        DepthImage.Recreate(Context.GetPhysicalDevice(),
                            SwapchainData.GetExtent(),
                            DepthFormat,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        DepthImageView.Recreate(DepthImage.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        MainFramebuffers.Recreate(MainRenderPass.GetHandle(),
                                  SwapchainData.GetExtent(),
                                  SwapchainData.GetImageViews(),
                                  DepthImageView.GetHandle());

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
        return SurfaceStates->GetGPUResources();
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
        UpdateMaterialUniforms();
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
        UpdateMaterialUniforms();
    }

    bool TRenderer::IsStateHeatmapReliefShadingEnabled() const noexcept
    {
        return bStateHeatmapReliefShadingEnabled;
    }

    void TRenderer::SetStateHeatmapReliefShadingEnabled(bool bEnabled)
    {
        if (bStateHeatmapReliefShadingEnabled == bEnabled)
        {
            return;
        }
        bStateHeatmapReliefShadingEnabled = bEnabled;
        UpdateMaterialUniforms();
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
        UpdateMaterialUniforms();
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
        UpdateMaterialUniforms();
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
        UpdateMaterialUniforms();
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
        if (SurfaceStates)
        {
            SurfaceStates->SetDebugSolverTermEnabled(Term, bEnabled);
        }
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
        if (SurfaceStates) SurfaceStates->SetRawFluxCacheEnabled(bEnabled);
        bTimestampQueriesSubmitted.fill(false);
        bSolverTimestampQueriesSubmitted.fill(false);
        LastRenderGpuMilliseconds = LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = LastSolverPass2GpuMilliseconds = -1.0F;
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
        UpdateMaterialUniforms();
        TLogger::Info("TRenderer", std::string("Normal-map Y flip ") + (bFlipNormalY ? "enabled." : "disabled."));
    }

    float TRenderer::GetNormalStrength() const noexcept
    {
        return NormalStrength;
    }

    void TRenderer::SetNormalStrength(float Strength)
    {
        NormalStrength = std::clamp(Strength, 0.0F, 4.0F);
        UpdateMaterialUniforms();
    }

    float TRenderer::GetAmbientLight() const noexcept
    {
        return AmbientLight;
    }

    void TRenderer::SetAmbientLight(float Intensity)
    {
        AmbientLight = std::clamp(Intensity, 0.0F, 1.0F);
        UpdateMaterialUniforms();
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
        Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
        if (SolverInstanceCount > (std::numeric_limits<std::uint32_t>::max() - 2U) / 4U)
        {
            TLogger::Warning("TRenderer", "GPU timing query count exceeds the supported range.");
            return;
        }

        SolverTimestampSlotCount = static_cast<std::uint32_t>(SolverInstanceCount);
        TimestampQueriesPerFrame = 2U + SolverTimestampSlotCount * 4U;
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
        bSolverTimestampQueriesSubmitted.fill(false);
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
        PoolSizes[0].descriptorCount = static_cast<std::uint32_t>(MaterialCount * 2U);
        PoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        PoolSizes[1].descriptorCount = static_cast<std::uint32_t>(MaterialCount);

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = static_cast<std::uint32_t>(MaterialCount);

        if (vkCreateDescriptorPool(Context.GetDevice(), &PoolInfo, nullptr, &MaterialDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create material descriptor pool.");
        }

        std::vector<VkDescriptorSetLayout> Layouts(MaterialCount, MaterialDescriptorSetLayout);
        std::vector<VkDescriptorSet>       Sets(MaterialCount, VK_NULL_HANDLE);

        VkDescriptorSetAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocateInfo.descriptorPool = MaterialDescriptorPool;
        AllocateInfo.descriptorSetCount = static_cast<std::uint32_t>(MaterialCount);
        AllocateInfo.pSetLayouts = Layouts.data();

        if (vkAllocateDescriptorSets(Context.GetDevice(), &AllocateInfo, Sets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate material descriptor sets.");
        }

        MaterialResources.resize(MaterialCount);
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            const TMaterialAssetHandle Handle = static_cast<TMaterialAssetHandle>(Index);
            const TMaterialAsset&      Material = Assets.GetMaterial(Handle);
            const TextureAsset&       BaseTexture = Assets.GetTexture(Material.GetBaseColorTexture());
            const TextureAsset&       NormalTexture = Assets.GetTexture(Material.GetNormalTexture());

            MaterialResources[Index].UniformBuffer = std::make_unique<TGPUBuffer>(
                Context.GetPhysicalDevice(),
                                            Context.GetDevice(),
                                            sizeof(TMaterialUniform),
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            MaterialResources[Index].DescriptorSet = Sets[Index];

            const TMaterialUniform Uniform{Material.GetBaseColor(),
                                           static_cast<std::uint32_t>(ViewMode),
                                           bFlipNormalY ? 1U : 0U,
                                           NormalStrength,
                                           AmbientLight,
                                           DebugStateChannel,
                                           static_cast<std::uint32_t>(Assets.GetSurfaceStateRegistry().GetStateCount()),
                                           GetDebugViewParameter(),
                                           bStateHeatmapReliefShadingEnabled ? 1.0F : 0.0F};
            MaterialResources[Index].UniformBuffer->Upload(&Uniform, sizeof(Uniform));

            VkDescriptorImageInfo BaseImage{};
            BaseImage.sampler = BaseTexture.GetSampler();
            BaseImage.imageView = BaseTexture.GetImageView();
            BaseImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo NormalImage{};
            NormalImage.sampler = NormalTexture.GetSampler();
            NormalImage.imageView = NormalTexture.GetImageView();
            NormalImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorBufferInfo MaterialBuffer{};
            MaterialBuffer.buffer = MaterialResources[Index].UniformBuffer->GetHandle();
            MaterialBuffer.offset = 0;
            MaterialBuffer.range = sizeof(TMaterialUniform);

            std::array<VkWriteDescriptorSet, 3> Writes{};
            for (VkWriteDescriptorSet& Write : Writes)
            {
                Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Write.dstSet = Sets[Index];
                Write.descriptorCount = 1;
            }
            Writes[0].dstBinding = 0;
            Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Writes[0].pImageInfo = &BaseImage;
            Writes[1].dstBinding = 1;
            Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Writes[1].pImageInfo = &NormalImage;
            Writes[2].dstBinding = 2;
            Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            Writes[2].pBufferInfo = &MaterialBuffer;

            vkUpdateDescriptorSets(
                Context.GetDevice(), static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
        }

        TLogger::Info("TRenderer",
                      "Material descriptor sets created: " + std::to_string(MaterialResources.size()) + ".");
    }

    void TRenderer::UpdateMaterialUniforms()
    {
        const std::size_t MaterialCount = std::min(Assets.GetMaterialCount(), MaterialResources.size());
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            if (!MaterialResources[Index].UniformBuffer)
            {
                continue;
            }

            const TMaterialAsset&  Material = Assets.GetMaterial(static_cast<TMaterialAssetHandle>(Index));
            const TMaterialUniform Uniform{Material.GetBaseColor(),
                                           static_cast<std::uint32_t>(ViewMode),
                                           bFlipNormalY ? 1U : 0U,
                                           NormalStrength,
                                           AmbientLight,
                                           DebugStateChannel,
                                           static_cast<std::uint32_t>(Assets.GetSurfaceStateRegistry().GetStateCount()),
                                           GetDebugViewParameter(),
                                           bStateHeatmapReliefShadingEnabled ? 1.0F : 0.0F};
            MaterialResources[Index].UniformBuffer->Upload(&Uniform, sizeof(Uniform));
        }
    }

    void TRenderer::RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                       std::uint32_t   ImageIndex,
                                       const TScene&    SceneData,
                                       const TDebugUI&  DebugInterface,
                                       float            DeltaTime,
                                       bool             bRunSolverStep)
    {
        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin Vulkan command buffer.");
        }

        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdResetQueryPool(CommandBuffer, TimestampQueryPool, QueryBase, TimestampQueriesPerFrame);
        }
        if (bRunSolverStep)
        {
            SurfaceStates->RecordStep(CommandBuffer, DeltaTime, TimestampQueryPool, QueryBase + 2U);
        }

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
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase);
        }
        vkCmdBeginRenderPass(CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        const bool bShowSurfaceDebug = ViewMode >= TRenderViewMode::SurfaceStateHeatmap;
        const bool bCanShowSurfaceDebug = bShowSurfaceDebug && SurfaceDebugPipeline != nullptr;
        const bool bWireframe = ViewMode == TRenderViewMode::Wireframe;
        vkCmdBindPipeline(CommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bCanShowSurfaceDebug ? SurfaceDebugPipeline->GetHandle()
                                               : (bWireframe ? WireframePipeline.GetHandle()
                                                             : StaticMeshPipeline.GetHandle()));

        const VkExtent2D    Extent = SwapchainData.GetExtent();
        const glm::vec4     NormalizedViewport = DebugInterface.GetSceneViewportRectNormalized();
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
        const glm::mat4 ViewProjection = SceneData.GetMainCamera().GetViewProjectionMatrix(AspectRatio);

        const TSurfaceGPUResourceManager& SurfaceGPU = SurfaceStates->GetGPUResources();
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
            const VkBuffer     VertexBuffer = Mesh.GetVertexBuffer().GetHandle();
            const VkDeviceSize VertexOffset = 0;
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &VertexOffset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh.GetIndexBuffer().GetHandle(), 0, VK_INDEX_TYPE_UINT32);

            const TStaticMeshPushConstants PushConstants{Instance.GetTransform().GetMatrix(), ViewProjection};
            vkCmdPushConstants(CommandBuffer,
                               bWireframe && !bCanShowSurfaceDebug ? WireframePipeline.GetLayout()
                                                                  : StaticMeshPipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(PushConstants),
                               &PushConstants);

            for (const TMeshSection& Section : Mesh.GetSections())
            {
                if (Section.Material >= MaterialResources.size())
                {
                    continue;
                }

                const VkDescriptorSet DescriptorSet = MaterialResources[Section.Material].DescriptorSet;
                vkCmdBindDescriptorSets(CommandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        bCanShowSurfaceDebug ? SurfaceDebugPipeline->GetLayout()
                                                            : (bWireframe ? WireframePipeline.GetLayout()
                                                                          : StaticMeshPipeline.GetLayout()),
                                        0,
                                        1,
                                        &DescriptorSet,
                                        0,
                                        nullptr);

                if (bCanShowSurfaceDebug)
                {
                    const VkDescriptorSet StateSet = SurfaceGPU.IsCurrentStateAB(CurrentSceneIndex)
                                                         ? SurfaceDescriptors->GetABSet()
                                                         : SurfaceDescriptors->GetBASet();
                    vkCmdBindDescriptorSets(CommandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            SurfaceDebugPipeline->GetLayout(),
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
                                 bCanShowSurfaceDebug ? Section.Surface : 0U);
            }
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
                const float GizmoScale = glm::length(SceneData.GetMainCamera().GetPosition() - Position) * 0.18F;
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
                    vkCmdDraw(CommandBuffer,
                              GizmoVertexCount - WorldGridVertexCount - WorldAxisVertexCount,
                              1,
                              WorldGridVertexCount + WorldAxisVertexCount,
                              0);
                }
            }
        }

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(CommandBuffer,
                                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                TimestampQueryPool,
                                QueryBase + 1U);
        }
        DebugInterface.Render(CommandBuffer);

        vkCmdEndRenderPass(CommandBuffer);
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
