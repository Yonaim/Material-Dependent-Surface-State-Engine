/**
 * @file DebugUIEditor.cpp
 * @brief Scene 선택·객체 편집·gizmo 및 데모 재생 조작을 그린다.
 */

#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Scene and Camera Controls
// ============================================================
#pragma region 00_Scene_and_Camera_Controls
    // Scene 선택·카메라 설정을 사용자 입력에 따라 적용한다.

    void FDebugUI::SetupDockspace()
    {
        ImGuiViewport               *Viewport = ImGui::GetMainViewport();
        constexpr ImGuiDockNodeFlags DockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        constexpr ImGuiID            StableDockspaceID = 0x4D44535BU;
        ViewportState.DockspaceID = ImGui::DockSpaceOverViewport(StableDockspaceID, Viewport, DockFlags);
        ImGuiDockNode *DockspaceNode = ImGui::DockBuilderGetNode(ViewportState.DockspaceID);

        if (DockspaceNode == nullptr || (bDockLayoutInitialized && !ViewportState.bResetWorkspaceLayout))
            return;
        if (DockspaceNode->IsSplitNode() && !ViewportState.bResetWorkspaceLayout)
        {
            bDockLayoutInitialized = true;
            return;
        }

        ViewportState.bResetWorkspaceLayout = false;
        ImGui::DockBuilderRemoveNode(ViewportState.DockspaceID);
        ImGui::DockBuilderAddNode(ViewportState.DockspaceID, ImGuiDockNodeFlags_DockSpace | DockFlags);
        ImGui::DockBuilderSetNodeSize(ViewportState.DockspaceID, Viewport->WorkSize);

        ImGuiID Left = 0;
        ImGuiID CenterRight = 0;
        ImGui::DockBuilderSplitNode(ViewportState.DockspaceID, ImGuiDir_Left, 0.19F, &Left, &CenterRight);
        ImGuiID Right = 0;
        ImGuiID Center = 0;
        ImGui::DockBuilderSplitNode(CenterRight, ImGuiDir_Right, 0.32F, &Right, &Center);
        ImGuiID Bottom = 0;
        ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.19F, &Bottom, nullptr);

        ImGuiID Setup = 0;
        ImGuiID Scenes = 0;
        ImGui::DockBuilderSplitNode(Left, ImGuiDir_Down, 0.52F, &Setup, &Scenes);
        ImGuiID Rendering = 0;
        ImGuiID SimulationSettings = 0;
        ImGui::DockBuilderSplitNode(Right, ImGuiDir_Down, 0.46F, &Rendering, &SimulationSettings);

        ImGui::DockBuilderDockWindow("Scenes", Scenes);
        ImGui::DockBuilderDockWindow("Camera", Scenes);
        ImGui::DockBuilderDockWindow("Scene Setup", Setup);
        ImGui::DockBuilderDockWindow("Simulation", SimulationSettings);
        ImGui::DockBuilderDockWindow("Cell Inspector", SimulationSettings);
        ImGui::DockBuilderDockWindow("Render Options", Rendering);
        ImGui::DockBuilderDockWindow("Experiments", Bottom);
        ImGui::DockBuilderDockWindow("Profiling", Bottom);
        ImGui::DockBuilderDockWindow("Log", Bottom);
        if (auto *Tools = ImGui::DockBuilderGetNode(Bottom))
            Tools->SelectedTabId = ImHashStr("Experiments");
        ImGui::DockBuilderFinish(ViewportState.DockspaceID);
        bDockLayoutInitialized = true;
    }

    void FDebugUI::ResetSurfaceStateSettings()
    {
        const bool bHasStates = SurfaceDataManager->GetSurfaceStateRegistry().GetStateCount() != 0;
        Simulation.InjectState = bHasStates ? 0 : SurfaceState::InvalidStateId;
        Simulation.DebugState = Simulation.InjectState;

        Simulation.bSolverStepRequested = false;
        Simulation.bSolverResetRequested = false;

        if (bHasStates)
        {
            FrameRenderer->SetDebugStateChannel(Simulation.DebugState);
        }
    }

    void FDebugUI::RestartScene(FScene &SceneData)
    {
        const std::filesystem::path Path = SceneData.GetSourcePath();
        const FCamera               CurrentCamera = SceneData.GetMainCamera();
        Simulation.bFrameTimeResetRequested = true;
        SceneData.RestoreInitialState();
        SceneData.GetMainCamera() = CurrentCamera;
        FrameRenderer->RestartSimulationState();
        if (SceneData.HasDemoAnimation())
        {
            SceneData.PlayDemoAnimation();
            Simulation.bSimulationPaused = false;
            Simulation.AnimationTimeScale = 1.0F;
            Simulation.SimulationTimeScale = 1.0F;
        }
        ResetProfilingAverages();
        Gizmo.SelectedObject.reset();
        Gizmo.ActiveAxis = -1;
        ViewportState.SceneStatus = Path.empty() ? "Restarted: Scene" : "Restarted: " + Path.filename().string();
    }

#pragma endregion

// ============================================================
// 01. Animation and Presentation
// ============================================================
#pragma region 01_Animation_and_Presentation
    // 데모 재생·재시작과 발표 화면 상태를 Scene 실행에 전달한다.

    void FDebugUI::ReplayDemo(FScene &SceneData)
    {
        Simulation.bFrameTimeResetRequested = true;
        SceneData.RestoreInitialState();
        FrameRenderer->RestartSimulationState();
        Simulation.bSimulationPaused = false;
        Simulation.AnimationTimeScale = 1.0F;
        Simulation.SimulationTimeScale = 1.0F;
        SceneData.PlayDemoAnimation();
    }

    void FDebugUI::TogglePresentationMode()
    {
        Presentation.bPresentationMode = !Presentation.bPresentationMode;

        if (Presentation.bPresentationMode)
        {
            Presentation.bPresentationGridWasVisible = FrameRenderer->IsWorldGridVisible();
            Presentation.bPresentationAxisWasVisible = FrameRenderer->IsWorldAxisVisible();
            FrameRenderer->SetWorldGridVisible(false);
            FrameRenderer->SetWorldAxisVisible(false);
            FrameRenderer->SetRenderViewMode(0, Rendering::ERenderViewMode::Lit);
        }
        else
        {
            FrameRenderer->SetWorldGridVisible(Presentation.bPresentationGridWasVisible);
            FrameRenderer->SetWorldAxisVisible(Presentation.bPresentationAxisWasVisible);
        }
    }

    void FDebugUI::DrawAnimationControls(FScene &SceneData)
    {
        if (!SceneData.HasDemoAnimation())
        {
            ImGui::TextDisabled("No animation track is attached.");
            return;
        }
        const float ControlColumnX = ImGui::GetCursorPosX() + LabeledControlColumnWidth;
        const auto  ContinueControlRow = [ControlColumnX](float NextItemWidth)
        {
            const float RightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + NextItemWidth <= RightEdge)
            {
                ImGui::SameLine();
            }
            else
            {
                ImGui::SetCursorPosX(ControlColumnX);
            }
        };
        const auto RadioWidth = [](const char *Label)
        { return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(Label).x; };

        const bool bAnimationPlaying = SceneData.IsDemoAnimationPlaying();
        BeginLabeledControlRow("Playback");

        if (ImGui::RadioButton("Running", bAnimationPlaying))
        {
            if (SceneData.GetDemoAnimationTime() >= SceneData.GetDemoAnimationDuration())
                ReplayDemo(SceneData);
            else
                SceneData.PlayDemoAnimation();
        }

        ContinueControlRow(RadioWidth("Paused"));

        if (ImGui::RadioButton("Paused", !bAnimationPlaying))
        {
            SceneData.PauseDemoAnimation();
        }

        if (ImGui::Button("Replay demo"))
        {
            ReplayDemo(SceneData);
        }

        ImGui::SameLine();
        ImGui::Text("%.1f / %.1f s", SceneData.GetDemoAnimationTime(), SceneData.GetDemoAnimationDuration());

        if (SceneData.GetDemoWeather())
            ImGui::Checkbox("Weather preview", &bSceneWeatherPreview);

        if (ImGui::Button(Presentation.bPresentationMode ? "Exit capture (F10)" : "Capture view (F10)"))
            TogglePresentationMode();

        BeginLabeledControlRow("Speed");
        constexpr std::array<std::pair<const char *, float>, 4> SpeedPresets = {
            {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F}, {"2x", 2.0F}}};
        bool bFirstPreset = true;

        for (const auto &[Label, Scale] : SpeedPresets)
        {
            if (!bFirstPreset)
            {
                ContinueControlRow(RadioWidth(Label));
            }

            if (ImGui::RadioButton(Label, std::abs(Simulation.AnimationTimeScale - Scale) < 0.001F))
            {
                Simulation.AnimationTimeScale = Scale;
            }

            bFirstPreset = false;
        }

        ContinueControlRow(160.0F);
        ImGui::SetNextItemWidth(-1.0F);
        SliderFloatWithDoubleClickInput("##AnimationSpeed", &Simulation.AnimationTimeScale, 0.05F, 4.0F, "%.2fx");
    }

#pragma endregion

// ============================================================
// 02. Selection and Gizmo Interaction
// ============================================================
#pragma region 02_Selection_and_Gizmo_Interaction
    // 선택 대상에 gizmo 입력을 적용하고 변환 변경을 이어간다.

    void FDebugUI::ProcessSelectionAndGizmo(FScene &SceneData)
    {
        if (bInjectMode)
        {
            Gizmo.ActiveAxis = -1;
            Gizmo.HoveredAxis = -1;
            return;
        }

        HandleGizmoKeyboardShortcuts(SceneData);
        ProcessGizmoPointerInteraction(SceneData);
    }

    void FDebugUI::HandleGizmoKeyboardShortcuts(FScene &SceneData)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Space) && !ShouldSuppressDebugHotkey())
        {
            Gizmo.bRotationMode = !Gizmo.bRotationMode;
            Gizmo.ActiveAxis = -1;
            Gizmo.HoveredAxis = -1;
        }

        if (Gizmo.SelectedObject && *Gizmo.SelectedObject < SceneData.GetStaticMeshInstances().size() &&
            !ShouldSuppressDebugHotkey())
        {
            constexpr std::array<ImGuiKey, 9> NumpadKeys = {
                ImGuiKey_Keypad1,
                ImGuiKey_Keypad2,
                ImGuiKey_Keypad3,
                ImGuiKey_Keypad4,
                ImGuiKey_Keypad5,
                ImGuiKey_Keypad6,
                ImGuiKey_Keypad7,
                ImGuiKey_Keypad8,
                ImGuiKey_Keypad9,
            };

            for (std::size_t Index = 0; Index < NumpadKeys.size(); ++Index)
            {
                if (ImGui::IsKeyPressed(NumpadKeys[Index]))
                {
                    ApplyCameraViewPreset(SceneData, Index);
                    break;
                }
            }
        }
    }

    // Gizmo 좌표 투영은 현재 활성 viewport의 스냅샷을 공유한다.
    struct FGizmoViewportProjection
    {
        ImVec2    ViewportOrigin{};
        ImVec2    ViewportSize{};
        glm::mat4 VP{1.0F};
        glm::vec3 Origin{0.0F};

        bool GetAxisScreenSegment(glm::vec3 Position, int Axis, ImVec2 &A, ImVec2 &B, float &WorldScale) const
        {
            const glm::vec3 AxisVector = Axis == 0   ? glm::vec3(1, 0, 0)
                                         : Axis == 1 ? glm::vec3(0, 1, 0)
                                                     : glm::vec3(0, 0, 1);
            WorldScale = glm::length(Origin - Position) * 0.18F;

            if (WorldScale <= 0.01F || !ProjectToScreen(Position, VP, ViewportSize, A) ||
                !ProjectToScreen(Position + AxisVector * WorldScale, VP, ViewportSize, B))
            {
                return false;
            }

            A.x += ViewportOrigin.x;
            A.y += ViewportOrigin.y;
            B.x += ViewportOrigin.x;
            B.y += ViewportOrigin.y;
            return true;
        }

        bool
        GetRotationRingScreenPoint(glm::vec3 Position, int Axis, float Angle, float WorldScale, ImVec2 &Screen) const
        {
            const glm::vec3 U = Axis == 0 ? glm::vec3(0, 1, 0) : Axis == 1 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
            const glm::vec3 V = Axis == 0 ? glm::vec3(0, 0, 1) : Axis == 1 ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            const glm::vec3 Point = Position + WorldScale * 0.9F * (U * std::cos(Angle) + V * std::sin(Angle));

            if (!ProjectToScreen(Point, VP, ViewportSize, Screen))
            {
                return false;
            }

            Screen.x += ViewportOrigin.x;
            Screen.y += ViewportOrigin.y;
            return true;
        }

        bool GetRotationRingAngle(glm::vec3 Position,
                                  int       Axis,
                                  float     WorldScale,
                                  ImVec2    MousePosition,
                                  float    &Angle,
                                  float    &Distance) const
        {
            constexpr int Segments = 96;
            Distance = std::numeric_limits<float>::max();
            bool bFound = false;

            for (int Segment = 0; Segment < Segments; ++Segment)
            {
                const float Angle0 = glm::two_pi<float>() * static_cast<float>(Segment) / Segments;
                const float Angle1 = glm::two_pi<float>() * static_cast<float>(Segment + 1) / Segments;
                ImVec2      A{}, B{};

                if (!GetRotationRingScreenPoint(Position, Axis, Angle0, WorldScale, A) ||
                    !GetRotationRingScreenPoint(Position, Axis, Angle1, WorldScale, B))
                {
                    continue;
                }

                float       Along = 0.0F;
                const float SegmentDistance = PointSegmentDistance(MousePosition, A, B, Along);

                if (SegmentDistance < Distance)
                {
                    Distance = SegmentDistance;
                    Angle = Angle0 + (Angle1 - Angle0) * Along;
                    bFound = true;
                }
            }

            return bFound;
        }
    };

    void FDebugUI::ProcessGizmoPointerInteraction(FScene &SceneData)
    {
        ImGuiIO     &IO = ImGui::GetIO();
        const ImVec2 DisplaySize = IO.DisplaySize;

        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }

        const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
        const glm::vec4      Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + Rect.x * DisplaySize.x,
                                            MainViewport->Pos.y + Rect.y * DisplaySize.y};
        const ImVec2         ViewportSize{Rect.z * DisplaySize.x, Rect.w * DisplaySize.y};

        if (ViewportSize.x <= 0.0F || ViewportSize.y <= 0.0F)
        {
            return;
        }

        const float                    Aspect = ViewportSize.x / ViewportSize.y;
        const FCamera                 &Camera = GetActiveViewportCamera(SceneData);
        const glm::mat4                VP = Camera.GetViewProjectionMatrix(Aspect);
        const glm::vec3                Origin = Camera.GetPosition();
        const ImVec2                   Mouse = IO.MousePos;
        const FGizmoViewportProjection Projection{ViewportOrigin, ViewportSize, VP, Origin};

        if (Gizmo.ActiveAxis >= 0)
        {
            UpdateActiveGizmoDrag(SceneData, Projection, Mouse);
            return;
        }

        UpdateHoveredGizmoAxis(SceneData, Projection, Mouse);
        HandleGizmoViewportClick(SceneData, Projection, Mouse);
    }

    void
    FDebugUI::UpdateActiveGizmoDrag(FScene &SceneData, const FGizmoViewportProjection &Projection, const ImVec2 &Mouse)
    {
        const glm::vec3 &Origin = Projection.Origin;
        const glm::mat4 &VP = Projection.VP;
        const ImVec2    &ViewportOrigin = Projection.ViewportOrigin;
        const ImVec2    &ViewportSize = Projection.ViewportSize;

        Gizmo.HoveredAxis = Gizmo.ActiveAxis;

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            Gizmo.ActiveAxis = -1;
        }
        else if (Gizmo.SelectedObject && *Gizmo.SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            if (Gizmo.bRotationMode)
            {
                const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - Gizmo.DragStartMouse;

                if (glm::length(MouseDelta) >= 3.0F && Gizmo.DragWorldScale > 0.0F)
                {
                    FTransform &Transform = SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform();
                    float       CurrentAngle = 0.0F, Distance = 0.0F;

                    if (Projection.GetRotationRingAngle(Transform.Position,
                                                        Gizmo.ActiveAxis,
                                                        Gizmo.DragWorldScale,
                                                        Mouse,
                                                        CurrentAngle,
                                                        Distance))
                    {
                        Gizmo.DragAccumulatedAngle +=
                            std::remainder(CurrentAngle - Gizmo.DragLastAngle, glm::two_pi<float>());
                        Gizmo.DragLastAngle = CurrentAngle;
                        glm::mat4 StartRotation(1.0F);
                        StartRotation = glm::rotate(StartRotation,
                                                    glm::radians(Gizmo.DragStartRotation.x),
                                                    glm::vec3(1.0F, 0.0F, 0.0F));
                        StartRotation = glm::rotate(StartRotation,
                                                    glm::radians(Gizmo.DragStartRotation.y),
                                                    glm::vec3(0.0F, 1.0F, 0.0F));
                        StartRotation = glm::rotate(StartRotation,
                                                    glm::radians(Gizmo.DragStartRotation.z),
                                                    glm::vec3(0.0F, 0.0F, 1.0F));
                        const glm::vec3 Axis = Gizmo.ActiveAxis == 0   ? glm::vec3(1, 0, 0)
                                               : Gizmo.ActiveAxis == 1 ? glm::vec3(0, 1, 0)
                                                                       : glm::vec3(0, 0, 1);
                        const glm::quat WorldRotation =
                            glm::angleAxis(Gizmo.DragAccumulatedAngle, Axis) * glm::quat_cast(glm::mat3(StartRotation));
                        const glm::mat3 Rotated = glm::mat3_cast(WorldRotation);
                        const float     SinY = std::clamp(Rotated[2][0], -1.0F, 1.0F);
                        const float     Y = std::asin(SinY);
                        const float     CosY = std::cos(Y);
                        float           X = 0.0F, Z = 0.0F;

                        if (std::abs(CosY) > 1.0e-5F)
                        {
                            X = std::atan2(-Rotated[2][1], Rotated[2][2]);
                            Z = std::atan2(-Rotated[1][0], Rotated[0][0]);
                        }
                        else
                        {
                            X = std::atan2(SinY * Rotated[0][1], Rotated[1][1]);
                        }

                        Transform.RotationDegrees = glm::degrees(glm::vec3(X, Y, Z));
                    }
                }
            }
            else if (glm::length(glm::vec2(Mouse.x, Mouse.y) - Gizmo.DragStartMouse) >= 3.0F &&
                     Gizmo.DragPixelLength > 1.0F && Gizmo.DragWorldScale > 0.0F)
            {
                const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - Gizmo.DragStartMouse;
                const float     WorldDelta =
                    glm::dot(MouseDelta, Gizmo.DragScreenAxis) * (Gizmo.DragWorldScale / Gizmo.DragPixelLength);
                const glm::vec3 AxisVector = Gizmo.ActiveAxis == 0   ? glm::vec3(1, 0, 0)
                                             : Gizmo.ActiveAxis == 1 ? glm::vec3(0, 1, 0)
                                                                     : glm::vec3(0, 0, 1);
                SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform().Position =
                    Gizmo.DragStartPosition + AxisVector * WorldDelta;
            }
        }

        return;
    }

    void
    FDebugUI::UpdateHoveredGizmoAxis(FScene &SceneData, const FGizmoViewportProjection &Projection, const ImVec2 &Mouse)
    {
        const glm::vec3 &Origin = Projection.Origin;
        const glm::mat4 &VP = Projection.VP;
        const ImVec2    &ViewportOrigin = Projection.ViewportOrigin;
        const ImVec2    &ViewportSize = Projection.ViewportSize;
        const bool       bMouseInViewport = Mouse.x >= ViewportOrigin.x && Mouse.y >= ViewportOrigin.y &&
                                            Mouse.x < ViewportOrigin.x + ViewportSize.x &&
                                            Mouse.y < ViewportOrigin.y + ViewportSize.y;
        Gizmo.HoveredAxis = -1;

        if (bMouseInViewport && !ViewportState.bMouseOverViewportOverlay && Gizmo.SelectedObject &&
            *Gizmo.SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position =
                SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform().Position;
            float BestDistance = 13.0F;
            float WorldScale = glm::length(Origin - Position) * 0.18F;

            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;

                if (Gizmo.bRotationMode)
                {
                    bProjected = WorldScale > 0.01F &&
                                 Projection.GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = Projection.GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);

                    if (bProjected)
                    {
                        Distance = PointSegmentDistance(Mouse, A, B, Along);
                    }
                }

                if (bProjected)
                {
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        Gizmo.HoveredAxis = Axis;
                    }
                }
            }
        }
    }

    void FDebugUI::HandleGizmoViewportClick(FScene                         &SceneData,
                                            const FGizmoViewportProjection &Projection,
                                            const ImVec2                   &Mouse)
    {
        const glm::vec3   &Origin = Projection.Origin;
        const glm::mat4   &VP = Projection.VP;
        const ImVec2      &ViewportOrigin = Projection.ViewportOrigin;
        const ImVec2      &ViewportSize = Projection.ViewportSize;
        const bool         bMouseInViewport = Mouse.x >= ViewportOrigin.x && Mouse.y >= ViewportOrigin.y &&
                                              Mouse.x < ViewportOrigin.x + ViewportSize.x &&
                                              Mouse.y < ViewportOrigin.y + ViewportSize.y;
        const ImGuiWindow *HoveredWindow = GImGui->HoveredWindow;
        const std::string  ActiveViewportWindowName =
            "Viewport " + std::to_string(ViewportState.ActiveViewportId) + "##ViewportPanel";
        const bool bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;
        if (ViewportState.bMouseOverViewportOverlay || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            ViewportState.HoveredViewportId != ViewportState.ActiveViewportId || !bMouseOverActiveViewportWindow ||
            (bInjectMode && !ImGui::GetIO().KeyShift))
        {
            return;
        }

        if (!bMouseInViewport)
        {
            return;
        }

        if (!ImGui::GetIO().KeyShift && Gizmo.SelectedObject &&
            *Gizmo.SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position =
                SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform().Position;
            float       BestDistance = 13.0F;
            int         BestAxis = -1;
            float       BestAngle = 0.0F;
            const float WorldScale = glm::length(Origin - Position) * 0.18F;

            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;

                if (Gizmo.bRotationMode)
                {
                    bProjected = WorldScale > 0.01F &&
                                 Projection.GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = Projection.GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);

                    if (bProjected)
                    {
                        Distance = PointSegmentDistance(Mouse, A, B, Along);
                    }
                }

                if (bProjected)
                {
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        BestAxis = Axis;
                        BestAngle = Angle;
                    }
                }
            }

            if (BestAxis >= 0)
            {
                Gizmo.ActiveAxis = BestAxis;
                Gizmo.DragStartMouse = {Mouse.x, Mouse.y};
                FTransform &Transform = SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform();
                Gizmo.DragStartPosition = Transform.Position;
                Gizmo.DragWorldScale = WorldScale;

                if (Gizmo.bRotationMode)
                {
                    Gizmo.DragLastAngle = BestAngle;
                    Gizmo.DragAccumulatedAngle = 0.0F;
                    Gizmo.DragStartRotation = Transform.RotationDegrees;
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F;

                    if (Projection.GetAxisScreenSegment(Transform.Position, BestAxis, A, B, AxisWorldScale))
                    {
                        const glm::vec2 ScreenAxis{B.x - A.x, B.y - A.y};
                        Gizmo.DragPixelLength = glm::length(ScreenAxis);
                        Gizmo.DragScreenAxis =
                            Gizmo.DragPixelLength > 0.0F ? ScreenAxis / Gizmo.DragPixelLength : glm::vec2(0.0F);
                        Gizmo.DragWorldScale = AxisWorldScale;
                    }
                }

                return;
            }
        }

        const glm::vec2 NDC{(Mouse.x - ViewportOrigin.x) / ViewportSize.x * 2.0F - 1.0F,
                            (Mouse.y - ViewportOrigin.y) / ViewportSize.y * 2.0F - 1.0F};
        const glm::mat4 InvVP = glm::inverse(VP);
        glm::vec4       FarPoint = InvVP * glm::vec4(NDC, 1.0F, 1.0F);

        if (std::abs(FarPoint.w) <= 1.0e-6F)
        {
            Gizmo.SelectedObject.reset();
            return;
        }

        FarPoint /= FarPoint.w;
        const FSurfaceRayHit Hit = FRaycaster::Cast(SceneData, *AssetManager, Origin, glm::vec3(FarPoint) - Origin);

        if (ImGui::GetIO().KeyShift)
        {
            if (Hit.bHit)
                (void)FrameRenderer->InspectTexel(SceneData, Hit.InstanceIndex, Hit.TriangleID, Hit.SimulationUV);
            else
                FrameRenderer->ClearInspectedTexel();
            return;
        }

        if (Hit.bHit)
        {
            Gizmo.SelectedObject = Hit.InstanceIndex;
        }
        else
        {
            Gizmo.SelectedObject.reset();
        }
    }

#pragma endregion

// ============================================================
// 03. Object and Camera Editing
// ============================================================
#pragma region 03_Object_and_Camera_Editing
    // 객체·카메라 편집 값을 현재 Scene에 반영한다.

    void FDebugUI::DrawSelectedObjectDetails(FScene &SceneData)
    {
        if (!Gizmo.SelectedObject || *Gizmo.SelectedObject >= SceneData.GetStaticMeshInstances().size())
        {
            return;
        }

        FTransform &Transform = SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform();
        ImGui::Text("Object %zu", *Gizmo.SelectedObject);
        DrawSectionHeader("Transform");

        if (ImGui::RadioButton("Translate", !Gizmo.bRotationMode))
        {
            Gizmo.bRotationMode = false;
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Rotate", Gizmo.bRotationMode))
        {
            Gizmo.bRotationMode = true;
        }

        LabeledDragFloat3("Position", &Transform.Position.x, 0.02F);
        LabeledDragFloat3("Rotation", &Transform.RotationDegrees.x, 0.25F);
        LabeledDragFloat3("Scale", &Transform.Scale.x, 0.02F);
        ImGui::TextDisabled(Gizmo.bRotationMode ? "Drag a colored ring to rotate around that axis."
                                                : "Drag a colored axis arrow to move along that axis.");
        DrawSectionHeader("Camera View Presets");
        ImGui::TextDisabled("Numpad 1-9 or buttons; rows = pitch, columns = yaw. Focuses the selected mesh.");

        for (std::size_t Index = 0; Index < CameraViewPresetPitchYawDegrees.size(); ++Index)
        {
            if (Index % 3 != 0)
            {
                ImGui::SameLine();
            }

            const std::string Label = std::to_string(Index + 1) + "###CameraViewPreset" + std::to_string(Index + 1);

            if (ImGui::Button(Label.c_str(), {78.0F, 28.0F}))
            {
                ApplyCameraViewPreset(SceneData, Index);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("카메라 피치: %.0f도, 요: %.0f도",
                                  CameraViewPresetPitchYawDegrees[Index][0],
                                  CameraViewPresetPitchYawDegrees[Index][1]);
            }
        }
    }

    void FDebugUI::ProcessCameraInput(FScene &SceneData)
    {
        ImGuiIO &IO = ImGui::GetIO();

        const bool bWindowFocused =
            NativeWindow != nullptr && glfwGetWindowAttrib(NativeWindow, GLFW_FOCUSED) == GLFW_TRUE;
        if (!bWindowFocused)
        {
            if (bRotatingCamera)
            {
                bRotatingCamera = false;
                glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            return;
        }

        const bool         bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const ImGuiWindow *HoveredWindow = GImGui->HoveredWindow;
        const std::string  ActiveViewportWindowName =
            "Viewport " + std::to_string(ViewportState.ActiveViewportId) + "##ViewportPanel";
        const bool bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;

        if (!bRotatingCamera && bRightMouseDown && ViewportState.HoveredViewportId != 0 &&
            bMouseOverActiveViewportWindow)
        {
            bRotatingCamera = true;
            ViewportState.ActiveViewportId = ViewportState.HoveredViewportId;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (bRotatingCamera && !bRightMouseDown)
        {
            bRotatingCamera = false;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        FCamera &CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

        if (bRotatingCamera)
        {
            constexpr float MouseSensitivity = 0.12F;
            glm::vec2       RotationDegrees = CameraData.GetRotationDegrees();
            RotationDegrees.x -= IO.MouseDelta.y * MouseSensitivity;
            RotationDegrees.y += IO.MouseDelta.x * MouseSensitivity;
            CameraData.SetRotationDegrees(RotationDegrees);
        }

        if (ViewportState.HoveredViewportId == ViewportState.ActiveViewportId && bMouseOverActiveViewportWindow &&
            IO.MouseWheel != 0.0F)
        {
            const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();

            if (glm::dot(ViewDirection, ViewDirection) > 1.0e-12F)
            {
                DollyCamera(CameraData, IO.MouseWheel * (CameraZoomSpeed / 12.0F));
            }
        }

        if (!bRotatingCamera && ShouldSuppressDebugHotkey())
        {
            return;
        }

        if (Gizmo.ActiveAxis >= 0)
        {
            return;
        }

        // Main-row Minus and Equal keys work on tenkeyless keyboards; Equal also
        // covers the shifted Plus character on the same physical key.
        const float ZoomSteps = CameraZoomSpeed * IO.DeltaTime;

        if (ImGui::IsKeyDown(ImGuiKey_Minus))
        {
            DollyCamera(CameraData, -ZoomSteps);
        }

        if (ImGui::IsKeyDown(ImGuiKey_Equal))
        {
            DollyCamera(CameraData, ZoomSteps);
        }

        const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();

        if (glm::dot(ViewDirection, ViewDirection) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Forward = glm::normalize(ViewDirection);
        const glm::vec3 WorldUp{0.0F, 0.0F, 1.0F};
        glm::vec3       FlatForward = Forward - WorldUp * glm::dot(Forward, WorldUp);

        if (glm::dot(FlatForward, FlatForward) <= 1.0e-12F)
        {
            FlatForward = {1.0F, 0.0F, 0.0F};
        }

        FlatForward = glm::normalize(FlatForward);
        const glm::vec3 RightVector = glm::cross(FlatForward, WorldUp);

        if (glm::dot(RightVector, RightVector) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Right = glm::normalize(RightVector);
        glm::vec3       MoveDirection{0.0F};

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            MoveDirection += FlatForward;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            MoveDirection -= FlatForward;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            MoveDirection += Right;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            MoveDirection -= Right;
        }

        if (glm::dot(MoveDirection, MoveDirection) <= 1.0e-12F)
        {
            return;
        }

        constexpr float FastMoveMultiplier = 3.0F;
        const float     Speed = CameraMoveSpeed * (IO.KeyShift ? FastMoveMultiplier : 1.0F);
        const glm::vec3 Delta = glm::normalize(MoveDirection) * Speed * IO.DeltaTime;
        CameraData.SetPosition(CameraData.GetPosition() + Delta);
        CameraData.SetTarget(CameraData.GetTarget() + Delta);
    }

    void FDebugUI::DrawCameraDetails(FScene &SceneData)
    {
        FCamera &CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

        ImGui::TextDisabled("Viewport %u", ViewportState.ActiveViewportId);

        if (ImGui::Button("Reset Camera View"))
        {
            CameraData = SceneData.GetInitialCamera();
        }

        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("시작 시 카메라 위치, 시선 대상, FOV로 되돌립니다.");
        }

        glm::vec3 Position = CameraData.GetPosition();

        if (LabeledDragFloat3("Position", &Position.x, 0.05F))
        {
            const glm::vec3 Delta = Position - CameraData.GetPosition();
            const glm::vec3 ShiftedTarget = CameraData.GetTarget() + Delta;
            CameraData.SetPosition(Position);
            CameraData.SetTarget(ShiftedTarget);
        }

        glm::vec2 RotationDegrees = CameraData.GetRotationDegrees();

        if (LabeledDragFloat2("Angle (Pitch/Yaw)", &RotationDegrees.x, 0.25F))
        {
            CameraData.SetRotationDegrees(RotationDegrees);
        }

        float FieldOfViewDegrees = CameraData.GetVerticalFieldOfViewDegrees();

        if (LabeledSliderFloat("FOV", &FieldOfViewDegrees, 20.0F, 120.0F, "%.1f deg"))
        {
            CameraData.SetVerticalFieldOfViewDegrees(FieldOfViewDegrees);
        }

        LabeledSliderFloat("Zoom speed", &CameraZoomSpeed, 4.0F, 36.0F, "%.0f steps/s");
        LabeledSliderFloat("Move speed", &CameraMoveSpeed, 0.5F, 20.0F, "%.1f units/s");

        ImGui::Separator();
        ImGui::TextDisabled("RMB + Mouse: look  |  WASD: move");
        ImGui::TextDisabled("Shift: 3x move  |  Wheel / hold - / = (+): zoom");
    }

#pragma endregion
} // namespace MDSS
