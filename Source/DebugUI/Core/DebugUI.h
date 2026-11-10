/**
 * @file DebugUI.h
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "DebugUI/Profiling/ProfilingUIState.h"
#include "Logger/Logger.h"
#include "Scene/Camera.h"
#include "SurfaceState/Solver/SimulationClock.h"
#include "SurfaceState/State/SurfaceInput.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
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
struct ImVec2;

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
    struct FDemoSurfaceEffectSettings;
    struct FDemoSurfaceStateBindings;
    struct FRendererProfilingStats;
    enum class ERenderViewMode : std::uint32_t;
}

namespace MDSS
{
    class FWindow;
}

namespace MDSS
{

    class FScene;
    struct FGizmoViewportProjection;

    class FDebugUI
    {
    public:
        // ============================================================
        // 00. Lifecycle and Public API
        // ============================================================
#pragma region "00_Lifecycle_and_Public_API"
// 외부 호출자가 UI 초기화·프레임·종료 경계를 통해 기능을 사용한다.

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
            if (Presentation.bPresentationMode != bEnabled)
                TogglePresentationMode();
        }

        void SetBenchmarkMode() noexcept
        {
            Simulation.bFixedSimulationTimestep = true;
            Simulation.bAutoSubstepping = false;
            Simulation.bSimulationPaused = false;
            Simulation.bSolverStepRequested = false;
            Simulation.bSolverResetRequested = false;
            Simulation.SimulationTimeScale = 1.0F;
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
            return Simulation.bFixedSimulationTimestep;
        }

        [[nodiscard]] bool IsAutoSubsteppingEnabled() const noexcept
        {
            return Simulation.bAutoSubstepping;
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

#pragma endregion

    private:
        // ============================================================
        // 01. Scene and Viewport Operations
        // ============================================================
#pragma region "01_Scene_and_Viewport_Operations"
// Scene·viewport 입력과 선택 편집을 같은 화면 상태에 연결한다.

        // Scene, selection, and camera controls
        void ProcessCameraInput(FScene& SceneData);
        void ProcessSelectionAndGizmo(FScene& SceneData);
        void HandleGizmoKeyboardShortcuts(FScene& SceneData);
        void ProcessGizmoPointerInteraction(FScene& SceneData);
        void UpdateActiveGizmoDrag(FScene& SceneData, const FGizmoViewportProjection& Projection, const ImVec2& Mouse);
        void UpdateHoveredGizmoAxis(FScene& SceneData, const FGizmoViewportProjection& Projection, const ImVec2& Mouse);
        void HandleGizmoViewportClick(FScene& SceneData, const FGizmoViewportProjection& Projection, const ImVec2& Mouse);
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

#pragma endregion

        // ============================================================
        // 02. Rendering and Performance Controls
        // ============================================================
#pragma region "02_Rendering_and_Performance_Controls"
// 렌더 표시와 실행 비용 옵션을 해당 Renderer 설정에 전달한다.

        // Rendering controls and performance overlays
        void DrawRenderOptionsWindow(FScene& SceneData);
        [[nodiscard]] const char* DrawToolbarViewSelector(std::size_t ActiveViewportIndex,
                                                          Rendering::ERenderViewMode& CurrentMode);
        void DrawToolbarStateSelector(Rendering::ERenderViewMode CurrentMode);
        [[nodiscard]] bool DrawToolbarViewSettingsButton();
        void DrawToolbarVisibilityAndZoom(FScene& SceneData);
        void DrawViewSettingsPopup(std::size_t                 ActiveViewportIndex,
                                   Rendering::ERenderViewMode& CurrentMode,
                                   const char*                 CurrentName);
        void DrawRenderSettingsWindow(FScene& SceneData);
        void DrawVisualRenderSettings();
        void DrawVisualEffectToggles(Rendering::FDemoSurfaceEffectSettings& Effects,
                                     const Rendering::FDemoSurfaceStateBindings& Bindings,
                                     bool& bChanged);
        void DrawVisualEffectDetails(Rendering::FDemoSurfaceEffectSettings& Effects,
                                     const Rendering::FDemoSurfaceStateBindings& Bindings,
                                     bool& bChanged);
        void DrawVisualHeightFade(Rendering::FDemoSurfaceEffectSettings& Effects, bool& bChanged);
        void DrawVisualShadingControls(Rendering::FDemoSurfaceEffectSettings& Effects, bool& bChanged);
        void DrawDisplayRenderSettings(FScene& SceneData);
        void DrawActiveViewModeSettings(std::size_t ActiveViewportIndex, Rendering::ERenderViewMode& CurrentMode);
        void DrawViewModeContext(Rendering::ERenderViewMode CurrentMode, const char* Title, const char* Description);
        void DrawHeightGridControls();
        void DrawWireframeViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawStateHeatmapViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawStateGeometryViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawOutgoingFluxViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawTransferWeightViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawTexelGridViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawTexelAreaViewSettings(Rendering::ERenderViewMode& CurrentMode);
        void DrawMesoGeometryViewSettings(std::size_t ActiveViewportIndex, Rendering::ERenderViewMode& CurrentMode);

        void DrawViewportStatsOverlay();
        [[nodiscard]] float DrawTimeProfilingOverlay(const ImVec2& ViewportOrigin);
        void DrawTimeProfilingContents(const Rendering::FRendererProfilingStats& Stats, bool bFineRenderTimings);
        [[nodiscard]] float DrawMemoryProfilingOverlay(const ImVec2& ViewportOrigin, float RenderStateY);
        void DrawRenderStateProfilingOverlay(const ImVec2& ViewportOrigin, float RenderStateY);
        void ResetProfilingAverages() noexcept;
        void ResetSurfaceStateSettings();

#pragma endregion

        // ============================================================
        // 03. Simulation and Diagnostic Panels
        // ============================================================
#pragma region "03_Simulation_and_Diagnostic_Panels"
// Solver 설정과 State 진단의 패널 구성 경계를 선언한다.

        // Simulation and diagnostic panels
        void DrawSimulationDebugWindow(FScene& SceneData);
        void DrawSimulationCommonControls();
        void DrawSolverTab();
        void DrawTexelInspectorTab();
        void DrawDebugStateSelector();
        void DrawContactInputTab();
        void DrawProfileTuningTab(FScene& SceneData);
        [[nodiscard]] std::vector<Asset::FSRProfileAssetHandle> CollectSceneProfiles(const FScene& SceneData) const;
        void DrawProfileSelector(const std::vector<Asset::FSRProfileAssetHandle>& SceneProfiles);
        void DrawProfileReactionRules();
        [[nodiscard]] bool DrawProfileStateSelector();
        [[nodiscard]] bool PrepareProfileDraft();
        void DrawProfileParameterControls();
        void DrawProfileCapacityControls(bool& bChanged);
        void DrawProfileTransportControls(bool& bChanged);
        void DrawProfileDecayControls(bool& bChanged);
        void DrawProfileAccumulationControls(bool& bChanged);
        void DrawProfileOverrideActions();
        void DrawSimulationSettingsTab(FScene& SceneData);
        void DrawPerformanceTab(bool bRendering);
        void DrawRenderingPerformanceSettings();
        void DrawSimulationPerformanceSettings();
        void DrawRenderingMeshAndTileSettings();
        void DrawRenderingSmoothingSettings();
        void DrawSimulationCacheSettings();
        void DrawSimulationSparseSettings();
        void DrawLogWindow();

#pragma endregion

        // ============================================================
        // 04. Shared Layout Helpers
        // ============================================================
#pragma region "04_Shared_Layout_Helpers"
// 여러 패널에서 사용하는 크기·배치·공통 widget 처리를 모은다.

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

#pragma endregion

        // ============================================================
        // 05. Resource References
        // ============================================================
#pragma region "05_Resource_References"
// Application이 소유하는 장치·Scene·Renderer를 빌려 사용한다.

        VkDevice                           Device = VK_NULL_HANDLE;
        GLFWwindow*                        NativeWindow = nullptr;
        Rendering::FRenderer*              FrameRenderer = nullptr;
        Asset::FAssetManager*              AssetManager = nullptr;
        SurfaceState::FSurfaceDataManager* SurfaceDataManager = nullptr;
#pragma endregion

        // ============================================================
        // 06. Camera Input and Dock State
        // ============================================================
#pragma region "06_Camera_Input_and_Dock_State"
// dock 배치와 카메라 입력 처리에 필요한 UI 상태를 보관한다.

        bool  bRotatingCamera = false;
        float CameraZoomSpeed = 12.0F;
        float CameraMoveSpeed = 2.5F;
        bool  bDockLayoutInitialized = false;
        bool  bInjectMode = false;
#pragma endregion

        // ============================================================
        // 07. Presentation State
        // ============================================================
#pragma region "07_Presentation_State"
// 발표·benchmark 표시 선택을 UI 프레임 사이에 유지한다.

        struct FPresentationUIState
        {
            bool bPresentationMode = false;
            bool bPresentationGridWasVisible = false;
            bool bPresentationAxisWasVisible = false;
            bool bDemoPlaybackGridAxisHidden = false;
            bool bDemoPlaybackGridWasVisible = false;
            bool bDemoPlaybackAxisWasVisible = false;
        };
        FPresentationUIState Presentation;
#pragma endregion

        // ============================================================
        // 08. Simulation and Contact Controls
        // ============================================================
#pragma region "08_Simulation_and_Contact_Controls"
// 시간·Solver·접촉 입력 편집 값을 관련 실행 경로에 전달한다.

        struct FSimulationUIState
        {
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
            bool bFixedSimulationTimestep = SurfaceState::bDefaultFixedSimulationTimestep;
            bool bAutoSubstepping = SurfaceState::bDefaultAutoSubstepping;
            bool bSimulationPaused = false;
            bool bSolverStepRequested = false;
            bool bSolverResetRequested = false;
            bool bFrameTimeResetRequested = false;
        };
        FSimulationUIState Simulation;
#pragma endregion

        // ============================================================
        // 09. Profile Editing State
        // ============================================================
#pragma region "09_Profile_Editing_State"
// Profile 편집 중 선택한 자산·State와 임시 입력 값을 유지한다.

        struct FProfileEditingUIState
        {
            Asset::FSRProfileAssetHandle                                    Profile = Asset::InvalidAssetHandle;
            SurfaceState::FStateId                                          State = 0;
            std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId> DraftKey{Asset::InvalidAssetHandle,
                                                                                     SurfaceState::InvalidStateId};
            SurfaceState::FSurfaceStateParameters                           Draft{};
            std::map<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>,
                     SurfaceState::FSurfaceStateParameters>
                RuntimeOverrides;
            std::map<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>,
                     SurfaceState::FSurfaceStateParameters>
                                                                                      Drafts;
            std::set<std::pair<Asset::FSRProfileAssetHandle, SurfaceState::FStateId>> DirtyDrafts;
            bool                                                                      bAvailable = false;
            bool                                                                      bDirty = false;
            std::string                                                               Status;
            std::string                                                               ResolutionStatus;
        };
        FProfileEditingUIState ProfileEditing;
#pragma endregion

        // ============================================================
        // 10. Selection and Gizmo State
        // ============================================================
#pragma region "10_Selection_and_Gizmo_State"
// 선택 객체와 gizmo 조작 중인 변환 상태를 같은 수명으로 보관한다.

        struct FGizmoUIState
        {
            std::optional<std::size_t> SelectedObject;
            int                        ActiveAxis = -1;
            int                        HoveredAxis = -1;
            bool                       bRotationMode = false;
            glm::vec2                  DragStartMouse{0.0F};
            glm::vec2                  DragScreenAxis{0.0F};
            glm::vec3                  DragStartPosition{0.0F};
            glm::vec3                  DragStartRotation{0.0F};
            float                      DragLastAngle = 0.0F;
            float                      DragAccumulatedAngle = 0.0F;
            float                      DragWorldScale = 0.0F;
            float                      DragPixelLength = 0.0F;
        };
        FGizmoUIState Gizmo;
#pragma endregion

        // ============================================================
        // 11. Scene Dock and Viewport State
        // ============================================================
#pragma region "11_Scene_Dock_and_Viewport_State"
// Scene 창과 viewport별 카메라·표시 상태를 관리한다.

        struct FViewportUIState
        {
            std::string   SceneStatus;
            std::string   EditorLayoutPath;
            std::uint32_t DockspaceID = 0;
            ImFont*       SectionHeaderFont = nullptr;
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
        };
        FViewportUIState ViewportState;
#pragma endregion

        // ============================================================
        // 12. Log State
        // ============================================================
#pragma region "12_Log_State"
// 로그 필터·검색·표시 위치를 프레임 사이에 유지한다.

        struct FLogUIState
        {
            std::array<bool, static_cast<std::size_t>(ELogLevel::Count)> LevelFilters{true, true, true, true, true};
            std::array<char, 128>                                        Search{};
            std::vector<FLogEntry>                                       Entries;
            std::uint64_t                                                LastSeenRevision = 0;
            bool                                                         bScrollToBottom = true;
            float                                                        WindowHeight = 540.0F;
        };
        FLogUIState LogState;
#pragma endregion

        // ============================================================
        // 13. Profiling and Overlay State
        // ============================================================
#pragma region "13_Profiling_and_Overlay_State"
// 측정 집계와 overlay 표시 설정을 UI 소비자에게 연결한다.

        FProfilingUIState Profiling;
        bool              bViewportOverlaysVisible = true;
#pragma endregion
    };
} // namespace MDSS
