/**
 * @file DebugUI.h
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Logger/Logger.h"
#include "Scene/Camera.h"
#include "SurfaceState/Solver/SimulationClock.h"
#include "SurfaceState/State/SurfaceInput.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <glm/glm.hpp>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;
struct ImFont;

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
namespace MDSS::Rendering
{
    class FRenderer;
}

namespace MDSS
{
    class FWindow;
}

namespace MDSS
{

    class FScene;

    class FDebugUI
    {
    public:
        // Debug UI lifecycle
        FDebugUI(const GPU::FVulkanContext&         Context,
                 const FWindow&                     Window,
                 Rendering::FRenderer&              Renderer,
                 Asset::FAssetManager&              Assets,
                 SurfaceState::FSurfaceDataManager& SurfaceData);
        ~FDebugUI();

        FDebugUI(const FDebugUI&) = delete;
        FDebugUI& operator=(const FDebugUI&) = delete;
        FDebugUI(FDebugUI&&) = delete;
        FDebugUI& operator=(FDebugUI&&) = delete;

        // Frame, scene, and swapchain control
        /** @brief 새 ImGui frame을 시작해 진단 창을 갱신하고 draw data를 확정한다. */
        void BeginFrame(FScene& SceneData);

        /** @brief 자동 benchmark에서 solver 조건을 fixed 1/60 s step으로 고정한다. */
        void SetPresentationMode(bool bEnabled)
        {
            if (bPresentationMode != bEnabled)
                TogglePresentationMode();
        }
        void SetBenchmarkMode() noexcept
        {
            bFixedSimulationTimestep = true;
            bAutoSubstepping = false;
            bSimulationPaused = false;
            bSolverStepRequested = false;
            bSolverResetRequested = false;
            SimulationTimeScale = 1.0F;
        }

        /** @brief 저장된 .Scene을 다시 읽어 애니메이션, 상태, 초기 접촉 입력을 처음부터 시작한다. */
        void RestartScene(FScene& SceneData);

        // Draw submission
        /** @brief 현재 Vulkan render pass에 ImGui draw command를 기록한다. */
        void Render(VkCommandBuffer CommandBuffer) const;

        /** @brief swapchain 재생성 후 ImGui Vulkan backend의 image count를 갱신한다. */
        void OnSwapchainRecreated(const GPU::FVulkanContext& Context, const Rendering::FRenderer& Renderer);

        // Runtime settings and viewport state
        [[nodiscard]] bool                                   IsInjectModeEnabled() const noexcept;
        [[nodiscard]] SurfaceState::FStateId                 GetInjectState() const noexcept;
        [[nodiscard]] float                                  GetInjectStrength() const noexcept;
        [[nodiscard]] float                                  GetInjectRadius() const noexcept;
        [[nodiscard]] float                                  GetInjectFalloff() const noexcept;
        [[nodiscard]] SurfaceState::ESurfaceContactOperation GetInjectOperation() const noexcept;
        [[nodiscard]] SurfaceState::ESurfaceContactShape     GetInjectShape() const noexcept;
        [[nodiscard]] float                                  GetInjectSecondaryRadius() const noexcept;
        [[nodiscard]] float                                  GetInjectRotationDegrees() const noexcept;
        [[nodiscard]] std::uint32_t                          GetInjectTexelSearchRadius() const noexcept;
        [[nodiscard]] float                                  GetAnimationTimeScale() const noexcept;
        [[nodiscard]] float                                  GetSimulationTimeScale() const noexcept;
        [[nodiscard]] bool                                   IsSimulationPaused() const noexcept;
        [[nodiscard]] bool                                   IsFixedSimulationTimestep() const noexcept
        {
            return bFixedSimulationTimestep;
        }
        [[nodiscard]] bool IsAutoSubsteppingEnabled() const noexcept
        {
            return bAutoSubstepping;
        }
        [[nodiscard]] bool ConsumeSolverStepRequest() noexcept;
        [[nodiscard]] bool ConsumeSolverResetRequest() noexcept;
        /** @brief 설정 준비나 파일 대화상자가 포함된 frame의 경과 시간을 한 번 제외한다. */
        [[nodiscard]] bool           ConsumeFrameTimeResetRequest() noexcept;
        [[nodiscard]] std::size_t    GetViewportCount() const noexcept;
        [[nodiscard]] glm::vec4      GetViewportRectNormalized(std::size_t Index) const noexcept;
        [[nodiscard]] const FCamera& GetViewportCamera(const FScene& SceneData, std::size_t Index) const noexcept;
        [[nodiscard]] const FCamera& GetActiveViewportCamera(const FScene& SceneData) const noexcept;
        [[nodiscard]] std::optional<std::size_t> GetSelectedObject() const noexcept;
        [[nodiscard]] int                        GetHoveredGizmoAxis() const noexcept;
        [[nodiscard]] bool                       IsRotationGizmoMode() const noexcept;
        [[nodiscard]] SurfaceState::FStateId     GetDebugState() const noexcept;
        [[nodiscard]] bool                       ShouldSuppressDebugHotkey() const noexcept;

    private:
        // Scene, selection, and camera controls
        void ProcessCameraInput(FScene& SceneData);
        void ProcessSelectionAndGizmo(FScene& SceneData);
        void DrawSceneWindow(FScene& SceneData);
        void ReplaceSceneFromPath(FScene& SceneData, const std::filesystem::path& Path);
        void DrawAnimationWindow(FScene& SceneData);
        void ReplayDemo(FScene& SceneData);
        void TogglePresentationMode();
        void DrawSelectedObjectWindow(FScene& SceneData);
        void DrawCameraWindow(FScene& SceneData);
        void DrawViewportPanels(FScene& SceneData);
        void DrawDemoWeatherOverlay(const FScene& SceneData);
        void DrawBrickCubeTexelMeshNotice(const FScene& SceneData);

        // Rendering controls and performance overlays
        void DrawRenderOptionsWindow(FScene& SceneData);
        void DrawRenderSettingsWindow(FScene& SceneData);
        void DrawViewportStatsOverlay();
        void ResetProfilingAverages() noexcept;
        void ResetSurfaceStateSettings();

        // Simulation and diagnostic panels
        void DrawSimulationDebugWindow(FScene& SceneData);
        void DrawSimulationCommonControls();
        void DrawSolverTab();
        void DrawTexelInspectorTab();
        void DrawDebugStateSelector();
        void DrawContactInputTab();
        void DrawProfileTuningTab(FScene& SceneData);
        void DrawSimulationSettingsTab(FScene& SceneData);
        void DrawPerformanceTab(bool bRendering);
        void DrawLogWindow();

        // Shared layout helpers
        void                      SetupDockspace();
        void                      ApplyCameraViewPreset(FScene& SceneData, std::size_t Index);
        void                      UpdateDemoPlaybackGridAxis(FScene& SceneData);
        [[nodiscard]] FCamera&    GetViewportCamera(FScene& SceneData, std::size_t Index) noexcept;
        [[nodiscard]] std::size_t GetActiveViewportIndex() const noexcept;
        static constexpr float    SectionHeaderTopPadding = 8.0F;
        void                      DrawSectionHeader(const char* Title,
                                                    float       TopPadding = SectionHeaderTopPadding,
                                                    float       BottomPadding = 4.0F) const;

        VkDevice                               Device = VK_NULL_HANDLE;
        GLFWwindow*                            NativeWindow = nullptr;
        Rendering::FRenderer*                  FrameRenderer = nullptr;
        Asset::FAssetManager*                  AssetManager = nullptr;
        SurfaceState::FSurfaceDataManager*     SurfaceDataManager = nullptr;
        bool                                   bRotatingCamera = false;
        float                                  CameraZoomSpeed = 12.0F;
        float                                  CameraMoveSpeed = 2.5F;
        bool                                   bDockLayoutInitialized = false;
        bool                                   bInjectMode = false;
        bool                                   bPresentationMode = false;
        bool                                   bPresentationGridWasVisible = false;
        bool                                   bPresentationAxisWasVisible = false;
        bool                                   bDemoPlaybackGridAxisHidden = false;
        bool                                   bDemoPlaybackGridWasVisible = false;
        bool                                   bDemoPlaybackAxisWasVisible = false;
        SurfaceState::FStateId                 InjectState = 0;
        SurfaceState::FStateId                 DebugState = 0;
        float                                  InjectStrength = 1.0F;
        float                                  InjectRadius = 0.25F;
        float                                  InjectFalloff = 1.0F;
        SurfaceState::ESurfaceContactOperation InjectOperation = SurfaceState::ESurfaceContactOperation::Add;
        SurfaceState::ESurfaceContactShape     InjectShape = SurfaceState::ESurfaceContactShape::Circle;
        float                                  InjectSecondaryRadius = 0.25F;
        float                                  InjectRotationDegrees = 0.0F;
        int                                    InjectTexelSearchRadius = 2;
        float                                  AnimationTimeScale = 1.0F;
        float                                  SimulationTimeScale = 1.0F;
        bool                                   bFixedSimulationTimestep = SurfaceState::bDefaultFixedSimulationTimestep;
        bool                                   bAutoSubstepping = SurfaceState::bDefaultAutoSubstepping;
        bool                                   bSimulationPaused = false;
        bool                                   bSolverStepRequested = false;
        bool                                   bSolverResetRequested = false;
        bool                                   bFrameTimeResetRequested = false;
        Asset::FSRProfileAssetHandle           DebugParameterProfile = Asset::InvalidAssetHandle;
        SurfaceState::FStateId                 DebugParameterState = 0;
        std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId> ParameterDraftKey{Asset::InvalidAssetHandle,
                                                                                          SurfaceState::InvalidStateId};
        SurfaceState::FSurfaceStateParameters                           ParameterDraft{};
        std::map<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>, SurfaceState::FSurfaceStateParameters>
            RuntimeProfileOverrides;
        std::map<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>, SurfaceState::FSurfaceStateParameters>
                                                                                  ParameterDrafts;
        std::set<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>> DirtyParameterDrafts;
        bool                                                                      bParameterDraftAvailable = false;
        bool                                                                      bParameterDraftDirty = false;
        std::string                                                               ParameterStatus;
        std::string                                                               ResolutionStatus;
        std::optional<std::size_t>                                                SelectedObject;
        int                                                                       ActiveGizmoAxis = -1;
        int                                                                       HoveredGizmoAxis = -1;
        bool                                                                      bRotationGizmoMode = false;
        glm::vec2                                                                 GizmoDragStartMouse{0.0F};
        glm::vec2                                                                 GizmoDragScreenAxis{0.0F};
        glm::vec3                                                                 GizmoDragStartPosition{0.0F};
        glm::vec3                                                                 GizmoDragStartRotation{0.0F};
        float                                                                     GizmoDragLastAngle = 0.0F;
        float                                                                     GizmoDragAccumulatedAngle = 0.0F;
        float                                                                     GizmoDragWorldScale = 0.0F;
        float                                                                     GizmoDragPixelLength = 0.0F;
        std::string                                                               SceneStatus;
        std::string                                                               EditorLayoutPath;
        std::uint32_t                                                             DockspaceID = 0;
        ImFont*                                                                   SectionHeaderFont = nullptr;
        struct FViewportPanel
        {
            std::uint32_t Id = 0;
            glm::vec4     RectNormalized{0.0F};
        };
        std::vector<FViewportPanel> ViewportPanels{FViewportPanel{1U, glm::vec4(0.0F)}};
        std::uint32_t               NextViewportId = 2;
        std::uint32_t               ActiveViewportId = 1;
        std::uint32_t               HoveredViewportId = 0;
        bool                        bMouseOverViewportOverlay = false;
        glm::vec4                   ViewportWorkspaceRectNormalized{0.0F, 0.0F, 1.0F, 1.0F};
        int                         ViewportLayout = 2; // 0: grid, 1: horizontal, 2: vertical

        std::array<bool, static_cast<std::size_t>(ELogLevel::Count)> LogLevelFilters{true, true, true, true, true};
        std::array<char, 128>                                        LogSearch{};
        std::vector<FLogEntry>                                       CachedLogEntries;
        std::uint64_t                                                LastSeenLogRevision = 0;
        bool                                                         bScrollLogToBottom = true;
        float                                                        LogWindowHeight = 540.0F;
        struct FProfilingSample
        {
            double                TimeSeconds = 0.0;
            std::array<float, 43> Values{};
        };
        double                        ProfilingWindowElapsed = 0.0;
        double                        ProfilingElapsedSeconds = 0.0;
        double                        ProfilingFpsSum = 0.0;
        double                        ProfilingFrameTimeSum = 0.0;
        std::uint32_t                 ProfilingFrameSamples = 0;
        std::array<double, 40>        ProfilingMetricSums{};
        std::array<std::uint32_t, 40> ProfilingMetricSamples{};
        std::array<float, 43>         ProfilingAverages{};
        std::array<float, 43>         ProfilingMaximums = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::array<float, 43> ProfilingWindowMaximums = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::array<float, 43> ProfilingRecent100msAverages = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::deque<FProfilingSample> ProfilingRecentSamples;
        bool                         bProfilingAverageAvailable = false;
        bool                         bViewportOverlaysVisible = true;
    };
} // namespace MDSS
