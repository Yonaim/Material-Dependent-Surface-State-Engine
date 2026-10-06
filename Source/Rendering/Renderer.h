/**
 * @file Renderer.h
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Pass/GraphicsPass.h"
#include "GPU/Vulkan/Pipeline/GraphicsPipeline.h"
#include "GPU/Vulkan/Render/Framebuffer.h"
#include "GPU/Vulkan/Render/RenderPass.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "GPU/Vulkan/Resource/GPUImage.h"
#include "GPU/Vulkan/Resource/GPUImageView.h"
#include "GPU/Vulkan/Swapchain/Swapchain.h"
#include "Rendering/AccumulationOverlaySides.h"
#include "Rendering/DemoSurfaceEffects.h"
#include "Rendering/HeightFieldSmoothing.h"
#include "Rendering/RenderStateTexture.h"
#include "Rendering/RenderContext.h"
#include "SurfaceState/Debug/TexelGeometryPreview.h"
#include "SurfaceState/Debug/TexelInspector.h"
#include "SurfaceState/State/SimulationClock.h"
#include "SurfaceState/SurfaceStateSystem.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace MDSS::Asset
{
    class TAssetManager;
}
namespace MDSS::SurfaceState
{
    class TSurfaceDataManager;
}
namespace MDSS::GPU
{
    class TVulkanContext;
}

namespace MDSS
{
    class TWindow;
}

namespace MDSS
{
    class TScene;
    class TDebugUI;
    class TStaticMeshInstance;
}

namespace MDSS::Rendering
{
    enum class TOverlayDisplayMode : std::uint32_t
    {
        Both = 0,
        TopOnly,
        SidesOnly
    };

    enum class TOverlayOccupancyTileSize : std::uint32_t
    {
        Tile8 = 8,
        Tile16 = 16,
        Tile32 = 32
    };

    struct TRendererProfilingStats
    {
        float                     SolverGpuMilliseconds = -1.0F;
        float                     SolverPass1GpuMilliseconds = -1.0F;
        float                     SolverPass2GpuMilliseconds = -1.0F;
        float                     AccumulationGeometryGpuMilliseconds = -1.0F;
        float                     TransferWeightGpuMilliseconds = -1.0F;
        float                     RenderPreparationGpuMilliseconds = -1.0F;
        float                     SceneDrawGpuMilliseconds = -1.0F;
        float                     TexelInspectorGpuMilliseconds = -1.0F;
        float                     BaseMeshDrawGpuMilliseconds = -1.0F;
        float                     MudOverlayDrawGpuMilliseconds = -1.0F;
        float                     ReservedOverlayDrawGpuMilliseconds = -1.0F;
        float                     OverlayPreparationGpuMilliseconds = -1.0F;
        float                     OverlayGeometryGpuMilliseconds = -1.0F;
        float                     OverlayHeightGpuMilliseconds = -1.0F;
        float                     OverlayNormalVertexGpuMilliseconds = -1.0F;
        float                     OverlaySmoothingGpuMilliseconds = -1.0F;
        float                     OverlaySidesGpuMilliseconds = -1.0F;
        float                     OverlaySidesPreBarrierGpuMilliseconds = -1.0F;
        float                     OverlaySidesDispatchGpuMilliseconds = -1.0F;
        float                     OverlaySidesPostBarrierGpuMilliseconds = -1.0F;
        float                     OverlayBoundarySearchGpuMilliseconds = -1.0F;
        float                     OverlayTopCommandGpuMilliseconds = -1.0F;
        float                     OverlaySidesInterPassBarrierGpuMilliseconds = -1.0F;
        float                     OverlayCoverageSampleGpuMilliseconds = -1.0F;
        float                     SceneSetupGpuMilliseconds = -1.0F;
        float                     RenderPassBeginGpuMilliseconds = -1.0F;
        float                     RenderPassColorStageGpuMilliseconds = -1.0F;
        float                     RenderPassDepthStageGpuMilliseconds = -1.0F;
        float                     ViewportSetupGpuMilliseconds = -1.0F;
        float                     UIDrawGpuMilliseconds = -1.0F;
        float                     RenderPassEndGpuMilliseconds = -1.0F;
        float                     SceneBetweenDrawsGpuMilliseconds = -1.0F;
        float                     SceneTailGpuMilliseconds = -1.0F;
        float                     FrameFenceWaitCpuMilliseconds = -1.0F;
        float                     AcquireCpuMilliseconds = -1.0F;
        float                     CommandRecordCpuMilliseconds = -1.0F;
        float                     QueueSubmitCpuMilliseconds = -1.0F;
        float                     PresentCpuMilliseconds = -1.0F;
        std::uint32_t             SimulationSteps = 0;
        float                     SimulatedThisFrameSeconds = 0.0F;
        float                     PendingSimulationSeconds = 0.0F;
        float                     DroppedSimulationSeconds = 0.0F;
        std::uint32_t             SimulationInstances = 0;
        std::uint64_t             SimulationTexels = 0;
        std::uint32_t             StateChannels = 0;
        std::uint32_t             SimulationResolution = 0;
        std::uint32_t             SurfaceTexelMeshResolution = 0;
        std::uint32_t             OverlayTexelMeshResolution = 0;
        std::uint64_t             OverlayActiveTopTriangles = 0;
        std::uint64_t             OverlayTotalTopTriangles = 0;
        std::uint64_t             OverlayActiveTiles = 0;
        std::uint64_t             OverlayTotalTiles = 0;
        TOverlayDisplayMode OverlayDisplayMode = TOverlayDisplayMode::Both;
    };

    enum class TRenderViewMode : std::uint32_t
    {
        Lit = 0,
        Unlit = 1,
        Wireframe = 2,
        VertexNormalWS = 3,
        NormalTextureTS = 4,
        MappedNormalWS = 5,
        SurfaceStateHeatmap = 6,
        SurfaceValidity = 7,
        SurfaceID = 8,
        NeighborCount = 9,
        SurfaceSeam = 10,
        OutgoingFluxScale = 11,
        SolverTransferWeight = 12,
        /** @brief 부호가 있는 중간 규모 높이를 색상으로 표시한다. */
        MesoHeight = 13,
        /** @brief texel 연결면을 복원한 Meso 높이로 표시한다. */
        MesoOffset = 14,
        /** @brief 원본 거시 형상을 노멀 맵 음영 없이 표시한다. */
        MacroGeometry = 15,
        /** @brief 확대 시 개별 텍셀과 고정 크기 묶음의 UV 격자를 표시한다. */
        SurfaceTexelGrid = 16,
        /** @brief 원본 삼각형의 월드 면적 / UV 면적 / 텍셀 수를 표시한다. */
        SurfaceTexelArea = 17,
        SurfaceAccumulation = 18,
        SurfaceFinalGeometry = 19,
        TotalSimulationHeight = 21
    };

    enum class TSolverTransferWeightView : std::uint32_t
    {
        Combined = 0,
        Distance,
        Normal,
        ProfileBoundary
    };

    struct TSurfaceDebugDisplaySettings
    {
        std::uint32_t AccumulationComponent = 0;
        float         DisplacementScale = 1.0F;
        // 0: shaded surface, 1: grid overlay, 2: grid on a dark surface.
        std::uint32_t HeightGridMode = 0;
        std::uint32_t HeightGridBlockSize = 8;
    };

    class TRenderer
    {
    public:
        // Renderer lifecycle
        TRenderer(const GPU::TVulkanContext&         Context,
                  TWindow&                           TWindow,
                  Asset::TAssetManager&              Assets,
                  SurfaceState::TSurfaceDataManager& SurfaceData,
                  const TScene&                      Scene,
                  SurfaceState::TSurfaceStateSystem& SurfaceStates);
        ~TRenderer();

        TRenderer(const TRenderer&) = delete;
        TRenderer& operator=(const TRenderer&) = delete;
        TRenderer(TRenderer&&) = delete;
        TRenderer& operator=(TRenderer&&) = delete;

        // Frame rendering and simulation clock
        /** @brief 이미지 획득, 명령 기록·제출, 화면 표시 순서로 한 프레임을 렌더링한다. */
        void                 RenderFrame(const TScene& SceneData,
                                         TDebugUI&     DebugInterface,
                                         float         DeltaTime,
                                         bool          bSuspendSimulationClock = false);
        [[nodiscard]] double GetPendingSimulationSeconds() const noexcept
        {
            return SimulationClock.GetPendingSeconds();
        }
        [[nodiscard]] double GetSimulatedSeconds() const noexcept
        {
            return SimulationClock.GetSimulatedSeconds();
        }
        [[nodiscard]] std::uint32_t GetLastSimulationStepCount() const noexcept
        {
            return LastSimulationStepCount;
        }
        [[nodiscard]] float GetMaximumSimulationStep() const noexcept
        {
            return MaximumSimulationStep;
        }

        // Scene resources, resolution, and runtime Profile settings
        /** @brief Rebuild the Scene Registry/resources; Scene changes also discard State-ID-based settings. */
        void ReloadSceneResources(const TScene& Scene, bool bResetStateSettings = true);
        /** @brief Reset simulation and caches while retaining loaded Scene/GPU resources. */
        void                        RestartSimulationState();
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        /** @brief Rebuild Surface mapping and GPU resources, resetting State on success. */
        void SetSimulationResolution(TScene& Scene, std::uint32_t Resolution);
        [[nodiscard]] std::uint32_t GetSurfaceTexelMeshResolution() const noexcept
        {
            return SurfaceTexelMeshResolution;
        }
        void SetSurfaceTexelMeshResolution(std::uint32_t Resolution);
        [[nodiscard]] std::uint32_t GetOverlayTexelMeshResolution() const noexcept
        {
            return OverlayTexelMeshResolution;
        }
        void SetOverlayTexelMeshResolution(std::uint32_t Resolution);
        void SetDebugProfileParameters(Asset::TSRProfileAssetHandle                 Profile,
                                       SurfaceState::TStateId                       State,
                                       const SurfaceState::TSurfaceStateParameters& Parameters,
                                       bool                                         bKeepRuntimeOverride = true);

        // Swapchain and profiling access
        [[nodiscard]] const GPU::TSwapchain&                          GetSwapchain() const noexcept;
        [[nodiscard]] VkRenderPass                                    GetRenderPassHandle() const noexcept;
        [[nodiscard]] const SurfaceState::TSurfaceGPUResourceManager& GetSurfaceGPUResources() const noexcept;
        /** @brief 마지막 완료 프레임에서 측정한 GPU Solver 시간(ms), 미지원 시 -1. */
        [[nodiscard]] float                          GetLastSolverGpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastRenderGpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastSolverPass1GpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastSolverPass2GpuMilliseconds() const noexcept;
        [[nodiscard]] const TRendererProfilingStats& GetProfilingStats() const noexcept
        {
            return ProfilingStats;
        }

        // Benchmark capture and overlay profiling
        void ConfigureBenchmarkCapture(const std::filesystem::path& OutputPath,
                                       std::uint32_t WarmupFrames,
                                       std::uint32_t MeasurementFrames);
        [[nodiscard]] TOverlayDisplayMode GetOverlayDisplayMode() const noexcept
        {
            return OverlayDisplayMode;
        }
        void SetOverlayDisplayMode(TOverlayDisplayMode Mode) noexcept
        {
            OverlayDisplayMode = Mode;
        }
        [[nodiscard]] bool IsOverlayOnlyDebugEnabled() const noexcept
        {
            return bOverlayOnlyDebug;
        }
        void SetOverlayOnlyDebugEnabled(bool bEnabled) noexcept
        {
            bOverlayOnlyDebug = bEnabled;
            if (!bEnabled)
                OverlayDisplayMode = TOverlayDisplayMode::Both;
        }
        [[nodiscard]] TOverlayOccupancyTileSize GetOverlayOccupancyTileSize() const noexcept
        {
            return OverlayOccupancyTileSize;
        }
        void               SetOverlayOccupancyTileSize(TOverlayOccupancyTileSize Size);
        [[nodiscard]] bool IsOverlayTileCullingEnabled() const noexcept
        {
            return bOverlayTileCullingEnabled;
        }
        void SetOverlayTileCullingEnabled(bool bEnabled) noexcept
        {
            bOverlayTileCullingEnabled = bEnabled;
        }
        [[nodiscard]] bool IsBaseMeshDrawEnabled() const noexcept { return bBaseMeshDrawEnabled; }
        void SetBaseMeshDrawEnabled(bool bEnabled) noexcept { bBaseMeshDrawEnabled = bEnabled; }
        [[nodiscard]] bool IsOverlayTopDrawEnabled() const noexcept { return bOverlayTopDrawEnabled; }
        void SetOverlayTopDrawEnabled(bool bEnabled) noexcept { bOverlayTopDrawEnabled = bEnabled; }
        [[nodiscard]] bool IsOverlaySidesDrawEnabled() const noexcept { return bOverlaySidesDrawEnabled; }
        void SetOverlaySidesDrawEnabled(bool bEnabled) noexcept { bOverlaySidesDrawEnabled = bEnabled; }
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept;
        void SetRawFluxCacheEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsCoalescedRawFluxLayoutEnabled() const noexcept;
        void SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsHalfRawFluxCacheEnabled() const noexcept;
        void SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsHalfDynamicWeightsEnabled() const noexcept;
        void SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSolverEnabled() const noexcept;
        void SetSparseSolverEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseAccumulationHeightEnabled() const noexcept;
        void SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsActiveChannelMaskEnabled() const noexcept;
        void SetActiveChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsPerWorkgroupChannelMaskEnabled() const noexcept;
        void SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSimulationGeometryEnabled() const noexcept;
        void SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseHeightSmoothingEnabled() const noexcept { return bSparseHeightSmoothingEnabled; }
        void SetSparseHeightSmoothingEnabled(bool bEnabled) noexcept { bSparseHeightSmoothingEnabled = bEnabled; }
        [[nodiscard]] bool IsPrecomputeCoverageSmoothingEnabled() const noexcept { return bPrecomputeCoverageSmoothingEnabled; }
        void SetPrecomputeCoverageSmoothingEnabled(bool bEnabled) noexcept { bPrecomputeCoverageSmoothingEnabled = bEnabled; }
        [[nodiscard]] bool IsRenderStateTextureSamplingEnabled() const noexcept { return bRenderStateTextureSamplingEnabled; }
        void SetRenderStateTextureSamplingEnabled(bool bEnabled) noexcept { bRenderStateTextureSamplingEnabled = bEnabled; }
        [[nodiscard]] bool IsSeparableCoverageSmoothingEnabled() const noexcept { return bSeparableCoverageSmoothingEnabled; }
        void SetSeparableCoverageSmoothingEnabled(bool bEnabled) noexcept { bSeparableCoverageSmoothingEnabled = bEnabled; }
        [[nodiscard]] bool AreRenderPassSubstageTimingsReliable() const noexcept
        {
            return bRenderPassSubstageTimingsReliable;
        }

        // Render view and debug settings
        [[nodiscard]] TRenderViewMode GetRenderViewMode() const noexcept;
        [[nodiscard]] TRenderViewMode GetRenderViewMode(std::size_t ViewportIndex) const noexcept;
        void               SetRenderViewMode(TRenderViewMode Mode);
        void               SetRenderViewMode(std::size_t ViewportIndex, TRenderViewMode Mode);
        [[nodiscard]] bool IsWireframeUniformWhite() const noexcept
        {
            return bWireframeUniformWhite;
        }
        void SetWireframeUniformWhite(bool bEnabled) noexcept
        {
            bWireframeUniformWhite = bEnabled;
        }
        [[nodiscard]] bool SupportsWireframeLineWidth() const noexcept
        {
            return bSupportsWireframeLineWidth;
        }
        [[nodiscard]] float GetWireframeLineWidth() const noexcept
        {
            return WireframeLineWidth;
        }
        [[nodiscard]] float GetWireframeLineWidthMin() const noexcept
        {
            return WireframeLineWidthMin;
        }
        [[nodiscard]] float GetWireframeLineWidthMax() const noexcept
        {
            return WireframeLineWidthMax;
        }
        void                                              SetWireframeLineWidth(float Width) noexcept;
        [[nodiscard]] bool                                IsWorldGridVisible() const noexcept;
        void                                              SetWorldGridVisible(bool bVisible) noexcept;
        [[nodiscard]] bool                                IsWorldAxisVisible() const noexcept;
        void                                              SetWorldAxisVisible(bool bVisible) noexcept;
        [[nodiscard]] std::uint32_t                       GetDebugStateChannel() const noexcept;
        void                                              SetDebugStateChannel(std::uint32_t Channel);
        [[nodiscard]] bool                                IsStateHeatmapReliefShadingEnabled() const noexcept;
        void                                              SetStateHeatmapReliefShadingEnabled(bool bEnabled);
        [[nodiscard]] const TSurfaceDebugDisplaySettings& GetSurfaceDebugDisplaySettings() const noexcept
        {
            return SurfaceDebugSettings;
        }
        void SetSurfaceDebugDisplaySettings(const TSurfaceDebugDisplaySettings& Settings);
        [[nodiscard]] const TDemoSurfaceEffectSettings& GetDemoSurfaceEffectSettings() const noexcept
        {
            return DemoEffects;
        }
        void               SetDemoSurfaceEffectSettings(const TDemoSurfaceEffectSettings& Settings);
        [[nodiscard]] bool IsLitTexelMeshBaseRendered(const TScene& Scene, std::size_t Instance) const;
        void               SetSceneLitHeightDisplayScale(TScene& Scene, float Scale);
        [[nodiscard]] TDemoSurfaceStateBindings GetDemoSurfaceStateBindings() const;

        [[nodiscard]] bool
             InspectTexel(const TScene& Scene, std::size_t Instance, std::uint32_t Triangle, glm::vec2 UV);
        void ClearInspectedTexel() noexcept;
        [[nodiscard]] const std::optional<SurfaceState::TSurfaceTexelSelection>& GetInspectedTexel() const noexcept
        {
            return InspectedTexel;
        }
        [[nodiscard]] const std::optional<SurfaceState::TSurfaceTexelSnapshot>& GetTexelSnapshot() const noexcept;
        [[nodiscard]] TSolverTransferWeightView GetSolverTransferWeightView() const noexcept;
        void                                    SetSolverTransferWeightView(TSolverTransferWeightView View);
        [[nodiscard]] std::uint32_t             GetTexelGridBlockSize() const noexcept;
        void                                    SetTexelGridBlockSize(std::uint32_t Size);
        [[nodiscard]] float                     GetTexelAreaReference() const noexcept;
        /** @brief 해상도·Scene 전환과 독립적인 기준 면적(world units²/texel)을 설정한다. */
        void               SetTexelAreaReference(float Area);
        [[nodiscard]] bool IsDebugGeometryDriveEnabled() const noexcept;
        void               SetDebugGeometryDriveEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugNormalWeightEnabled() const noexcept;
        void               SetDebugNormalWeightEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm Term) const noexcept;
        void               SetDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm Term, bool bEnabled);
        [[nodiscard]] bool IsAccumulationGeometryUpdateEnabled() const noexcept;
        void               SetAccumulationGeometryUpdateEnabled(bool bEnabled);

        [[nodiscard]] bool GetFlipNormalY() const noexcept;
        void               SetFlipNormalY(bool bEnabled);

        [[nodiscard]] float GetNormalStrength() const noexcept;
        void                SetNormalStrength(float Strength);

        [[nodiscard]] float GetAmbientLight() const noexcept;
        void                SetAmbientLight(float Intensity);

    private:
        struct TBenchmarkFrameSubmission
        {
            std::uint64_t FrameIndex = std::numeric_limits<std::uint64_t>::max();
            std::uint32_t SimulationSteps = 0;
            std::uint32_t Resolution = 0;
            std::uint32_t SurfaceMeshResolution = 0;
            std::uint32_t OverlayMeshResolution = 0;
            std::uint32_t Instances = 0;
            std::uint32_t StateChannels = 0;
            std::uint64_t Texels = 0;
            float         SimulatedSeconds = 0.0F;
        };

        void WriteBenchmarkSample(std::uint32_t FrameSlot);


        struct TMaterialRenderResource
        {
            std::array<std::unique_ptr<GPU::TGPUBuffer>, TRenderContext::MaxFramesInFlight> UniformBuffers;
            std::array<VkDescriptorSet, TRenderContext::MaxFramesInFlight>                  DescriptorSets{};
        };

        // Render resources and command recording
        static VkFormat              FindDepthFormat(VkPhysicalDevice PhysicalDevice);
        static VkFormat              FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
                                                         const VkFormat*      Candidates,
                                                         std::uint32_t        CandidateCount,
                                                         VkImageTiling        Tiling,
                                                         VkFormatFeatureFlags Features);
        static VkDescriptorSetLayout CreateMaterialDescriptorSetLayout(VkDevice Device);

        void                CreateMaterialDescriptorResources();
        void                CreateRenderFinishedSemaphores();
        void                DestroyRenderFinishedSemaphores() noexcept;
        void                CreateTimestampQueryPool(std::size_t SolverInstanceCount);
        void                UploadMaterialUniforms(std::uint32_t   Frame,
                                                   const TScene&   SceneData,
                                                   const TDebugUI& DebugInterface,
                                                   float           LitHeightDisplayScale);
        [[nodiscard]] float GetDebugViewParameter(TRenderViewMode Mode) const noexcept;
        [[nodiscard]] bool  CanRenderLitOverlays() const noexcept;
        [[nodiscard]] std::array<bool, 3>
             GetLitOverlayActivity(const TStaticMeshInstance&                              Instance,
                                   const SurfaceState::TSurfaceSharedGeometryGPUResources* Shared,
                                   const TDemoSurfaceStateBindings&                        Bindings) const;
        void RecreateSwapchain(TDebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer        CommandBuffer,
                                 std::uint32_t          ImageIndex,
                                 const TScene&          SceneData,
                                 const TDebugUI&        DebugInterface,
                                 std::span<const float> SimulationSteps);

        const GPU::TVulkanContext&         Context;
        TWindow&                           TargetWindow;
        Asset::TAssetManager&              Assets;
        SurfaceState::TSurfaceDataManager& SurfaceData;
        /** @brief Application이 소유하며 Renderer는 compute 기록과 렌더링 동안만 참조한다. */
        SurfaceState::TSurfaceStateSystem&                   SurfaceStates;
        GPU::TSwapchain                                      SwapchainData;
        VkFormat                                             DepthFormat = VK_FORMAT_UNDEFINED;
        std::vector<std::unique_ptr<GPU::TGPUImage>>         DepthImages;
        std::vector<std::unique_ptr<GPU::TGPUImageView>>     DepthImageViews;
        GPU::TRenderPass                                     MainRenderPass;
        GPU::TGraphicsPass                                   MainGraphicsPass;
        VkDescriptorSetLayout                                MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        GPU::TGraphicsPipeline                               StaticMeshPipeline;
        GPU::TGraphicsPipeline                               WireframePipeline;
        GPU::TGraphicsPipeline                               GizmoPipeline;
        GPU::TGraphicsPipeline                               WorldReferencePipeline;
        std::unique_ptr<GPU::TGPUBuffer>                     GizmoVertexBuffer;
        std::uint32_t                                        WorldGridVertexCount = 0;
        std::uint32_t                                        WorldAxisVertexCount = 0;
        std::uint32_t                                        TranslateGizmoVertexCount = 0;
        std::uint32_t                                        RotateGizmoVertexCount = 0;
        bool                                                 bWorldGridVisible = true;
        bool                                                 bWorldAxisVisible = true;
        std::unique_ptr<GPU::TGraphicsPipeline>              SurfaceDebugPipeline;
        std::unique_ptr<SurfaceState::TTexelGeometryPreview> TexelGeometryPreview;
        std::unique_ptr<GPU::TGraphicsPipeline>              TexelGeometryPipeline;
        std::unique_ptr<GPU::TGraphicsPipeline>              SurfaceLitPipeline;
        std::unique_ptr<GPU::TGraphicsPipeline>              BaseSurfaceLitPipeline;
        std::unique_ptr<TRenderStateTexture>                  RenderStateTexture;
        std::unique_ptr<SurfaceState::TTexelGeometryPreview> MudLayerGeometry;
        std::unique_ptr<THeightFieldSmoothing>               HeightFieldSmoothing;
        std::unique_ptr<TAccumulationOverlaySides>           OverlaySides;
        std::unique_ptr<GPU::TGraphicsPipeline>              MudOverlayTopPipeline;
        std::unique_ptr<GPU::TGraphicsPipeline>              MudOverlaySidePipeline;
        GPU::TFramebuffer                                    MainFramebuffers;
        TRenderContext                                       FrameContext;
        VkDescriptorPool                                     MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<TMaterialRenderResource>                 MaterialResources;
        VkDeviceSize                                         MaterialUniformStride = 0;
        std::size_t                                          MaterialViewportCapacity = 1;
        std::vector<TRenderViewMode>                         ViewModes{TRenderViewMode::Lit};
        bool                                                 bWireframeUniformWhite = true;
        bool                                                 bSupportsWireframeLineWidth = false;
        float                                                WireframeLineWidth = 2.0F;
        float                                                WireframeLineWidthMin = 1.0F;
        float                                                WireframeLineWidthMax = 1.0F;
        std::uint32_t                                        DebugStateChannel = 0;
        bool                                                 bStateHeatmapReliefShadingEnabled = true;
        TSurfaceDebugDisplaySettings                         SurfaceDebugSettings;
        TDemoSurfaceEffectSettings                           DemoEffects;
        TOverlayDisplayMode                                  OverlayDisplayMode = TOverlayDisplayMode::Both;
        bool                                                 bOverlayOnlyDebug = false;
        bool                                                 bOverlayTileCullingEnabled = true;
        bool                                                 bBaseMeshDrawEnabled = true;
        bool                                                 bOverlayTopDrawEnabled = true;
        bool                                                 bOverlaySidesDrawEnabled = true;
        bool                                                 bSparseHeightSmoothingEnabled = true;
        bool                                                 bPrecomputeCoverageSmoothingEnabled = true;
        bool                                                 bRenderStateTextureSamplingEnabled = true;
        bool                                                 bSeparableCoverageSmoothingEnabled = true;
        TOverlayOccupancyTileSize OverlayOccupancyTileSize = TOverlayOccupancyTileSize::Tile16;
        std::uint32_t             SurfaceTexelMeshResolution = SurfaceState::SurfaceSimulationResolution;
        std::uint32_t             OverlayTexelMeshResolution = SurfaceState::SurfaceSimulationResolution;
        std::optional<SurfaceState::TSurfaceTexelSelection> InspectedTexel;
        std::unique_ptr<SurfaceState::TTexelInspector>      TexelInspector;
        std::uint64_t                                       SimulationStepSerial = 0;
        TSolverTransferWeightView                 SolverTransferWeightView = TSolverTransferWeightView::Combined;
        std::uint32_t                             TexelGridBlockSize = 8;
        float                                     TexelAreaReference = 1.0e-4F;
        SurfaceState::TSurfaceSolverDebugSettings DebugSolverSettings;
        bool                                      bFlipNormalY = true;
        float                                     NormalStrength = 1.0F;
        float                                     AmbientLight = 0.25F;
        std::map<std::pair<Asset::TSRProfileAssetHandle, SurfaceState::TStateId>, SurfaceState::TSurfaceStateParameters>
                                                                                 DebugProfileParameterOverrides;
        std::vector<VkSemaphore>                                                 RenderFinishedSemaphores;
        VkQueryPool                                                              TimestampQueryPool = VK_NULL_HANDLE;
        std::array<bool, TRenderContext::MaxFramesInFlight>                      bTimestampQueriesSubmitted{};
        std::array<std::uint32_t, TRenderContext::MaxFramesInFlight>             SolverTimestampStepsSubmitted{};
        std::array<std::uint32_t, TRenderContext::MaxFramesInFlight>             OverlayTimestampLayersSubmitted{};
        std::array<TOverlayDisplayMode, TRenderContext::MaxFramesInFlight> OverlayDisplayModesSubmitted{};
        std::array<TBenchmarkFrameSubmission, TRenderContext::MaxFramesInFlight> BenchmarkSubmissions{};
        std::ofstream      BenchmarkOutput;
        std::vector<std::string> BenchmarkJsonLines;
        std::uint64_t      BenchmarkWarmupFrames = 0;
        std::uint64_t      BenchmarkMeasurementFrames = 0;
        std::uint64_t      NextBenchmarkFrameIndex = 0;
        bool               bBenchmarkCaptureEnabled = false;
        SurfaceState::TSimulationClock                                           SimulationClock;
        std::uint32_t                                                            LastSimulationStepCount = 0;
        float                   MaximumSimulationStep = SurfaceState::FixedSimulationStepSeconds;
        std::uint32_t           TimestampQueriesPerFrame = 21;
        std::uint32_t           SolverTimestampGroupCount = 0;
        float                   TimestampPeriodNanoseconds = 0.0F;
        std::uint32_t           TimestampValidBits = 0;
        bool                    bRenderPassSubstageTimingsReliable = true;
        float                   LastRenderGpuMilliseconds = -1.0F;
        float                   LastSolverGpuMilliseconds = -1.0F;
        float                   LastSolverPass1GpuMilliseconds = -1.0F;
        float                   LastSolverPass2GpuMilliseconds = -1.0F;
        TRendererProfilingStats ProfilingStats;
    };
} // MDSS 네임스페이스
