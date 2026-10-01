/**
 * @file DebugUI.h
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Logger/Logger.h"
#include "SurfaceStateSystem/State/SimulationClock.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;
struct ImFont;

namespace MDSS
{
    class TRenderer;
    class TScene;
    class TAssetManager;
    class TVulkanContext;
    class TWindow;

    class TDebugUI
    {
    public:
        TDebugUI(const TVulkanContext& Context,
                 const TWindow&      TWindow,
                 TRenderer&          TRenderer,
                 TAssetManager&      Assets);
        ~TDebugUI();

        TDebugUI(const TDebugUI&) = delete;
        TDebugUI& operator=(const TDebugUI&) = delete;
        TDebugUI(TDebugUI&&) = delete;
        TDebugUI& operator=(TDebugUI&&) = delete;

        /** @brief 새 ImGui frame을 시작해 진단 창을 갱신하고 draw data를 확정한다. */
        void BeginFrame(TScene& SceneData);

        /** @brief 현재 Vulkan render pass에 ImGui draw command를 기록한다. */
        void Render(VkCommandBuffer CommandBuffer) const;

        /** @brief swapchain 재생성 후 ImGui Vulkan backend의 image count를 갱신한다. */
        void OnSwapchainRecreated(const TVulkanContext& Context, const TRenderer& TRenderer);

        [[nodiscard]] bool IsInjectModeEnabled() const noexcept;
        [[nodiscard]] TStateId GetInjectState() const noexcept;
        [[nodiscard]] float GetInjectStrength() const noexcept;
        [[nodiscard]] float GetInjectRadius() const noexcept;
        [[nodiscard]] float GetInjectFalloff() const noexcept;
        [[nodiscard]] std::uint32_t GetInjectTexelSearchRadius() const noexcept;
        [[nodiscard]] float GetAnimationTimeScale() const noexcept;
        [[nodiscard]] float GetSimulationTimeScale() const noexcept;
        [[nodiscard]] bool IsSimulationPaused() const noexcept;
        [[nodiscard]] bool IsFixedSimulationTimestep() const noexcept { return bFixedSimulationTimestep; }
        [[nodiscard]] bool IsAutoSubsteppingEnabled() const noexcept { return bAutoSubstepping; }
        [[nodiscard]] bool ConsumeSolverStepRequest() noexcept;
        [[nodiscard]] bool ConsumeSolverResetRequest() noexcept;
        /** @brief 파일 대화상자와 동기 로딩이 포함된 frame의 경과 시간을 한 번 제외한다. */
        [[nodiscard]] bool ConsumeFrameTimeResetRequest() noexcept;
        [[nodiscard]] glm::vec4 GetSceneViewportRectNormalized() const noexcept;
        [[nodiscard]] std::optional<std::size_t> GetSelectedObject() const noexcept;
        [[nodiscard]] int GetHoveredGizmoAxis() const noexcept;
        [[nodiscard]] bool IsRotationGizmoMode() const noexcept;
        [[nodiscard]] TStateId GetDebugState() const noexcept;
        [[nodiscard]] bool ShouldSuppressDebugHotkey() const noexcept;

    private:
        void ProcessCameraInput(TScene& SceneData);
        void ProcessSelectionAndGizmo(TScene& SceneData);
        void DrawSceneWindow(TScene& SceneData);
        void DrawAnimationWindow(TScene& SceneData);
        void DrawSelectedTransformWindow(TScene& SceneData);
        void DrawCameraWindow(TScene& SceneData);
        void DrawRenderOptionsWindow(TScene& SceneData);
        void DrawRenderSettingsWindow(TScene& SceneData);
        void DrawViewportStatsOverlay();
        void ResetProfilingAverages() noexcept;
        void ResetSurfaceStateSettings();
        void DrawSimulationDebugWindow(TScene& SceneData);
        void DrawSimulationCommonControls();
        void DrawSolverTab();
        void DrawTexelInspectorTab();
        void DrawDebugStateSelector();
        void DrawContactInputTab();
        void DrawProfileTuningTab(TScene& SceneData);
        void DrawGlobalSettingsTab(TScene& SceneData);
        void DrawLogWindow();
        void SetupDockspace();
        static constexpr float SectionHeaderTopPadding = 8.0F;
        void DrawSectionHeader(const char* Title,
                               float TopPadding = SectionHeaderTopPadding,
                               float BottomPadding = 4.0F) const;

        VkDevice    Device = VK_NULL_HANDLE;
        GLFWwindow* NativeWindow = nullptr;
        TRenderer*   FrameRenderer = nullptr;
        TAssetManager* AssetManager = nullptr;
        bool        bRotatingCamera = false;
        float       CameraZoomSpeed = 12.0F;
        float       CameraMoveSpeed = 2.5F;
        bool        bDockLayoutInitialized = false;
        bool        bInjectMode = false;
        TStateId    InjectState = 0;
        TStateId    DebugState = 0;
        float       InjectStrength = 1.0F;
        float       InjectRadius = 0.25F;
        float       InjectFalloff = 1.0F;
        int         InjectTexelSearchRadius = 2;
        float       AnimationTimeScale = 1.0F;
        float       SimulationTimeScale = 1.0F;
        bool        bFixedSimulationTimestep = DefaultFixedSimulationTimestep;
        bool        bAutoSubstepping = DefaultAutoSubstepping;
        bool        bSimulationPaused = false;
        bool        bSolverStepRequested = false;
        bool        bSolverResetRequested = false;
        bool        bFrameTimeResetRequested = false;
        TSRProfileAssetHandle DebugParameterProfile = InvalidAssetHandle;
        TStateId DebugParameterState = 0;
        std::pair<TSRProfileAssetHandle, TStateId> ParameterDraftKey{InvalidAssetHandle, InvalidStateId};
        TSurfaceStateParameters ParameterDraft{};
        std::map<std::pair<TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> RuntimeProfileOverrides;
        std::map<std::pair<TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> ParameterDrafts;
        std::set<std::pair<TSRProfileAssetHandle, TStateId>> DirtyParameterDrafts;
        bool bParameterDraftAvailable = false;
        bool bParameterDraftDirty = false;
        std::string ParameterStatus;
        std::string ResolutionStatus;
        std::optional<std::size_t> SelectedObject;
        int         ActiveGizmoAxis = -1;
        int         HoveredGizmoAxis = -1;
        bool        bRotationGizmoMode = false;
        glm::vec2   GizmoDragStartMouse{0.0F};
        glm::vec2   GizmoDragScreenAxis{0.0F};
        glm::vec3   GizmoDragStartPosition{0.0F};
        glm::vec3   GizmoDragStartRotation{0.0F};
        float       GizmoDragLastAngle = 0.0F;
        float       GizmoDragAccumulatedAngle = 0.0F;
        float       GizmoDragWorldScale = 0.0F;
        float       GizmoDragPixelLength = 0.0F;
        std::string SceneStatus;
        std::string EditorLayoutPath;
        std::uint32_t DockspaceID = 0;
        ImFont* SectionHeaderFont = nullptr;
        glm::vec4 SceneViewportRectNormalized{0.0F, 0.0F, 1.0F, 1.0F};

        std::array<bool, static_cast<std::size_t>(TLogLevel::Count)> LogLevelFilters{true, true, true, true, true};
        std::array<char, 128>                                     LogSearch{};
        std::vector<TLogEntry>                                       CachedLogEntries;
        std::uint64_t                                               LastSeenLogRevision = 0;
        bool                                                        bScrollLogToBottom = true;
        float                                                       LogWindowHeight = 540.0F;
        struct TProfilingSample
        {
            double TimeSeconds = 0.0;
            std::array<float, 38> Values{};
        };
        double                                                      ProfilingWindowElapsed = 0.0;
        double                                                      ProfilingElapsedSeconds = 0.0;
        double                                                      ProfilingFpsSum = 0.0;
        double                                                      ProfilingFrameTimeSum = 0.0;
        std::uint32_t                                               ProfilingFrameSamples = 0;
        std::array<double, 36>                                      ProfilingMetricSums{};
        std::array<std::uint32_t, 36>                               ProfilingMetricSamples{};
        std::array<float, 38>                                       ProfilingAverages{};
        std::array<float, 38>                                       ProfilingMaximums = []
        {
            std::array<float, 38> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::array<float, 38>                                       ProfilingWindowMaximums = []
        {
            std::array<float, 38> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::array<float, 38>                                       ProfilingRecent100msAverages = []
        {
            std::array<float, 38> Values{};
            Values.fill(-1.0F);
            return Values;
        }();
        std::deque<TProfilingSample>                                ProfilingRecentSamples;
        bool                                                        bProfilingAverageAvailable = false;
        bool                                                        bShowProfilingAverage = true;
        bool                                                        bShowProfilingMaximum = false;
        bool                                                        bShowProfilingPast100ms = false;
        bool                                                        bShowDetailedProfiling = true;
        bool                                                        bProfiledRawFluxCacheEnabled = false;
        bool                                                        bViewportOverlaysVisible = true;
    };
} // namespace MDSS
