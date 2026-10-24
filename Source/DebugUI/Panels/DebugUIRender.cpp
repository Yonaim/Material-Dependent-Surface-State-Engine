#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
#pragma region Render_Options_and_Viewport_Panels

    void TDebugUI::DrawRenderOptionsWindow(TScene& SceneData)
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiDockNode* ViewportNode = DockspaceID != 0 ? ImGui::DockBuilderGetCentralNode(DockspaceID) : nullptr;
        if (ViewportNode == nullptr)
        {
            return;
        }

        constexpr float ToolbarHeight = 48.0F;
        ImGui::SetNextWindowPos(ViewportNode->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ViewportNode->Size.x, ToolbarHeight}, ImGuiCond_Always);
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2         DisplaySize = ImGui::GetIO().DisplaySize;
        if (DisplaySize.x > 0.0F && DisplaySize.y > 0.0F)
        {
            ViewportWorkspaceRectNormalized = {(ViewportNode->Pos.x - MainViewport->Pos.x) / DisplaySize.x,
                                               (ViewportNode->Pos.y + ToolbarHeight - MainViewport->Pos.y) /
                                                   DisplaySize.y,
                                               ViewportNode->Size.x / DisplaySize.x,
                                               std::max(ViewportNode->Size.y - ToolbarHeight, 1.0F) / DisplaySize.y};
        }

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNavFocus;
        const std::size_t          ActiveViewportIndex = GetActiveViewportIndex();
        Rendering::TRenderViewMode CurrentMode = FrameRenderer->GetRenderViewMode(ActiveViewportIndex);

        const auto                 GetViewGeometryDescription = [](Rendering::TRenderViewMode Mode) -> const char*
        {
            switch (Mode)
            {
                case Rendering::TRenderViewMode::Lit:
                    return "기본적으로 원본 메시를 렌더링합니다. Mud/WaterFilm/Lava 적층 레이어가 활성화된 물체는 "
                           "베이스 표면도 텍셀 메시로 그립니다.";
                case Rendering::TRenderViewMode::Unlit:
                    return "원본 메시 위치에 조명 없이 재질색을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::Wireframe:
                    return "원본 메시 삼각형을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::VertexNormalWS:
                    return "원본 메시 정점 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::NormalTextureTS:
                    return "원본 메시의 노멀 텍스처를 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::MappedNormalWS:
                    return "원본 메시의 매핑된 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceStateHeatmap:
                    return "원본 메시 위에 State 데이터를 색으로 표시합니다. Relief 음영은 Meso 노멀을 쓸 수 있지만 "
                           "형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceValidity:
                case Rendering::TRenderViewMode::SurfaceID:
                case Rendering::TRenderViewMode::NeighborCount:
                case Rendering::TRenderViewMode::SurfaceSeam:
                case Rendering::TRenderViewMode::OutgoingFluxScale:
                case Rendering::TRenderViewMode::SolverTransferWeight:
                case Rendering::TRenderViewMode::SurfaceTexelGrid:
                case Rendering::TRenderViewMode::SurfaceTexelArea:
                    return "원본 메시 위에 진단 데이터를 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::MesoHeight:
                case Rendering::TRenderViewMode::MesoOffset:
                    return "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.";
                case Rendering::TRenderViewMode::MacroGeometry:
                    return "원본 메시 형상입니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceAccumulation:
                    return "Meso + 선택 State 전체 높이를 형상에 적용합니다. 색은 선택한 적층 성분을 표시합니다.";
                case Rendering::TRenderViewMode::SurfaceFinalGeometry:
                    return "Meso + 선택 State 적층을 적용한 형상입니다. 모든 State를 합친 최종 형상은 아닙니다.";
                case Rendering::TRenderViewMode::TotalSimulationHeight:
                    return "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.";
            }
            return "원본 메시 형상입니다.";
        };

        if (ImGui::Begin("Render Options##ViewportToolbar", nullptr, Flags))
        {
            const int   CurrentIndex = static_cast<int>(CurrentMode);
            const char* CurrentName = "Solver Transfer Weights";
            if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(RenderViewModeNames.size()))
            {
                CurrentName = RenderViewModeNames[CurrentIndex];
            }
            else if (CurrentMode >= Rendering::TRenderViewMode::SurfaceStateHeatmap &&
                     CurrentMode < Rendering::TRenderViewMode::SolverTransferWeight)
            {
                CurrentName = SurfaceDebugViewNames[CurrentIndex -
                                                    static_cast<int>(Rendering::TRenderViewMode::SurfaceStateHeatmap)];
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation)
                CurrentName = "State Height Heatmap";
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry)
                CurrentName = "Meso + State Geometry";
            else if (CurrentMode == Rendering::TRenderViewMode::TotalSimulationHeight)
                CurrentName = "Total Simulation Height";
            else if (CurrentMode == Rendering::TRenderViewMode::MesoHeight ||
                     CurrentMode == Rendering::TRenderViewMode::MesoOffset)
            {
                CurrentName = "Meso Geometry";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::MacroGeometry)
            {
                CurrentName = "Base Geometry";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceTexelGrid)
            {
                CurrentName = "Texel Grid";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceTexelArea)
            {
                CurrentName = "Texel Area Heatmap";
            }

            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("View");
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::SetNextItemWidth(185.0F);
            ImGui::PushID("ViewportViewCombo");
            const bool bViewComboOpen = ImGui::BeginCombo("##View", CurrentName, ImGuiComboFlags_HeightLarge);

            if (bViewComboOpen)
            {
                const auto DrawSelectionMark = [&](bool bSelected)
                {
                    if (!bSelected)
                    {
                        return;
                    }
                    const ImVec2 Minimum = ImGui::GetItemRectMin();
                    const ImVec2 Maximum = ImGui::GetItemRectMax();
                    const float  Size = ImGui::GetFontSize() * 0.62F;
                    const float  X = Maximum.x - ImGui::GetStyle().FramePadding.x - Size;
                    const float  Y = (Minimum.y + Maximum.y) * 0.5F;
                    const ImU32  Color = ImGui::GetColorU32({0.35F, 0.78F, 1.0F, 1.0F});
                    ImDrawList*  DrawList = ImGui::GetWindowDrawList();
                    DrawList->AddLine({X, Y}, {X + Size * 0.34F, Y + Size * 0.32F}, Color, 2.0F);
                    DrawList->AddLine({X + Size * 0.34F, Y + Size * 0.32F}, {X + Size, Y - Size * 0.42F}, Color, 2.0F);
                };
                DrawSectionHeader("RENDER", 0.0F, 2.0F);
                const auto SelectDisplayView = [&](int Index, const char* Description = nullptr)
                {
                    const auto Mode = static_cast<Rendering::TRenderViewMode>(Index);
                    const bool bSelected = CurrentMode == Mode;
                    const bool bClicked = ImGui::MenuItem(RenderViewModeNames[Index]);
                    DrawSelectionMark(bSelected);
                    if (bClicked)
                    {
                        CurrentMode = Mode;
                        FrameRenderer->SetRenderViewMode(ActiveViewportIndex, Mode);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        SetDescriptionTooltip(Description != nullptr ? Description : GetViewGeometryDescription(Mode));
                    }
                };
                for (int Index = 0; Index < 3; ++Index)
                {
                    SelectDisplayView(Index);
                }
                if (ImGui::BeginMenu("Normal"))
                {
                    SelectDisplayView(3, "원본 메시의 정점 노멀을 월드 공간 RGB로 표시합니다.");
                    SelectDisplayView(4, "원본 메시의 노멀 텍스처를 탄젠트 공간 RGB로 표시합니다.");
                    SelectDisplayView(5, "원본 메시의 매핑된 노멀을 월드 공간 RGB로 표시합니다.");
                    ImGui::EndMenu();
                }
                DrawSectionHeader("DEBUG", 12.0F, 2.0F);
                const auto SelectDebugView =
                    [&](const char* Name, Rendering::TRenderViewMode Mode, const char* Description = nullptr)
                {
                    const bool bMesoGeometry = Mode == Rendering::TRenderViewMode::MesoHeight &&
                                               (CurrentMode == Rendering::TRenderViewMode::MesoHeight ||
                                                CurrentMode == Rendering::TRenderViewMode::MesoOffset);
                    const bool bSelected = CurrentMode == Mode || bMesoGeometry;
                    const bool bClicked = ImGui::MenuItem(Name);
                    DrawSelectionMark(bSelected);
                    if (bClicked)
                    {
                        if (!bMesoGeometry)
                        {
                            CurrentMode = Mode;
                        }
                        FrameRenderer->SetRenderViewMode(ActiveViewportIndex, CurrentMode);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        SetDescriptionTooltip(Description != nullptr ? Description : GetViewGeometryDescription(Mode));
                    }
                };
                SelectDebugView(
                    SurfaceDebugViewNames[0],
                    Rendering::TRenderViewMode::SurfaceStateHeatmap,
                    "원본 메시 위에 State/용량 비율을 색으로 표시합니다. Meso와 적층 변위는 적용하지 않습니다.");
                if (ImGui::BeginMenu("Surface"))
                {
                    SelectDebugView(SurfaceDebugViewNames[1],
                                    Rendering::TRenderViewMode::SurfaceValidity,
                                    "원본 메시 위에 텍셀의 유효성과 시뮬레이션 활성 상태를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[2],
                                    Rendering::TRenderViewMode::SurfaceID,
                                    "원본 메시 위에 Surface ID를 색으로 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[3],
                                    Rendering::TRenderViewMode::NeighborCount,
                                    "원본 메시 위에 텍셀 이웃 수를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[4],
                                    Rendering::TRenderViewMode::SurfaceSeam,
                                    "원본 메시 위에 UV chart 사이의 텍셀 이웃 경계를 표시합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Texel"))
                {
                    SelectDebugView("Texel Grid",
                                    Rendering::TRenderViewMode::SurfaceTexelGrid,
                                    "원본 메시 표면에 시뮬레이션 텍셀 격자를 표시합니다.");
                    SelectDebugView("Texel Area Heatmap",
                                    Rendering::TRenderViewMode::SurfaceTexelArea,
                                    "원본 메시 표면에 텍셀별 월드 면적을 표시합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Geometry"))
                {
                    SelectDebugView("Base Geometry",
                                    Rendering::TRenderViewMode::MacroGeometry,
                                    "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                    SelectDebugView("Meso Geometry",
                                    Rendering::TRenderViewMode::MesoHeight,
                                    "Meso 높이만 형상에 적용합니다. 표시 방식은 View Settings에서 선택합니다.");
                    SelectDebugView(
                        "State Height Heatmap",
                        Rendering::TRenderViewMode::SurfaceAccumulation,
                        "Meso + 선택 State 전체 높이로 형상을 변위합니다. 색은 선택한 적층 성분을 표시합니다.");
                    SelectDebugView("Meso + State Geometry",
                                    Rendering::TRenderViewMode::SurfaceFinalGeometry,
                                    "Meso + 선택 State 적층을 적용한 형상입니다. 선택 State 미리보기이며 모든 State의 "
                                    "최종 합성이 아닙니다.");
                    SelectDebugView("Total Simulation Height",
                                    Rendering::TRenderViewMode::TotalSimulationHeight,
                                    "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Solver"))
                {
                    SelectDebugView(SurfaceDebugViewNames[5],
                                    Rendering::TRenderViewMode::OutgoingFluxScale,
                                    "원본 메시 위에 State별 유출 제한값을 표시합니다.");
                    SelectDebugView("Solver Transfer Weights",
                                    Rendering::TRenderViewMode::SolverTransferWeight,
                                    "원본 메시 위에 선택한 Solver 전달 가중치를 표시합니다.");
                    ImGui::EndMenu();
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            const bool bViewControlHovered = ImGui::IsItemHovered();
            ImGui::SameLine(0.0F, 6.0F);
            ImGui::TextDisabled("View %zu", ActiveViewportIndex + 1U);
            ImGui::SameLine(0.0F, 6.0F);
            if (ViewportPanels.size() == 1U)
            {
                if (ImGui::SmallButton("+ View"))
                {
                    const std::size_t   NewIndex = ViewportPanels.size();
                    const std::uint32_t NewId = NextViewportId++;
                    ViewportPanels.push_back({NewId, glm::vec4(0.0F)});
                    FrameRenderer->SetRenderViewMode(NewIndex, Rendering::TRenderViewMode::SurfaceStateHeatmap);
                }
            }
            else if (ImGui::SmallButton("Remove View"))
            {
                const std::size_t RemoveIndex =
                    ActiveViewportIndex > 0U ? ActiveViewportIndex : ViewportPanels.size() - 1U;
                const std::uint32_t RemoveId = ViewportPanels[RemoveIndex].Id;
                FrameRenderer->SetRenderViewMode(RemoveIndex, Rendering::TRenderViewMode::SurfaceStateHeatmap);
                ViewportPanels.erase(ViewportPanels.begin() + static_cast<std::ptrdiff_t>(RemoveIndex));
                if (ActiveViewportId == RemoveId)
                    ActiveViewportId = ViewportPanels.front().Id;
            }
            if (!bViewComboOpen && bViewControlHovered)
            {
                SetDescriptionTooltip(GetViewGeometryDescription(CurrentMode));
            }

            ImGui::SameLine(0.0F, 10.0F);
            ImGui::TextDisabled("|");

            if (CurrentMode == Rendering::TRenderViewMode::SurfaceStateHeatmap ||
                CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation ||
                CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry)
            {
                ImGui::SameLine(0.0F, 8.0F);
                const SurfaceState::TSurfaceStateRegistry* Registry =
                    AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;
                if (StateCount == 0)
                {
                    ImGui::TextDisabled("State: none");
                }
                else
                {
                    if (DebugState >= StateCount)
                    {
                        DebugState = 0;
                    }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("State");
                    ImGui::SameLine(0.0F, 6.0F);
                    ImGui::SetNextItemWidth(145.0F);
                    const bool  bHeatmap = CurrentMode == Rendering::TRenderViewMode::SurfaceStateHeatmap;
                    const bool  bAllStates = bHeatmap && FrameRenderer->IsStateHeatmapAllStatesEnabled();
                    const char* StateComboId = bHeatmap ? "##ToolbarHeatmapState" : "##ToolbarHeightState";
                    if (ImGui::BeginCombo(StateComboId,
                                          bAllStates ? "All states (max)" : Registry->GetStateName(DebugState).c_str()))
                    {
                        if (bHeatmap && ImGui::Selectable("All states (max)", bAllStates))
                        {
                            FrameRenderer->SetStateHeatmapAllStatesEnabled(true);
                        }
                        for (std::size_t Index = 0; Index < StateCount; ++Index)
                        {
                            const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                            const bool                   bSelected = !bAllStates && State == DebugState;
                            if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                            {
                                DebugState = State;
                                FrameRenderer->SetDebugStateChannel(DebugState);
                                if (bHeatmap)
                                    FrameRenderer->SetStateHeatmapAllStatesEnabled(false);
                            }
                            if (bSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation)
                {
                    ImGui::SameLine(0.0F, 8.0F);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Color");
                    ImGui::SameLine(0.0F, 6.0F);
                    auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    int  Component = static_cast<int>(Settings.AccumulationComponent);
                    ImGui::SetNextItemWidth(185.0F);
                    if (ImGui::Combo(
                            "##ToolbarHeightComponent",
                            &Component,
                            "Total Height\0Cavity Height\0Following Height\0Cavity Fill\0Filled Cavities Only\0"))
                    {
                        Settings.AccumulationComponent = static_cast<std::uint32_t>(Component);
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    }
                }
            }

            ImGui::SameLine(0.0F, 6.0F);
            const float  GearButtonSize = ImGui::GetFrameHeight();
            const bool   bOpenViewSettings = ImGui::Button("##ViewSettingsButton", {GearButtonSize, GearButtonSize});
            const ImVec2 GearMinimum = ImGui::GetItemRectMin();
            const ImVec2 GearMaximum = ImGui::GetItemRectMax();
            const ImVec2 GearCenter{(GearMinimum.x + GearMaximum.x) * 0.5F, (GearMinimum.y + GearMaximum.y) * 0.5F};
            const ImU32  GearColor = ImGui::GetColorU32(ImGuiCol_Text);
            ImDrawList*  DrawList = ImGui::GetWindowDrawList();
            DrawList->AddCircle(GearCenter, 4.5F, GearColor, 8, 1.4F);
            for (int Tooth = 0; Tooth < 8; ++Tooth)
            {
                const float  Angle = glm::two_pi<float>() * static_cast<float>(Tooth) / 8.0F;
                const ImVec2 Direction{std::cos(Angle), std::sin(Angle)};
                DrawList->AddLine({GearCenter.x + Direction.x * 4.5F, GearCenter.y + Direction.y * 4.5F},
                                  {GearCenter.x + Direction.x * 6.3F, GearCenter.y + Direction.y * 6.3F},
                                  GearColor,
                                  1.4F);
            }
            DrawList->AddCircleFilled(GearCenter, 1.7F, ImGui::GetColorU32(ImGuiCol_Button));
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("뷰 설정");
            }
            if (bOpenViewSettings)
            {
                ImGui::OpenPopup("ViewSettingsPopup");
            }

            const ImGuiStyle& Style = ImGui::GetStyle();
            const auto        CheckboxWidth = [&](const char* Label)
            { return ImGui::GetFrameHeight() + Style.ItemInnerSpacing.x + ImGui::CalcTextSize(Label).x; };
            const auto ZoomButtonWidth = [&](const char* Label)
            { return ImGui::CalcTextSize(Label).x + Style.FramePadding.x * 2.0F; };
            const float RightControlsWidth = CheckboxWidth("Grid") + CheckboxWidth("Axis") + CheckboxWidth("Overlays") +
                                             ImGui::CalcTextSize("|").x + ZoomButtonWidth("-") + ZoomButtonWidth("+") +
                                             Style.ItemSpacing.x * 3.0F + 24.0F;
            const float RightControlsStart = ImGui::GetWindowWidth() - Style.WindowPadding.x - RightControlsWidth;
            ImGui::SameLine(0.0F, 6.0F);
            if (RightControlsStart > ImGui::GetCursorPosX() + Style.ItemSpacing.x)
            {
                ImGui::SetCursorPosX(RightControlsStart);
            }

            const bool bDemoPlaybackActive = SceneData.HasDemoAnimation() && SceneData.IsDemoAnimationPlaying();
            ImGui::BeginDisabled(bDemoPlaybackActive);
            bool bGridVisible = FrameRenderer->IsWorldGridVisible();
            if (ImGui::Checkbox("Grid", &bGridVisible))
            {
                FrameRenderer->SetWorldGridVisible(bGridVisible);
            }
            ImGui::SameLine();
            bool bAxisVisible = FrameRenderer->IsWorldAxisVisible();
            if (ImGui::Checkbox("Axis", &bAxisVisible))
            {
                FrameRenderer->SetWorldAxisVisible(bAxisVisible);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Checkbox("Overlays", &bViewportOverlaysVisible);
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("프로파일링 및 렌더 상태 오버레이를 표시하거나 숨깁니다.");
            }
            ImGui::SameLine(0.0F, 10.0F);
            ImGui::TextDisabled("|");

            ImGui::SameLine(0.0F, 10.0F);
            if (ImGui::Button("-"))
            {
                DollyCamera(GetViewportCamera(SceneData, GetActiveViewportIndex()), -1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("축소합니다.");
            }
            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                DollyCamera(GetViewportCamera(SceneData, GetActiveViewportIndex()), 1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("확대합니다.");
            }

            if (bOpenViewSettings || ImGui::IsPopupOpen("ViewSettingsPopup"))
                ImGui::SetNextWindowSizeConstraints({360.0F, 0.0F}, {560.0F, 600.0F});
            if (ImGui::BeginPopup("ViewSettingsPopup"))
            {
                const char* ViewSettingsTitle = CurrentName;
                ImGui::TextColored({0.46F, 0.68F, 0.92F, 1.0F}, "%s", ViewSettingsTitle);
                ImGui::Separator();
                const ImVec4 ContextTitleColor{0.24F, 0.48F, 0.82F, 1.0F};
                auto         BeginViewContext = [&](const char* Title, const char* Description)
                {
                    ImGui::TextColored(ContextTitleColor, "%s", Title);
                    ImGui::TextWrapped("%s", Description);
                    ImGui::PushTextWrapPos(0.0F);
                    ImGui::TextDisabled("형상 기준: %s", GetViewGeometryDescription(CurrentMode));
                    ImGui::PopTextWrapPos();
                    ImGui::Spacing();
                };
                auto DrawHeightGridControls = [&]()
                {
                    auto        Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    int         GridMode = static_cast<int>(Settings.HeightGridMode);
                    const char* Modes[] = {"Off", "Overlay", "Grid only"};
                    bool        Changed = ImGui::Combo("Height grid", &GridMode, Modes, 3);
                    Settings.HeightGridMode = static_cast<std::uint32_t>(GridMode);
                    ImGui::SameLine();
                    int BlockSize = static_cast<int>(Settings.HeightGridBlockSize);
                    ImGui::SetNextItemWidth(130.0F);
                    Changed |= SliderIntWithDoubleClickInput("Cell texels", &BlockSize, 1, 64);
                    Settings.HeightGridBlockSize = static_cast<std::uint32_t>(BlockSize);
                    if (Changed)
                    {
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    }
                };

                switch (CurrentMode)
                {
                    case Rendering::TRenderViewMode::Lit:
                        BeginViewContext("LIT", "재질색 × 조명(노멀 맵 포함).");
                        break;
                    case Rendering::TRenderViewMode::Unlit:
                        BeginViewContext("UNLIT", "조명 없이 재질색만 표시.");
                        break;
                    case Rendering::TRenderViewMode::Wireframe:
                    {
                        BeginViewContext("WIREFRAME", "삼각형 와이어프레임으로 형상을 표시.");
                        bool bUniformWhite = FrameRenderer->IsWireframeUniformWhite();
                        if (ImGui::Checkbox("Uniform white lines", &bUniformWhite))
                        {
                            FrameRenderer->SetWireframeUniformWhite(bUniformWhite);
                        }
                        if (FrameRenderer->SupportsWireframeLineWidth())
                        {
                            ImGui::SameLine(0.0F, 16.0F);
                            float Width = FrameRenderer->GetWireframeLineWidth();
                            ImGui::SetNextItemWidth(180.0F);
                            if (SliderFloatWithDoubleClickInput("Line width",
                                                                &Width,
                                                                FrameRenderer->GetWireframeLineWidthMin(),
                                                                FrameRenderer->GetWireframeLineWidthMax(),
                                                                "%.1f px"))
                            {
                                FrameRenderer->SetWireframeLineWidth(Width);
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("Line width adjustment is unsupported by this GPU.");
                        }
                        break;
                    }
                    case Rendering::TRenderViewMode::VertexNormalWS:
                        BeginViewContext("VERTEX NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::NormalTextureTS:
                        BeginViewContext("NORMAL TEXTURE",
                                         "RGB 채널은 탄젠트 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::MappedNormalWS:
                        BeginViewContext("MAPPED NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::SurfaceStateHeatmap:
                    {
                        BeginViewContext("STATE HEATMAP",
                                         "프로파일 용량 대비 State 비율을 표시합니다. 100% 초과는 주황색이며, 실제 "
                                         "양은 Texel Inspector에서 확인할 수 있습니다.");
                        bool bReliefShadingEnabled = FrameRenderer->IsStateHeatmapReliefShadingEnabled();
                        if (ImGui::Checkbox("Relief Shading", &bReliefShadingEnabled))
                        {
                            FrameRenderer->SetStateHeatmapReliefShadingEnabled(bReliefShadingEnabled);
                        }
                        ImGui::NewLine();
                        DrawLegendColor({0.075F, 0.090F, 0.260F, 1.0F}, "0");
                        DrawLegendColor({0.990F, 0.900F, 0.200F, 1.0F}, "100% 용량");
                        DrawLegendColor({1.0F, 0.25F, 0.05F, 1.0F}, "> 100%");
                        DrawLegendColor({0.42F, 0.42F, 0.45F, 1.0F}, "State 미지원");
                        DrawLegendColor({0.18F, 0.20F, 0.24F, 1.0F}, "프로파일 미할당");
                        DrawLegendColor({1, 0, 1, 1}, "Invalid value / area");
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceAccumulation:
                    case Rendering::TRenderViewMode::SurfaceFinalGeometry:
                    {
                        const bool bFinal = CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry;
                        BeginViewContext(bFinal ? "MESO + STATE GEOMETRY" : "STATE HEIGHT HEATMAP",
                                         bFinal ? "Meso와 선택 State의 높이를 적용한 형상입니다. 선택 State "
                                                  "미리보기이며 모든 State의 최종 합성이 아닙니다."
                                                : "형상에는 Meso와 선택 State 전체 높이를 적용합니다. 색은 선택한 적층 "
                                                  "성분을 표시하며, UV chart 경계는 열려 있습니다.");
                        auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                        bool Changed = false;
                        ImGui::NewLine();
                        ImGui::SetNextItemWidth(95.0F);
                        Changed |= ImGui::DragFloat("Display scale",
                                                    &Settings.DisplacementScale,
                                                    0.1F,
                                                    1e-8F,
                                                    1e6F,
                                                    "%.3gx",
                                                    ImGuiSliderFlags_AlwaysClamp);
                        if (bFinal)
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("Meso + selected State; scale affects display only");
                        }
                        else
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("형상 배율; 색상 기준은 유지");
                            if (Settings.AccumulationComponent < 3)
                            {
                                ImGui::SameLine();
                                ImGui::TextDisabled("프로파일 용량 기준");
                                DrawLegendColor({1, 0.25F, 0.05F, 1}, "> 용량 기준");
                            }
                            else
                            {
                                ImGui::SameLine();
                                ImGui::TextDisabled(Settings.AccumulationComponent == 3
                                                        ? "Cavity Fill: 0-100%%; 초과 높이는 Following에 포함"
                                                        : "채워진 cavity만 표시");
                            }
                        }
                        if (Changed)
                            FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                        ImGui::NewLine();
                        DrawHeightGridControls();
                        if (!bFinal)
                        {
                            if (Settings.AccumulationComponent == 4)
                            {
                                DrawLegendColor({0.12F, 0.13F, 0.15F, 1}, "채워진 cavity 없음");
                            }
                            else
                            {
                                DrawLegendColor({0.075F, 0.090F, 0.260F, 1}, "0");
                            }
                            DrawLegendColor({0.990F, 0.900F, 0.200F, 1},
                                            Settings.AccumulationComponent == 4
                                                ? "채워진 cavity"
                                                : (Settings.AccumulationComponent == 3 ? "100%" : "용량 기준"));
                            DrawLegendColor({1, 0, 1, 1}, "Invalid value / area");
                        }
                        break;
                    }
                    case Rendering::TRenderViewMode::TotalSimulationHeight:
                    {
                        BeginViewContext("TOTAL SIMULATION HEIGHT",
                                         "Meso와 모든 State의 현재 높이를 적용한 형상입니다. "
                                         "높이 캐시는 시뮬레이션 상태나 메시 회전·스케일이 바뀌면 갱신됩니다.");
                        auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                        ImGui::SetNextItemWidth(95.0F);
                        if (ImGui::DragFloat("Display scale",
                                             &Settings.DisplacementScale,
                                             0.1F,
                                             1e-8F,
                                             1e6F,
                                             "%.3gx",
                                             ImGuiSliderFlags_AlwaysClamp))
                            FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                        DrawHeightGridControls();
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceValidity:
                        BeginViewContext("VALIDITY", "Surface 텍셀 유효 여부와 시뮬레이션 적용 여부.");
                        DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "유효·시뮬레이션 켜짐");
                        DrawLegendColor({0.18F, 0.48F, 0.82F, 1.0F}, "유효·시뮬레이션 꺼짐");
                        DrawLegendColor({0.86F, 0.12F, 0.08F, 1.0F}, "무효");
                        break;
                    case Rendering::TRenderViewMode::SurfaceID:
                        BeginViewContext("SURFACE ID",
                                         "색으로 Surface를 구분합니다. 색상 자체는 수치 의미가 없습니다.");
                        break;
                    case Rendering::TRenderViewMode::NeighborCount:
                        BeginViewContext("NEIGHBOR COUNT", "유효한 이웃 텍셀 개수.");
                        DrawLegendColor({0.08F, 0.10F, 0.18F, 1.0F}, "0개");
                        DrawLegendColor({0.95F, 0.72F, 0.12F, 1.0F}, "8개");
                        break;
                    case Rendering::TRenderViewMode::SurfaceSeam:
                        BeginViewContext("UV SEAM", "다른 UV chart로 이어지는 이웃 관계가 있는 경계.");
                        DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "Seam 이웃 있음");
                        DrawLegendColor({0.12F, 0.16F, 0.22F, 1.0F}, "없음");
                        break;
                    case Rendering::TRenderViewMode::OutgoingFluxScale:
                    {
                        BeginViewContext("OUTGOING FLUX SCALE",
                                         "선택 State 채널의 유출 제한값: 0은 제한, 1은 제한 없음.");
                        const SurfaceState::TSurfaceStateRegistry* Registry =
                            AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                        if (Registry != nullptr && Registry->GetStateCount() > 0)
                        {
                            const std::size_t StateCount = Registry->GetStateCount();
                            DebugState = std::min<SurfaceState::TStateId>(
                                DebugState, static_cast<SurfaceState::TStateId>(StateCount - 1));
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted("State");
                            ImGui::SameLine(0.0F, 6.0F);
                            ImGui::SetNextItemWidth(125.0F);
                            if (ImGui::BeginCombo("##OutgoingFluxState", Registry->GetStateName(DebugState).c_str()))
                            {
                                for (std::size_t Index = 0; Index < StateCount; ++Index)
                                {
                                    const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                                    const bool                   bSelected = State == DebugState;
                                    if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                                    {
                                        DebugState = State;
                                        FrameRenderer->SetDebugStateChannel(DebugState);
                                    }
                                    if (bSelected)
                                    {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        }
                        DrawLegendColor({0.76F, 0.08F, 0.10F, 1.0F}, "0 제한");
                        DrawLegendColor({1.00F, 0.70F, 0.10F, 1.0F}, "0.5 부분 제한");
                        DrawLegendColor({0.34F, 0.86F, 0.28F, 1.0F}, "1 제한 없음");
                        DrawLegendColor({0.42F, 0.42F, 0.45F, 1.0F}, "시뮬레이션 꺼짐");
                        break;
                    }
                    case Rendering::TRenderViewMode::SolverTransferWeight:
                    {
                        BeginViewContext("TRANSFER WEIGHT", "선택 가중치 평균: 0은 차단, 1은 완전 전달.");
                        int SelectedWeightView = static_cast<int>(FrameRenderer->GetSolverTransferWeightView());
                        ImGui::SetNextItemWidth(205.0F);
                        if (ImGui::Combo("##TransferWeightComponent",
                                         &SelectedWeightView,
                                         SolverTransferWeightViewNames.data(),
                                         static_cast<int>(SolverTransferWeightViewNames.size())))
                        {
                            FrameRenderer->SetSolverTransferWeightView(
                                static_cast<Rendering::TSolverTransferWeightView>(SelectedWeightView));
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        DrawLegendColor({0.16F, 0.035F, 0.24F, 1.0F}, "0 차단");
                        DrawLegendColor({0.30F, 0.24F, 0.78F, 1.0F}, "0.5 중간");
                        DrawLegendColor({0.18F, 0.94F, 0.98F, 1.0F}, "1 허용");
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceTexelGrid:
                    {
                        BeginViewContext("TEXEL GRID", "시뮬레이션 UV 격자. 확대하면 개별 텍셀 경계가 나타납니다.");
                        const std::uint32_t                  BlockSize = FrameRenderer->GetTexelGridBlockSize();
                        int                                  SelectedBlock = BlockSize == 16U ? 1 : 0;
                        constexpr std::array<const char*, 2> BlockNames = {"8 x 8 texels", "16 x 16 texels"};
                        ImGui::SetNextItemWidth(150.0F);
                        if (ImGui::Combo("##TexelGridBlock",
                                         &SelectedBlock,
                                         BlockNames.data(),
                                         static_cast<int>(BlockNames.size())))
                        {
                            FrameRenderer->SetTexelGridBlockSize(SelectedBlock == 0 ? 8U : 16U);
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        ImGui::Text("Resolution: %u x %u",
                                    FrameRenderer->GetSimulationResolution(),
                                    FrameRenderer->GetSimulationResolution());
                        DrawLegendColor({0.42F, 0.46F, 0.51F, 1.0F}, "1 텍셀");
                        DrawLegendColor({0.72F, 0.88F, 0.98F, 1.0F}, "묶음 경계");
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceTexelArea:
                    {
                        BeginViewContext("TEXEL AREA", "텍셀당 월드 면적. 해상도 변경에도 동일 색 기준.");
                        float Reference = FrameRenderer->GetTexelAreaReference();
                        ImGui::SetNextItemWidth(150.0F);
                        if (ImGui::DragFloat("Reference",
                                             &Reference,
                                             Reference * 0.05F,
                                             1.0e-12F,
                                             1.0e12F,
                                             "%.6g",
                                             ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
                        {
                            FrameRenderer->SetTexelAreaReference(Reference);
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        ImGui::TextDisabled("world units^2 / texel");
                        DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "1/4x 이하 (촘촘)");
                        DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "1x 기준");
                        DrawLegendColor({0.92F, 0.18F, 0.12F, 1.0F}, "4x 이상 (성김)");
                        DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "면적 계산 불가");
                        break;
                    }
                    case Rendering::TRenderViewMode::MesoHeight:
                    case Rendering::TRenderViewMode::MesoOffset:
                    {
                        BeginViewContext("MESO GEOMETRY",
                                         "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.");
                        int Representation = CurrentMode == Rendering::TRenderViewMode::MesoHeight ? 0 : 1;
                        ImGui::SetNextItemWidth(150.0F);
                        if (ImGui::Combo("Representation", &Representation, "Height Color\0Displacement\0"))
                        {
                            CurrentMode = Representation == 0 ? Rendering::TRenderViewMode::MesoHeight
                                                              : Rendering::TRenderViewMode::MesoOffset;
                            FrameRenderer->SetRenderViewMode(ActiveViewportIndex, CurrentMode);
                        }
                        if (CurrentMode == Rendering::TRenderViewMode::MesoHeight)
                        {
                            ImGui::SameLine(0.0F, 10.0F);
                            DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "음수");
                            DrawLegendColor({0.12F, 0.13F, 0.17F, 1.0F}, "0 기준");
                            DrawLegendColor({1.0F, 0.42F, 0.10F, 1.0F}, "양수");
                        }
                        DrawHeightGridControls();
                        break;
                    }
                    case Rendering::TRenderViewMode::MacroGeometry:
                        BeginViewContext("BASE GEOMETRY",
                                         "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                        break;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

} // namespace MDSS
