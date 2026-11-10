/**
 * @file Renderer.h
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#pragma once

#include "Application/FrameCoordinator.h"
#include "AssetManager/Core/Asset.h"
#include "GPU/Pass/GraphicsPass.h"
#include "GPU/Vulkan/Pipeline/GraphicsPipeline.h"
#include "GPU/Vulkan/Render/Framebuffer.h"
#include "GPU/Vulkan/Render/RenderPass.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "GPU/Vulkan/Resource/GPUImage.h"
#include "GPU/Vulkan/Resource/GPUImageView.h"
#include "GPU/Vulkan/Swapchain/Swapchain.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Core/RenderSettings.h"
#include "Rendering/Overlay/AccumulationOverlaySides.h"
#include "Rendering/Overlay/Geometry/TexelGeometryPreview.h"
#include "Rendering/Surface/DemoSurfaceEffects.h"
#include "Rendering/Surface/HeightFieldSmoothing.h"
#include "Rendering/Surface/RenderStateTexture.h"
#include "SurfaceState/Debug/TexelInspector.h"
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
    class FAssetManager;
}

namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
}

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    class FWindow;
}

namespace MDSS
{
    class FScene;
    class FDebugUI;
    class FStaticMeshInstance;
}

namespace MDSS::Rendering
{
    class FRenderer
    {
    public:
// ============================================================
// 00. Renderer lifecycle
// ============================================================
#pragma region "00_Renderer_lifecycle"
// 외부 소유 자원을 빌리고 Renderer 소유 자원의 생성·해제를 선언한다.

        // Renderer lifecycle
        FRenderer(const GPU::FVulkanContext&         Context,
                  FWindow&                           FWindow,
                  Asset::FAssetManager&              Assets,
                  SurfaceState::FSurfaceDataManager& SurfaceData,
                  const FScene&                      Scene,
                  SurfaceState::FSurfaceStateSystem& SurfaceStateSystem,
                  FFrameCoordinator&                 FrameCoordinator);
        ~FRenderer();

        FRenderer(const FRenderer&) = delete;
        FRenderer& operator=(const FRenderer&) = delete;
        FRenderer(FRenderer&&) = delete;
        FRenderer& operator=(FRenderer&&) = delete;

#pragma endregion

// ============================================================
// 01. Frame rendering and simulation clock
// ============================================================
#pragma region "01_Frame_rendering_and_simulation_clock"
// 프레임 렌더링과 Solver 시간 진행을 Application의 호출 경계에 연결한다.

        // Frame rendering and simulation clock
        /** @brief 이미지 획득, 명령 기록·제출, 화면 표시 순서로 한 프레임을 렌더링한다. */
        void                 RenderFrame(const FScene& SceneData,
                                         FDebugUI&     DebugInterface,
                                         float         DeltaTime,
                                         bool          bSuspendSimulationClock = false);
        [[nodiscard]] double GetPendingSimulationSeconds() const noexcept
        {
            return FrameCoordinator.GetPendingSeconds();
        }

        [[nodiscard]] double GetSimulatedSeconds() const noexcept
        {
            return FrameCoordinator.GetSimulatedSeconds();
        }

        [[nodiscard]] std::uint32_t GetLastSimulationStepCount() const noexcept
        {
            return FrameCoordinator.GetStepCount();
        }

        [[nodiscard]] float GetMaximumSimulationStep() const noexcept
        {
            return FrameCoordinator.GetMaximumStep();
        }

#pragma endregion

// ============================================================
// 02. Scene resources, resolution, and runtime Profile settings
// ============================================================
#pragma region "02_Scene_resources_resolution_and_runtime_Profile_settings"
// Scene·해상도·Profile 변경을 해당 자원 갱신 경계로 전달한다.

        // Scene resources, resolution, and runtime Profile settings
        /** @brief Rebuild the Scene Registry/resources; Scene changes also discard State-ID-based settings. */
        void ReloadSceneResources(const FScene& Scene, bool bResetStateSettings = true);
        /** @brief Reset simulation and caches while retaining loaded Scene/GPU resources. */
        void                        RestartSimulationState();
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        /** @brief Rebuild Surface mapping and GPU resources, resetting State on success. */
        void                        SetSimulationResolution(FScene& Scene, std::uint32_t Resolution);
        [[nodiscard]] std::uint32_t GetSurfaceTexelMeshResolution() const noexcept
        {
            return SurfaceTexelMeshResolution;
        }

        void                        SetSurfaceTexelMeshResolution(std::uint32_t Resolution);
        [[nodiscard]] std::uint32_t GetOverlayTexelMeshResolution() const noexcept
        {
            return OverlayTexelMeshResolution;
        }

        void SetOverlayTexelMeshResolution(std::uint32_t Resolution);
        void SetDebugProfileParameters(Asset::FSRProfileAssetHandle                 Profile,
                                       SurfaceState::FStateId                       State,
                                       const SurfaceState::FSurfaceStateParameters& Parameters,
                                       bool                                         bKeepRuntimeOverride = true);

#pragma endregion

// ============================================================
// 03. Swapchain and profiling access
// ============================================================
#pragma region "03_Swapchain_and_profiling_access"
// UI가 조회하는 swapchain과 완료 frame의 측정 결과를 제공한다.

        // Swapchain and profiling access
        [[nodiscard]] const GPU::FSwapchain&                          GetSwapchain() const noexcept;
        [[nodiscard]] VkRenderPass                                    GetRenderPassHandle() const noexcept;
        [[nodiscard]] const SurfaceState::FSurfaceGPUResourceManager& GetSurfaceGPUResources() const noexcept;
        /** @brief 마지막 완료 프레임에서 측정한 GPU Solver 시간(ms), 미지원 시 -1. */
        [[nodiscard]] float                          GetLastSolverGpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastRenderGpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastSolverPass1GpuMilliseconds() const noexcept;
        [[nodiscard]] float                          GetLastSolverPass2GpuMilliseconds() const noexcept;
        [[nodiscard]] const FRendererProfilingStats& GetProfilingStats() const noexcept
        {
            return ProfilingStats;
        }

#pragma endregion

// ============================================================
// 04. Benchmark capture and overlay profiling
// ============================================================
#pragma region "04_Benchmark_capture_and_overlay_profiling"
// 결과 출력과 overlay 실행·표시 설정을 외부 제어에 연결한다.

        // 04.00. Benchmark Capture
        #pragma region "04_00_Benchmark_Capture"
        // 프레임 측정과 선택적 checkpoint 출력을 설정하고 종료 후 결과를 확정한다.

        /**
         * @brief benchmark 출력과 선택적 authoring checkpoint 캡처를 설정한다.
         * @param OutputPath 프레임 기록 및 추가 캡처 파일의 기준 경로.
         * @param WarmupFrames 측정 전에 제외할 프레임 수.
         * @param MeasurementFrames 기록할 측정 프레임 수. 0보다 커야 한다.
         * @param AuthoringScene 초기 입력 관찰자가 참조할 Scene. checkpoint 사용 시 필수다.
         * @pre AuthoringScene은 초기 입력 관찰자가 실행될 때까지 유효해야 한다.
         * @throws std::invalid_argument 출력 경로·프레임 수·checkpoint 설정이 유효하지 않을 때.
         * @throws std::runtime_error timestamp query를 지원하지 않거나 출력 파일을 열 수 없을 때.
         */
        void ConfigureBenchmarkCapture(const std::filesystem::path& OutputPath,
                                       std::uint32_t                WarmupFrames,
                                       std::uint32_t                MeasurementFrames,
                                       const FScene*                AuthoringScene = nullptr);
        // Called only after the application has waited for the device to become idle.
        void                              FinalizeBenchmarkCapture();
        void                              RecordBenchmarkScreenshot(VkCommandBuffer Command, std::uint32_t ImageIndex);
        #pragma endregion

        // 04.01. Overlay Display Controls
        #pragma region "04_01_Overlay_Display_Controls"
        // 계산 결과를 바꾸지 않는 overlay 표시·진단 선택을 관리한다.

        [[nodiscard]] EOverlayDisplayMode GetOverlayDisplayMode() const noexcept
        {
            return OverlayDisplayMode;
        }

        void SetOverlayDisplayMode(EOverlayDisplayMode Mode) noexcept
        {
            OverlayDisplayMode = Mode;
        }

        [[nodiscard]] EOverlayShadingDiagnostic GetOverlayShadingDiagnostic() const noexcept
        {
            return OverlayShadingDiagnostic;
        }

        void SetOverlayShadingDiagnostic(EOverlayShadingDiagnostic Mode) noexcept
        {
            OverlayShadingDiagnostic = Mode;
        }

        [[nodiscard]] bool IsOverlayOnlyDebugEnabled() const noexcept
        {
            return bOverlayOnlyDebug;
        }

        void SetOverlayOnlyDebugEnabled(bool bEnabled) noexcept
        {
            bOverlayOnlyDebug = bEnabled;

            if (!bEnabled)
                OverlayDisplayMode = EOverlayDisplayMode::Both;
        }

        [[nodiscard]] EOverlayOccupancyTileSize GetOverlayOccupancyTileSize() const noexcept
        {
            return OverlayOccupancyTileSize;
        }

        void               SetOverlayOccupancyTileSize(EOverlayOccupancyTileSize Size);
        [[nodiscard]] bool IsOverlayTileCullingEnabled() const noexcept
        {
            return bOverlayTileCullingEnabled;
        }

        void SetOverlayTileCullingEnabled(bool bEnabled) noexcept
        {
            bOverlayTileCullingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsBaseMeshDrawEnabled() const noexcept
        {
            return bBaseMeshDrawEnabled;
        }

        void SetBaseMeshDrawEnabled(bool bEnabled) noexcept
        {
            bBaseMeshDrawEnabled = bEnabled;
        }

        [[nodiscard]] bool IsOverlayTopDrawEnabled() const noexcept
        {
            return bOverlayTopDrawEnabled;
        }

        void SetOverlayTopDrawEnabled(bool bEnabled) noexcept
        {
            bOverlayTopDrawEnabled = bEnabled;
        }

        [[nodiscard]] bool IsOverlaySidesDrawEnabled() const noexcept
        {
            return bOverlaySidesDrawEnabled;
        }

        void SetOverlaySidesDrawEnabled(bool bEnabled) noexcept
        {
            bOverlaySidesDrawEnabled = bEnabled;
        }

        #pragma endregion

        // 04.02. Runtime Performance Controls
        #pragma region "04_02_Runtime_Performance_Controls"
        // 실행 경로·저장 형식 옵션을 관련 Solver·표시 자원에 전달한다.

        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept;
        void               SetRawFluxCacheEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsCoalescedRawFluxLayoutEnabled() const noexcept;
        void               SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsHalfRawFluxCacheEnabled() const noexcept;
        void               SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsHalfDynamicWeightsEnabled() const noexcept;
        void               SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSolverEnabled() const noexcept;
        void               SetSparseSolverEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseAccumulationHeightEnabled() const noexcept;
        void               SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsActiveChannelMaskEnabled() const noexcept;
        void               SetActiveChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsPerWorkgroupChannelMaskEnabled() const noexcept;
        void               SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSimulationGeometryEnabled() const noexcept;
        void               SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseHeightSmoothingEnabled() const noexcept
        {
            return bSparseHeightSmoothingEnabled;
        }

        void SetSparseHeightSmoothingEnabled(bool bEnabled) noexcept
        {
            bSparseHeightSmoothingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsSharedHeightSmoothingEnabled() const noexcept
        {
            return bSharedHeightSmoothingEnabled;
        }

        void SetSharedHeightSmoothingEnabled(bool bEnabled) noexcept
        {
            bSharedHeightSmoothingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsSharedOverlayCoverageEnabled() const noexcept
        {
            return bSharedOverlayCoverageEnabled;
        }

        void SetSharedOverlayCoverageEnabled(bool bEnabled) noexcept
        {
            bSharedOverlayCoverageEnabled = bEnabled;
        }

        [[nodiscard]] bool IsSharedStateTextureSmoothingEnabled() const noexcept
        {
            return bSharedStateTextureSmoothingEnabled;
        }

        void SetSharedStateTextureSmoothingEnabled(bool bEnabled) noexcept
        {
            bSharedStateTextureSmoothingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsPrecomputeCoverageSmoothingEnabled() const noexcept
        {
            return bPrecomputeCoverageSmoothingEnabled;
        }

        void SetPrecomputeCoverageSmoothingEnabled(bool bEnabled) noexcept
        {
            bPrecomputeCoverageSmoothingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsRenderStateTextureSamplingEnabled() const noexcept
        {
            return bRenderStateTextureSamplingEnabled;
        }

        void SetRenderStateTextureSamplingEnabled(bool bEnabled) noexcept
        {
            bRenderStateTextureSamplingEnabled = bEnabled;
        }

        [[nodiscard]] bool IsSeparableCoverageSmoothingEnabled() const noexcept
        {
            return bSeparableCoverageSmoothingEnabled;
        }

        void SetSeparableCoverageSmoothingEnabled(bool bEnabled) noexcept
        {
            bSeparableCoverageSmoothingEnabled = bEnabled;
        }

        [[nodiscard]] bool AreRenderPassSubstageTimingsReliable() const noexcept
        {
            return bRenderPassSubstageTimingsReliable;
        }
        #pragma endregion


#pragma endregion

// ============================================================
// 05. Render view and debug settings
// ============================================================
#pragma region "05_Render_view_and_debug_settings"
// view별 진단·재질 표시 설정을 기록 단계에서 읽을 상태로 유지한다.

        // Render view and debug settings
        [[nodiscard]] ERenderViewMode GetRenderViewMode() const noexcept;
        [[nodiscard]] ERenderViewMode GetRenderViewMode(std::size_t ViewportIndex) const noexcept;
        void                          SetRenderViewMode(ERenderViewMode Mode);
        void                          SetRenderViewMode(std::size_t ViewportIndex, ERenderViewMode Mode);
        [[nodiscard]] bool            IsWireframeUniformWhite() const noexcept
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
        [[nodiscard]] bool                                IsStateHeatmapAllStatesEnabled() const noexcept;
        void                                              SetStateHeatmapAllStatesEnabled(bool bEnabled) noexcept;
        [[nodiscard]] const FSurfaceDebugDisplaySettings& GetSurfaceDebugDisplaySettings() const noexcept
        {
            return SurfaceDebugSettings;
        }

        void SetSurfaceDebugDisplaySettings(const FSurfaceDebugDisplaySettings& Settings);
        [[nodiscard]] const FDemoSurfaceEffectSettings& GetDemoSurfaceEffectSettings() const noexcept
        {
            return DemoEffects;
        }

        void               SetDemoSurfaceEffectSettings(const FDemoSurfaceEffectSettings& Settings);
        [[nodiscard]] bool IsLitTexelMeshBaseRendered(const FScene& Scene, std::size_t Instance) const;
        void               SetSceneLitHeightDisplayScale(FScene& Scene, float Scale);
        [[nodiscard]] FDemoSurfaceStateBindings GetDemoSurfaceStateBindings() const;

        [[nodiscard]] bool
             InspectTexel(const FScene& Scene, std::size_t Instance, std::uint32_t Triangle, glm::vec2 UV);
        void ClearInspectedTexel() noexcept;
        [[nodiscard]] const std::optional<SurfaceState::FSurfaceTexelSelection>& GetInspectedTexel() const noexcept
        {
            return InspectedTexel;
        }

        [[nodiscard]] const std::optional<SurfaceState::FSurfaceTexelSnapshot>& GetTexelSnapshot() const noexcept;
        [[nodiscard]] ESolverTransferWeightView GetSolverTransferWeightView() const noexcept;
        void                                    SetSolverTransferWeightView(ESolverTransferWeightView View);
        [[nodiscard]] std::uint32_t             GetTexelGridBlockSize() const noexcept;
        void                                    SetTexelGridBlockSize(std::uint32_t Size);
        [[nodiscard]] float                     GetTexelAreaReference() const noexcept;
        /** @brief 해상도·Scene 전환과 독립적인 기준 면적(world units²/texel)을 설정한다. */
        void               SetTexelAreaReference(float Area);
        [[nodiscard]] bool IsDebugGeometryDriveEnabled() const noexcept;
        void               SetDebugGeometryDriveEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugNormalWeightEnabled() const noexcept;
        void               SetDebugNormalWeightEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm Term) const noexcept;
        void               SetDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm Term, bool bEnabled);
        [[nodiscard]] bool IsStateDrivenGeometryUpdateEnabled() const noexcept;
        void               SetStateDrivenGeometryUpdateEnabled(bool bEnabled);

        [[nodiscard]] bool GetFlipNormalY() const noexcept;
        void               SetFlipNormalY(bool bEnabled);

        [[nodiscard]] float GetNormalStrength() const noexcept;
        void                SetNormalStrength(float Strength);

        [[nodiscard]] float GetAmbientLight() const noexcept;
        void                SetAmbientLight(float Intensity);

    private:
#pragma endregion

// ============================================================
// 06. Benchmark Submission and Material Records
// ============================================================
#pragma region "06_Benchmark_Submission_and_Material_Records"
// in-flight 측정 record와 material 자원이 유지할 필드 형식을 정의한다.

        struct FBenchmarkFrameSubmission
        {
            std::uint64_t        FrameIndex = std::numeric_limits<std::uint64_t>::max();
            std::uint32_t        SimulationSteps = 0;
            std::uint32_t        Resolution = 0;
            std::uint32_t        SurfaceMeshResolution = 0;
            std::uint32_t        OverlayMeshResolution = 0;
            std::uint32_t        Instances = 0;
            std::uint32_t        StateChannels = 0;
            std::uint64_t        Texels = 0;
            float                SimulatedSeconds = 0.0F;
            std::array<float, 5> CpuMilliseconds{};
        };

        void CollectCompletedFrameTimings(std::uint32_t FrameIndex);
        void WriteBenchmarkSample(std::uint32_t FrameSlot);
        void WriteAuthoringCheckpoint(const FScene& SceneData, bool bInitialInput = false);
        void WriteBenchmarkMetadata(const FScene& SceneData, const FDebugUI& DebugInterface);

        struct FMaterialRenderResource
        {
            std::array<std::unique_ptr<GPU::FGPUBuffer>, FRenderContext::MaxFramesInFlight> UniformBuffers;
            std::array<VkDescriptorSet, FRenderContext::MaxFramesInFlight>                  DescriptorSets{};
        };

#pragma endregion

// ============================================================
// 07. Render resources and command recording
// ============================================================
#pragma region "07_Render_resources_and_command_recording"
// frame·viewport 문맥을 전달해 pass별 명령 기록을 조율한다.

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
        void                TrimRenderResolutionCaches();
        struct FSceneReplacementResources;
        void BuildSceneReplacement(const FScene& Scene, bool bResetStateSettings, FSceneReplacementResources& Resources);
        void BuildSceneRenderPipelines(const FScene& Scene, FSceneReplacementResources& Resources);
        void CommitSceneReplacement(FSceneReplacementResources& Resources);
        void WaitForSceneReloadAndUpdateMaterials();
        void ConfigureReplacementSolver(SurfaceState::FSurfaceStateSystem& Replacement, bool bResetStateSettings);
        void ReleaseSceneRenderObjects();
        void ResetSceneGpuProfiling();
        void                UploadMaterialUniforms(std::uint32_t   Frame,
                                                   const FScene&   SceneData,
                                                   const FDebugUI& DebugInterface,
                                                   float           LitHeightDisplayScale);
        [[nodiscard]] float GetDebugViewParameter(ERenderViewMode Mode) const noexcept;
        [[nodiscard]] bool  CanRenderLitOverlays() const noexcept;
        [[nodiscard]] std::array<bool, 3>
             GetLitOverlayActivity(const FStaticMeshInstance&                              Instance,
                                   const SurfaceState::FSurfaceSharedGeometryGPUResources* Shared,
                                   const FDemoSurfaceStateBindings&                        Bindings) const;
        // Frame execution stages: the public RenderFrame reads as the frame timeline.
        void CompletePreviousFrame(std::uint32_t FrameIndex, const FDebugUI& DebugInterface);
        [[nodiscard]] bool AcquireFrameImage(FDebugUI& DebugInterface,
                                             std::uint32_t& ImageIndex,
                                             VkResult& AcquireResult);
        [[nodiscard]] VkCommandBuffer RecordFrameCommands(std::uint32_t FrameIndex,
                                                          std::uint32_t ImageIndex,
                                                          const FScene& SceneData,
                                                          FDebugUI& DebugInterface,
                                                          bool bResetSolverState);
        [[nodiscard]] VkSemaphore SubmitFrameCommands(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex);
        [[nodiscard]] bool PresentFrameImage(std::uint32_t ImageIndex,
                                             VkResult AcquireResult,
                                             VkSemaphore SignalSemaphore);
        void FinalizeFrameBenchmark(std::uint32_t FrameIndex,
                                    const FScene& SceneData,
                                    const FDebugUI& DebugInterface);

        void RecreateSwapchain(FDebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer        CommandBuffer,
                                 std::uint32_t          ImageIndex,
                                 const FScene&          SceneData,
                                 const FDebugUI&        DebugInterface,
                                 std::span<const float> SimulationSteps);

        struct FCommandRecordingState;
        struct FViewportRecordingState;
        [[nodiscard]] FCommandRecordingState  BeginCommandRecording(VkCommandBuffer        CommandBuffer,
                                                                    const FScene&          SceneData,
                                                                    const FDebugUI&        DebugInterface,
                                                                    std::span<const float> SimulationSteps);
        void                                  RecordSimulationAndInspector(VkCommandBuffer               CommandBuffer,
                                                                           const FScene&                 SceneData,
                                                                           const FCommandRecordingState& Frame);
        void                                  PrepareDisplayResources(VkCommandBuffer         CommandBuffer,
                                                                      const FScene&           SceneData,
                                                                      const FDebugUI&         DebugInterface,
                                                                      FCommandRecordingState& Frame);
        [[nodiscard]] std::vector<ERenderViewMode> CollectDisplayViewModes(const FDebugUI& DebugInterface,
                                                                           bool& bAnyTexelGeometry,
                                                                           bool& bAnyLitView) const;
        void RecordInstanceDisplayResources(VkCommandBuffer CommandBuffer,
                                            const FScene& SceneData,
                                            FCommandRecordingState& Frame,
                                            const FDemoSurfaceStateBindings& DemoBindings,
                                            bool bAnyTexelGeometry,
                                            bool bAnyLitView);
        void RecordTexelGeometryViews(VkCommandBuffer CommandBuffer,
                                      std::size_t Instance,
                                      const FCommandRecordingState& Frame,
                                      const SurfaceState::FSurfaceSharedGeometryGPUResources& Shared,
                                      const SurfaceState::FSurfaceStateDescriptorResources& Descriptors,
                                      std::uint32_t ChannelCount,
                                      const glm::mat4& Model,
                                      bool bStateAB);
        void                                  BeginSceneGraphicsPass(VkCommandBuffer               CommandBuffer,
                                                                     std::uint32_t                 ImageIndex,
                                                                     const FCommandRecordingState& Frame);
        void                                  RecordSceneViewports(VkCommandBuffer               CommandBuffer,
                                                                   const FScene&                 SceneData,
                                                                   const FDebugUI&               DebugInterface,
                                                                   const FCommandRecordingState& Frame);
        void                                  RecordUIAndEndCommandBuffer(VkCommandBuffer               CommandBuffer,
                                                                          std::uint32_t                 ImageIndex,
                                                                          const FDebugUI&               DebugInterface,
                                                                          const FCommandRecordingState& Frame);
        [[nodiscard]] FViewportRecordingState ConfigureViewport(VkCommandBuffer               CommandBuffer,
                                                                const FScene&                 SceneData,
                                                                const FDebugUI&               DebugInterface,
                                                                const FCommandRecordingState& Frame,
                                                                std::size_t                   ViewportIndex,
                                                                const glm::vec4&              NormalizedViewport,
                                                                VkExtent2D                    Extent);
        void RecordBaseMeshes(VkCommandBuffer                                 CommandBuffer,
                              const FScene&                                   SceneData,
                              const SurfaceState::FSurfaceGPUResourceManager& SurfaceGPU,
                              const FCommandRecordingState&                   Frame,
                              const FViewportRecordingState&                  View);
        void RecordAccumulationOverlays(VkCommandBuffer                                 CommandBuffer,
                                        const FScene&                                   SceneData,
                                        const SurfaceState::FSurfaceGPUResourceManager& SurfaceGPU,
                                        const FCommandRecordingState&                   Frame,
                                        const FViewportRecordingState&                  View);
        void RecordWorldReferenceAndGizmo(VkCommandBuffer                CommandBuffer,
                                          const FScene&                  SceneData,
                                          const FDebugUI&                DebugInterface,
                                          const FViewportRecordingState& View);

#pragma endregion

// ============================================================
// 08. Resource Ownership
// ============================================================
#pragma region "08_Resource_Ownership"
// Vulkan 자원과 표시 객체를 Renderer의 소유 수명에 연결한다.

        const GPU::FVulkanContext&         Context;
        FWindow&                           TargetWindow;
        Asset::FAssetManager&              Assets;
        SurfaceState::FSurfaceDataManager& SurfaceData;
        /** @brief Application이 소유하며 Renderer는 compute 기록과 렌더링 동안만 참조한다. */
        SurfaceState::FSurfaceStateSystem&                     SurfaceStateSystem;
        FFrameCoordinator&                                     FrameCoordinator;
        GPU::FSwapchain                                        SwapchainData;
        VkFormat                                               DepthFormat = VK_FORMAT_UNDEFINED;
        std::vector<std::unique_ptr<GPU::FGPUImage>>           DepthImages;
        std::vector<std::unique_ptr<GPU::FGPUImageView>>       DepthImageViews;
        GPU::FRenderPass                                       MainRenderPass;
        GPU::FGraphicsPass                                     MainGraphicsPass;
        VkDescriptorSetLayout                                  MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        GPU::FGraphicsPipeline                                 StaticMeshPipeline;
        GPU::FGraphicsPipeline                                 WireframePipeline;
        GPU::FGraphicsPipeline                                 GizmoPipeline;
        GPU::FGraphicsPipeline                                 WorldReferencePipeline;
        std::unique_ptr<GPU::FGPUBuffer>                       GizmoVertexBuffer;
        std::uint32_t                                          WorldGridVertexCount = 0;
        std::uint32_t                                          WorldAxisVertexCount = 0;
        std::uint32_t                                          TranslateGizmoVertexCount = 0;
        std::uint32_t                                          RotateGizmoVertexCount = 0;
        bool                                                   bWorldGridVisible = true;
        bool                                                   bWorldAxisVisible = true;
        std::unique_ptr<GPU::FGraphicsPipeline>                SurfaceDebugPipeline;
        std::unique_ptr<SurfaceState::FTexelGeometryPreview>   TexelGeometryPreview;
        std::unique_ptr<GPU::FGraphicsPipeline>                TexelGeometryPipeline;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> SurfaceLitPipelines;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> BaseSurfaceLitPipelines;
        std::unique_ptr<FRenderStateTexture>                   RenderStateTexture;
        std::unique_ptr<SurfaceState::FTexelGeometryPreview>   MudLayerGeometry;
        std::unique_ptr<FHeightFieldSmoothing>                 HeightFieldSmoothing;
        std::unique_ptr<FAccumulationOverlaySides>             OverlaySides;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> MudOverlayTopPipelines;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> MudOverlaySidePipelines;
        GPU::FFramebuffer                                      MainFramebuffers;
        FRenderContext                                         FrameContext;
        VkDescriptorPool                                       MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<FMaterialRenderResource>                   MaterialResources;
        VkDeviceSize                                           MaterialUniformStride = 0;
        std::size_t                                            MaterialViewportCapacity = 1;
#pragma endregion

// ============================================================
// 09. View and Display Settings
// ============================================================
#pragma region "09_View_and_Display_Settings"
// view·외형·overlay 제어 값을 프레임 사이에 유지한다.

        std::vector<ERenderViewMode> ViewModes{ERenderViewMode::Lit};
        bool                         bWireframeUniformWhite = true;
        bool                         bSupportsWireframeLineWidth = false;
        float                        WireframeLineWidth = 2.0F;
        float                        WireframeLineWidthMin = 1.0F;
        float                        WireframeLineWidthMax = 1.0F;
        std::uint32_t                DebugStateChannel = 0;
        bool                         bStateHeatmapReliefShadingEnabled = true;
        bool                         bStateHeatmapAllStatesEnabled = false;
        FSurfaceDebugDisplaySettings SurfaceDebugSettings;
        FDemoSurfaceEffectSettings   DemoEffects;
        EOverlayDisplayMode          OverlayDisplayMode = EOverlayDisplayMode::Both;
        EOverlayShadingDiagnostic    OverlayShadingDiagnostic = EOverlayShadingDiagnostic::Lit;
        bool                         bOverlayOnlyDebug = false;
        bool                         bOverlayTileCullingEnabled = true;
        bool                         bBaseMeshDrawEnabled = true;
        bool                         bOverlayTopDrawEnabled = true;
        bool                         bOverlaySidesDrawEnabled = true;
        bool                         bSparseHeightSmoothingEnabled = true;
        bool                         bSharedHeightSmoothingEnabled = false;
        bool                         bSharedOverlayCoverageEnabled = false;
        bool                         bSharedStateTextureSmoothingEnabled = false;
        bool                         bPrecomputeCoverageSmoothingEnabled = true;
        bool                         bRenderStateTextureSamplingEnabled = true;
        bool                         bSeparableCoverageSmoothingEnabled = true;
        EOverlayOccupancyTileSize    OverlayOccupancyTileSize = EOverlayOccupancyTileSize::Tile16;
        std::uint32_t                SurfaceTexelMeshResolution = SurfaceState::SurfaceSimulationResolution;
        std::uint32_t                OverlayTexelMeshResolution = SurfaceState::SurfaceSimulationResolution;
        std::optional<SurfaceState::FSurfaceTexelSelection> InspectedTexel;
        std::unique_ptr<SurfaceState::FTexelInspector>      TexelInspector;
        ESolverTransferWeightView                 SolverTransferWeightView = ESolverTransferWeightView::Combined;
        std::uint32_t                             TexelGridBlockSize = 8;
        float                                     TexelAreaReference = 1.0e-4F;
        SurfaceState::FSurfaceSolverDebugSettings DebugSolverSettings;
        bool                                      bFlipNormalY = true;
        float                                     NormalStrength = 1.0F;
        float                                     AmbientLight = 0.25F;
        std::map<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>, SurfaceState::FSurfaceStateParameters>
                                                                           DebugProfileParameterOverrides;
        std::vector<VkSemaphore>                                           RenderFinishedSemaphores;
        VkQueryPool                                                        TimestampQueryPool = VK_NULL_HANDLE;
        std::array<bool, FRenderContext::MaxFramesInFlight>                bTimestampQueriesSubmitted{};
        std::array<std::uint32_t, FRenderContext::MaxFramesInFlight>       SolverTimestampStepsSubmitted{};
        std::array<std::uint32_t, FRenderContext::MaxFramesInFlight>       OverlayTimestampLayersSubmitted{};
        std::array<EOverlayDisplayMode, FRenderContext::MaxFramesInFlight> OverlayDisplayModesSubmitted{};
#pragma endregion

// ============================================================
// 10. Benchmark Capture State
// ============================================================
#pragma region "10_Benchmark_Capture_State"
// stream·경로·buffer·frame serial을 캡처 작업이 값으로 소유한다.

        struct FBenchmarkCaptureState
        {
            std::array<FBenchmarkFrameSubmission, FRenderContext::MaxFramesInFlight> BenchmarkSubmissions{};
            std::ofstream                                                            BenchmarkOutput;
            std::filesystem::path                                                    BenchmarkOutputPath;
            bool                                                                     bBenchmarkMetadataWritten = false;
            std::vector<std::string>                                                 BenchmarkJsonLines;
            std::uint64_t                                                            BenchmarkWarmupFrames = 0;
            std::uint64_t                                                            BenchmarkMeasurementFrames = 0;
            std::uint64_t                                                            NextBenchmarkFrameIndex = 0;
            bool                                                                     bBenchmarkCaptureEnabled = false;
            std::unique_ptr<GPU::FGPUBuffer>                                         BenchmarkScreenshot;
            std::unique_ptr<GPU::FGPUBuffer>                                         AuthoringScreenshot;
            std::vector<std::uint64_t>                                               AuthoringCheckpointSteps;
            std::uint64_t AuthoringLastCapturedStep = std::numeric_limits<std::uint64_t>::max();
            VkExtent2D                                                               BenchmarkScreenshotExtent{};
            VkFormat BenchmarkScreenshotFormat = VK_FORMAT_UNDEFINED;

            float BenchmarkGpuElapsed = -1.0F;
            float BenchmarkSimulationElapsed = -1.0F;
        };
        FBenchmarkCaptureState  Benchmark;
        std::uint32_t           TimestampQueriesPerFrame = 24;
        std::uint32_t           SolverTimestampGroupCount = 0;
        float                   TimestampPeriodNanoseconds = 0.0F;
        std::uint32_t           TimestampValidBits = 0;
        bool                    bRenderPassSubstageTimingsReliable = true;
        float                   LastRenderGpuMilliseconds = -1.0F;
        float                   LastSolverGpuMilliseconds = -1.0F;
        float                   LastSolverPass1GpuMilliseconds = -1.0F;
        float                   LastSolverPass2GpuMilliseconds = -1.0F;
        FRendererProfilingStats ProfilingStats;

#pragma endregion
    };
} // MDSS 네임스페이스
