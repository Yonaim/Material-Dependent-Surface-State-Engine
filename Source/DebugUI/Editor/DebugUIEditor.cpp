#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Scene and Camera Controls
// ============================================================
#pragma region 00_Scene_and_Camera_Controls

    void FDebugUI::SetupDockspace()
    {
        ImGuiViewport*               Viewport = ImGui::GetMainViewport();
        constexpr ImGuiDockNodeFlags DockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        constexpr ImGuiID            StableDockspaceID = 0x4D445356U;
        DockspaceID = ImGui::DockSpaceOverViewport(StableDockspaceID, Viewport, DockFlags);
        ImGuiDockNode* DockspaceNode = ImGui::DockBuilderGetNode(DockspaceID);

        if (bDockLayoutInitialized)
        {
            return;
        }
        if (DockspaceNode == nullptr)
        {
            return;
        }
        if (DockspaceNode->IsSplitNode())
        {
            bDockLayoutInitialized = true;
            return;
        }

        ImGui::DockBuilderRemoveNode(DockspaceID);
        ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace | DockFlags);
        ImGui::DockBuilderSetNodeSize(DockspaceID, Viewport->WorkSize);

        ImGuiID LogID = 0;
        ImGuiID WorkspaceID = 0;
        ImGui::DockBuilderSplitNode(DockspaceID, ImGuiDir_Down, 0.36F, &LogID, &WorkspaceID);

        ImGuiID LeftID = 0;
        ImGuiID MainID = 0;
        ImGui::DockBuilderSplitNode(WorkspaceID, ImGuiDir_Left, 0.23F, &LeftID, &MainID);
        ImGuiID RightID = 0;
        ImGuiID CenterID = 0;
        ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Right, 0.30F, &RightID, &CenterID);

        ImGuiID SceneID = 0;
        ImGuiID LeftUpperID = 0;
        ImGui::DockBuilderSplitNode(LeftID, ImGuiDir_Up, 0.20F, &SceneID, &LeftUpperID);
        ImGuiID CameraID = 0;
        ImGuiID LeftLowerID = 0;
        ImGui::DockBuilderSplitNode(LeftUpperID, ImGuiDir_Up, 0.43F, &CameraID, &LeftLowerID);
        ImGuiID TransformID = 0;
        ImGuiID RenderSettingsID = 0;
        ImGui::DockBuilderSplitNode(LeftLowerID, ImGuiDir_Up, 0.62F, &TransformID, &RenderSettingsID);

        ImGui::DockBuilderDockWindow("Scene", SceneID);
        ImGui::DockBuilderDockWindow("Animation", SceneID);
        ImGui::DockBuilderDockWindow("Camera", CameraID);
        ImGui::DockBuilderDockWindow("Selected Object##SceneTools", TransformID);
        ImGui::DockBuilderDockWindow("Render Debug###Render Settings", RenderSettingsID);
        ImGui::DockBuilderDockWindow("Simulation Debug", RightID);
        ImGui::DockBuilderDockWindow("Log", LogID);
        ImGui::DockBuilderFinish(DockspaceID);
        bDockLayoutInitialized = true;
    }

    void FDebugUI::ResetSurfaceStateSettings()
    {
        const bool bHasStates = SurfaceDataManager->GetSurfaceStateRegistry().GetStateCount() != 0;
        InjectState = bHasStates ? 0 : SurfaceState::InvalidStateId;
        DebugState = InjectState;
        DebugParameterState = InjectState;
        DebugParameterProfile = Asset::InvalidAssetHandle;
        ParameterDraftKey = {Asset::InvalidAssetHandle, SurfaceState::InvalidStateId};
        ParameterDraft = {};
        bParameterDraftAvailable = false;
        bParameterDraftDirty = false;
        ParameterDrafts.clear();
        DirtyParameterDrafts.clear();
        RuntimeProfileOverrides.clear();
        ParameterStatus.clear();
        bSolverStepRequested = false;
        bSolverResetRequested = false;
        if (bHasStates)
        {
            FrameRenderer->SetDebugStateChannel(DebugState);
        }
    }

    void FDebugUI::ReplaceSceneFromPath(FScene& SceneData, const std::filesystem::path& Path)
    {
        FScene Loaded = FSceneLoader::Load(Path, *AssetManager, *SurfaceDataManager);
        FScene PreviousScene = SceneData;
        SceneData = std::move(Loaded);
        try
        {
            // Solver가 임시 Loaded가 아니라 수명이 유지되는 SceneData를 참조하게 한다.
            FrameRenderer->ReloadSceneResources(SceneData);
        }
        catch (...)
        {
            SceneData = std::move(PreviousScene);
            throw;
        }
        ResetSurfaceStateSettings();
        ResetProfilingAverages();
        SelectedObject.reset();
        ActiveGizmoAxis = -1;
    }

    void FDebugUI::RestartScene(FScene& SceneData)
    {
        const std::filesystem::path Path = SceneData.GetSourcePath();
        const FCamera               CurrentCamera = SceneData.GetMainCamera();
        bFrameTimeResetRequested = true;
        SceneData.RestoreInitialState();
        SceneData.GetMainCamera() = CurrentCamera;
        FrameRenderer->RestartSimulationState();
        ResetProfilingAverages();
        SelectedObject.reset();
        ActiveGizmoAxis = -1;
        SceneStatus = Path.empty() ? "Restarted: Scene" : "Restarted: " + Path.filename().string();
    }

    void FDebugUI::DrawSceneWindow(FScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 10.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 180.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Scene", nullptr, Flags);
        const auto& SourcePath = SceneData.GetSourcePath();
        ImGui::TextWrapped("Current Scene: %s",
                           SourcePath.empty() ? "(unsaved)" : SourcePath.filename().string().c_str());
        if (!SourcePath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", SourcePath.string().c_str());
        }
        if (ImGui::Button("Load Scene"))
        {
            // 취소하거나 로딩에 실패해도 대화상자 대기 시간은 시뮬레이션에서 제외한다.
            bFrameTimeResetRequested = true;
            if (const auto Path = FSceneFileDialog::OpenScene())
            {
                try
                {
                    ReplaceSceneFromPath(SceneData, *Path);
                    SceneStatus = "Loaded: " + Path->filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Load failed: ") + Error.what();
                    FLogger::Error("TDebugUI", SceneStatus);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Scene"))
        {
            bFrameTimeResetRequested = true;
            if (const auto Path = FSceneFileDialog::SaveScene())
            {
                try
                {
                    std::filesystem::path SavePath = *Path;
                    if (SavePath.extension() != ".Scene")
                    {
                        SavePath += ".Scene";
                    }
                    FSceneLoader::Save(SceneData, SavePath);
                    SceneData.SetSourcePath(std::filesystem::absolute(SavePath).lexically_normal());
                    SceneData.CaptureInitialState();
                    SceneStatus = "Saved: " + SavePath.filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Save failed: ") + Error.what();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart Scene"))
        {
            try
            {
                RestartScene(SceneData);
            }
            catch (const std::exception& Error)
            {
                SceneStatus = std::string("Restart failed: ") + Error.what();
                FLogger::Error("TDebugUI", SceneStatus);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("초기 Scene 상태로 복원하고 리소스를 다시 불러오지 않은 채 접촉 입력을 재생합니다.");

        ImGui::Separator();

        const std::filesystem::path SceneDirectory = std::filesystem::path(MDSS_ASSET_DIR) / "Scenes";
        const auto                  DrawDemoSceneButton = [&](const char* Label, const char* FileName)
        {
            if (ImGui::Button(Label))
            {
                bFrameTimeResetRequested = true;
                const std::filesystem::path DemoScenePath = SceneDirectory / FileName;
                try
                {
                    ReplaceSceneFromPath(SceneData, DemoScenePath);
                    SceneStatus = "Loaded: " + DemoScenePath.filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Load failed: ") + Error.what();
                    FLogger::Error("TDebugUI", SceneStatus);
                }
            }
        };

        DrawDemoSceneButton("Mountain", "Mountain.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("BrickCube", "BrickCube.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("Bunny", "Bunny.Scene");
        DrawDemoSceneButton("State-Driven Geometry", "StateDrivenGeometryDemo.Scene");
        DrawDemoSceneButton("Transition / Reaction", "ReactionTransitionComparison.Scene");
        ImGui::TextDisabled("Midterm demos (F9: replay, F10: presentation):");
        DrawDemoSceneButton("1 Rain / Materials", "MidtermRainMaterials.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("2 Dustpan / Gravity", "MidtermDustpanGravity.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("3 Water / Mud", "MidtermWaterMud.Scene");
        ImGui::TextDisabled("Showcase scenes:");
        DrawDemoSceneButton("Lion Head", "Lion Head.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("Marble Bust 01", "Marble Bust 01.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("Rock 07", "Rock 07.Scene");
        ImGui::TextDisabled("Performance scenes:");
        DrawDemoSceneButton("3 Cubes", "BrickCube_3.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("5 Cubes", "BrickCube_5.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("9 Cubes", "BrickCube_9.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("Cube 512", "BrickCube_512.Scene");
        if (!SceneStatus.empty())
        {
            ImGui::TextWrapped("%s", SceneStatus.c_str());
        }
        ImGui::End();
    }

    void FDebugUI::ReplayDemo(FScene& SceneData)
    {
        bFrameTimeResetRequested = true;
        SceneData.RestoreInitialState();
        FrameRenderer->RestartSimulationState();
        bSimulationPaused = false;
        AnimationTimeScale = 1.0F;
        SimulationTimeScale = 1.0F;
        SceneData.PlayDemoAnimation();
    }

    void FDebugUI::TogglePresentationMode()
    {
        bPresentationMode = !bPresentationMode;
        if (bPresentationMode)
        {
            bPresentationGridWasVisible = FrameRenderer->IsWorldGridVisible();
            bPresentationAxisWasVisible = FrameRenderer->IsWorldAxisVisible();
            FrameRenderer->SetWorldGridVisible(false);
            FrameRenderer->SetWorldAxisVisible(false);
            FrameRenderer->SetRenderViewMode(0, Rendering::ERenderViewMode::Lit);
        }
        else
        {
            FrameRenderer->SetWorldGridVisible(bPresentationGridWasVisible);
            FrameRenderer->SetWorldAxisVisible(bPresentationAxisWasVisible);
        }
    }

    void FDebugUI::UpdateDemoPlaybackGridAxis(FScene& SceneData)
    {
        const bool bShouldHide = SceneData.HasDemoAnimation() && SceneData.IsDemoAnimationPlaying();
        if (bShouldHide && !bDemoPlaybackGridAxisHidden)
        {
            bDemoPlaybackGridWasVisible = FrameRenderer->IsWorldGridVisible();
            bDemoPlaybackAxisWasVisible = FrameRenderer->IsWorldAxisVisible();
            FrameRenderer->SetWorldGridVisible(false);
            FrameRenderer->SetWorldAxisVisible(false);
            bDemoPlaybackGridAxisHidden = true;
        }
        else if (!bShouldHide && bDemoPlaybackGridAxisHidden)
        {
            if (!bPresentationMode)
            {
                FrameRenderer->SetWorldGridVisible(bDemoPlaybackGridWasVisible);
                FrameRenderer->SetWorldAxisVisible(bDemoPlaybackAxisWasVisible);
            }
            bDemoPlaybackGridAxisHidden = false;
        }
    }

    void FDebugUI::DrawAnimationWindow(FScene& SceneData)
    {
        if (!SceneData.HasDemoAnimation())
        {
            return;
        }

        // 기존 EditorLayout.ini에서도 Scene과 같은 탭 그룹으로 처음 배치한다.
        if (const ImGuiWindow* SceneWindow = ImGui::FindWindowByName("Scene");
            SceneWindow != nullptr && SceneWindow->DockId != 0)
        {
            ImGui::SetNextWindowDockID(SceneWindow->DockId, ImGuiCond_FirstUseEver);
        }
        ImGui::SetNextWindowSize({330.0F, 90.0F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Animation"))
        {
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
            const auto RadioWidth = [](const char* Label)
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
            if (ImGui::Button(bPresentationMode ? "Exit capture (F10)" : "Capture view (F10)"))
                TogglePresentationMode();

            BeginLabeledControlRow("Speed");
            constexpr std::array<std::pair<const char*, float>, 4> SpeedPresets = {
                {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F}, {"2x", 2.0F}}};
            bool bFirstPreset = true;
            for (const auto& [Label, Scale] : SpeedPresets)
            {
                if (!bFirstPreset)
                {
                    ContinueControlRow(RadioWidth(Label));
                }
                if (ImGui::RadioButton(Label, std::abs(AnimationTimeScale - Scale) < 0.001F))
                {
                    AnimationTimeScale = Scale;
                }
                bFirstPreset = false;
            }
            ContinueControlRow(160.0F);
            ImGui::SetNextItemWidth(-1.0F);
            SliderFloatWithDoubleClickInput("##AnimationSpeed", &AnimationTimeScale, 0.05F, 4.0F, "%.2fx");
        }
        ImGui::End();
    }

    void FDebugUI::ProcessSelectionAndGizmo(FScene& SceneData)
    {
        if (bInjectMode)
        {
            ActiveGizmoAxis = -1;
            HoveredGizmoAxis = -1;
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Space) && !ShouldSuppressDebugHotkey())
        {
            bRotationGizmoMode = !bRotationGizmoMode;
            ActiveGizmoAxis = -1;
            HoveredGizmoAxis = -1;
        }

        if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size() &&
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

        ImGuiIO&     IO = ImGui::GetIO();
        const ImVec2 DisplaySize = IO.DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const glm::vec4      Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + Rect.x * DisplaySize.x,
                                    MainViewport->Pos.y + Rect.y * DisplaySize.y};
        const ImVec2         ViewportSize{Rect.z * DisplaySize.x, Rect.w * DisplaySize.y};
        if (ViewportSize.x <= 0.0F || ViewportSize.y <= 0.0F)
        {
            return;
        }
        const float     Aspect = ViewportSize.x / ViewportSize.y;
        const FCamera&  Camera = GetActiveViewportCamera(SceneData);
        const glm::mat4 VP = Camera.GetViewProjectionMatrix(Aspect);
        const glm::vec3 Origin = Camera.GetPosition();
        const ImVec2    Mouse = IO.MousePos;
        const bool      bMouseInViewport = Mouse.x >= ViewportOrigin.x && Mouse.y >= ViewportOrigin.y &&
                                      Mouse.x < ViewportOrigin.x + ViewportSize.x &&
                                      Mouse.y < ViewportOrigin.y + ViewportSize.y;

        auto GetAxisScreenSegment = [&](glm::vec3 Position, int Axis, ImVec2& A, ImVec2& B, float& WorldScale)
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
        };

        auto GetRotationRingScreenPoint =
            [&](glm::vec3 Position, int Axis, float Angle, float WorldScale, ImVec2& Screen)
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
        };

        auto GetRotationRingAngle =
            [&](glm::vec3 Position, int Axis, float WorldScale, ImVec2 MousePosition, float& Angle, float& Distance)
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
        };

        if (ActiveGizmoAxis >= 0)
        {
            HoveredGizmoAxis = ActiveGizmoAxis;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ActiveGizmoAxis = -1;
            }
            else if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
            {
                if (bRotationGizmoMode)
                {
                    const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse;
                    if (glm::length(MouseDelta) >= 3.0F && GizmoDragWorldScale > 0.0F)
                    {
                        FTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
                        float       CurrentAngle = 0.0F, Distance = 0.0F;
                        if (GetRotationRingAngle(Transform.Position,
                                                 ActiveGizmoAxis,
                                                 GizmoDragWorldScale,
                                                 Mouse,
                                                 CurrentAngle,
                                                 Distance))
                        {
                            GizmoDragAccumulatedAngle +=
                                std::remainder(CurrentAngle - GizmoDragLastAngle, glm::two_pi<float>());
                            GizmoDragLastAngle = CurrentAngle;
                            glm::mat4 StartRotation(1.0F);
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.x), glm::vec3(1.0F, 0.0F, 0.0F));
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.y), glm::vec3(0.0F, 1.0F, 0.0F));
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.z), glm::vec3(0.0F, 0.0F, 1.0F));
                            const glm::vec3 Axis = ActiveGizmoAxis == 0   ? glm::vec3(1, 0, 0)
                                                   : ActiveGizmoAxis == 1 ? glm::vec3(0, 1, 0)
                                                                          : glm::vec3(0, 0, 1);
                            const glm::quat WorldRotation = glm::angleAxis(GizmoDragAccumulatedAngle, Axis) *
                                                            glm::quat_cast(glm::mat3(StartRotation));
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
                else if (glm::length(glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse) >= 3.0F &&
                         GizmoDragPixelLength > 1.0F && GizmoDragWorldScale > 0.0F)
                {
                    const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse;
                    const float     WorldDelta =
                        glm::dot(MouseDelta, GizmoDragScreenAxis) * (GizmoDragWorldScale / GizmoDragPixelLength);
                    const glm::vec3 AxisVector = ActiveGizmoAxis == 0   ? glm::vec3(1, 0, 0)
                                                 : ActiveGizmoAxis == 1 ? glm::vec3(0, 1, 0)
                                                                        : glm::vec3(0, 0, 1);
                    SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position =
                        GizmoDragStartPosition + AxisVector * WorldDelta;
                }
            }
            return;
        }

        HoveredGizmoAxis = -1;
        if (bMouseInViewport && !bMouseOverViewportOverlay && SelectedObject &&
            *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            float           WorldScale = glm::length(Origin - Position) * 0.18F;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;
                if (bRotationGizmoMode)
                {
                    bProjected =
                        WorldScale > 0.01F && GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);
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
                        HoveredGizmoAxis = Axis;
                    }
                }
            }
        }

        const ImGuiWindow* HoveredWindow = GImGui->HoveredWindow;
        const std::string ActiveViewportWindowName = "Viewport " + std::to_string(ActiveViewportId) + "##ViewportPanel";
        const bool        bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;
        if (bMouseOverViewportOverlay || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            HoveredViewportId != ActiveViewportId || !bMouseOverActiveViewportWindow ||
            (bInjectMode && !ImGui::GetIO().KeyShift))
        {
            return;
        }
        if (!bMouseInViewport)
        {
            return;
        }

        if (!ImGui::GetIO().KeyShift && SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            int             BestAxis = -1;
            float           BestAngle = 0.0F;
            const float     WorldScale = glm::length(Origin - Position) * 0.18F;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;
                if (bRotationGizmoMode)
                {
                    bProjected =
                        WorldScale > 0.01F && GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);
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
                ActiveGizmoAxis = BestAxis;
                GizmoDragStartMouse = {Mouse.x, Mouse.y};
                FTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
                GizmoDragStartPosition = Transform.Position;
                GizmoDragWorldScale = WorldScale;
                if (bRotationGizmoMode)
                {
                    GizmoDragLastAngle = BestAngle;
                    GizmoDragAccumulatedAngle = 0.0F;
                    GizmoDragStartRotation = Transform.RotationDegrees;
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F;
                    if (GetAxisScreenSegment(Transform.Position, BestAxis, A, B, AxisWorldScale))
                    {
                        const glm::vec2 ScreenAxis{B.x - A.x, B.y - A.y};
                        GizmoDragPixelLength = glm::length(ScreenAxis);
                        GizmoDragScreenAxis =
                            GizmoDragPixelLength > 0.0F ? ScreenAxis / GizmoDragPixelLength : glm::vec2(0.0F);
                        GizmoDragWorldScale = AxisWorldScale;
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
            SelectedObject.reset();
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
            SelectedObject = Hit.InstanceIndex;
        }
        else
        {
            SelectedObject.reset();
        }
    }

    void FDebugUI::DrawSelectedObjectWindow(FScene& SceneData)
    {
        if (!SelectedObject || *SelectedObject >= SceneData.GetStaticMeshInstances().size())
        {
            return;
        }
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const ImVec2   Position{Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 135.0F};
        ImGui::SetNextWindowPos(Position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 420.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Selected Object##SceneTools", nullptr, Flags);
        FTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
        ImGui::Text("Object %zu", *SelectedObject);
        DrawSectionHeader("Transform");
        if (ImGui::RadioButton("Translate", !bRotationGizmoMode))
        {
            bRotationGizmoMode = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", bRotationGizmoMode))
        {
            bRotationGizmoMode = true;
        }
        LabeledDragFloat3("Position", &Transform.Position.x, 0.02F);
        LabeledDragFloat3("Rotation", &Transform.RotationDegrees.x, 0.25F);
        LabeledDragFloat3("Scale", &Transform.Scale.x, 0.02F);
        ImGui::TextDisabled(bRotationGizmoMode ? "Drag a colored ring to rotate around that axis."
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
        ImGui::End();
    }

    void FDebugUI::ProcessCameraInput(FScene& SceneData)
    {
        ImGuiIO& IO = ImGui::GetIO();

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
        const ImGuiWindow* HoveredWindow = GImGui->HoveredWindow;
        const std::string ActiveViewportWindowName = "Viewport " + std::to_string(ActiveViewportId) + "##ViewportPanel";
        const bool        bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;

        if (!bRotatingCamera && bRightMouseDown && HoveredViewportId != 0 && bMouseOverActiveViewportWindow)
        {
            bRotatingCamera = true;
            ActiveViewportId = HoveredViewportId;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (bRotatingCamera && !bRightMouseDown)
        {
            bRotatingCamera = false;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        FCamera& CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

        if (bRotatingCamera)
        {
            constexpr float MouseSensitivity = 0.12F;
            glm::vec2       RotationDegrees = CameraData.GetRotationDegrees();
            RotationDegrees.x -= IO.MouseDelta.y * MouseSensitivity;
            RotationDegrees.y += IO.MouseDelta.x * MouseSensitivity;
            CameraData.SetRotationDegrees(RotationDegrees);
        }

        if (HoveredViewportId == ActiveViewportId && bMouseOverActiveViewportWindow && IO.MouseWheel != 0.0F)
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
        if (ActiveGizmoAxis >= 0)
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

    void FDebugUI::DrawCameraWindow(FScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    CameraWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{CameraWidth, 275.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + 10.0F, Viewport->WorkPos.y + 10.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({240.0F, 275.0F},
                                            {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;

        if (ImGui::Begin("Camera", nullptr, Flags))
        {
            FCamera& CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

            ImGui::TextDisabled("Viewport %u", ActiveViewportId);

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
        ImGui::End();
    }

#pragma endregion
} // namespace MDSS
