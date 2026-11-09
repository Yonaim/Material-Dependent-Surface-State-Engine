#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Render Options and Viewport Panels
// ============================================================
#pragma region 00_Render_Options_and_Viewport_Panels

    void FDebugUI::DrawRenderOptionsWindow(FScene& SceneData)
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiDockNode* ViewportNode =
            ViewportState.DockspaceID != 0 ? ImGui::DockBuilderGetCentralNode(ViewportState.DockspaceID) : nullptr;
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
            ViewportState.ViewportWorkspaceRectNormalized = {
                (ViewportNode->Pos.x - MainViewport->Pos.x) / DisplaySize.x,
                (ViewportNode->Pos.y + ToolbarHeight - MainViewport->Pos.y) / DisplaySize.y,
                ViewportNode->Size.x / DisplaySize.x,
                std::max(ViewportNode->Size.y - ToolbarHeight, 1.0F) / DisplaySize.y};
        }

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNavFocus;
        const std::size_t          ActiveViewportIndex = GetActiveViewportIndex();
        Rendering::ERenderViewMode CurrentMode = FrameRenderer->GetRenderViewMode(ActiveViewportIndex);

        if (ImGui::Begin("Render Options##ViewportToolbar", nullptr, Flags))
        {
            const int   CurrentIndex = static_cast<int>(CurrentMode);
            const char* CurrentName = "Solver Transfer Weights";

            if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(RenderViewModeNames.size()))
            {
                CurrentName = RenderViewModeNames[CurrentIndex];
            }
            else if (CurrentMode >= Rendering::ERenderViewMode::SurfaceStateHeatmap &&
                     CurrentMode < Rendering::ERenderViewMode::SolverTransferWeight)
            {
                CurrentName = SurfaceDebugViewNames[CurrentIndex -
                                                    static_cast<int>(Rendering::ERenderViewMode::SurfaceStateHeatmap)];
            }
            else if (CurrentMode == Rendering::ERenderViewMode::SurfaceAccumulation)
                CurrentName = "State Height Heatmap";
            else if (CurrentMode == Rendering::ERenderViewMode::SurfaceFinalGeometry)
                CurrentName = "Meso + State Geometry";
            else if (CurrentMode == Rendering::ERenderViewMode::TotalSimulationHeight)
                CurrentName = "Total Simulation Height";
            else if (CurrentMode == Rendering::ERenderViewMode::MesoHeight ||
                     CurrentMode == Rendering::ERenderViewMode::MesoOffset)
            {
                CurrentName = "Meso Geometry";
            }
            else if (CurrentMode == Rendering::ERenderViewMode::MacroGeometry)
            {
                CurrentName = "Base Geometry";
            }
            else if (CurrentMode == Rendering::ERenderViewMode::SurfaceTexelGrid)
            {
                CurrentName = "Texel Grid";
            }
            else if (CurrentMode == Rendering::ERenderViewMode::SurfaceTexelArea)
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
                    const auto Mode = static_cast<Rendering::ERenderViewMode>(Index);
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
                    [&](const char* Name, Rendering::ERenderViewMode Mode, const char* Description = nullptr)
                {
                    const bool bMesoGeometry = Mode == Rendering::ERenderViewMode::MesoHeight &&
                                               (CurrentMode == Rendering::ERenderViewMode::MesoHeight ||
                                                CurrentMode == Rendering::ERenderViewMode::MesoOffset);
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
                    Rendering::ERenderViewMode::SurfaceStateHeatmap,
                    "원본 메시 위에 State/용량 비율을 색으로 표시합니다. Meso와 적층 변위는 적용하지 않습니다.");
                if (ImGui::BeginMenu("Surface"))
                {
                    SelectDebugView(SurfaceDebugViewNames[1],
                                    Rendering::ERenderViewMode::SurfaceValidity,
                                    "원본 메시 위에 텍셀의 유효성과 시뮬레이션 활성 상태를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[2],
                                    Rendering::ERenderViewMode::SurfaceID,
                                    "원본 메시 위에 Surface ID를 색으로 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[3],
                                    Rendering::ERenderViewMode::NeighborCount,
                                    "원본 메시 위에 텍셀 이웃 수를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[4],
                                    Rendering::ERenderViewMode::SurfaceSeam,
                                    "원본 메시 위에 UV chart 사이의 텍셀 이웃 경계를 표시합니다.");
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Texel"))
                {
                    SelectDebugView("Texel Grid",
                                    Rendering::ERenderViewMode::SurfaceTexelGrid,
                                    "원본 메시 표면에 시뮬레이션 텍셀 격자를 표시합니다.");
                    SelectDebugView("Texel Area Heatmap",
                                    Rendering::ERenderViewMode::SurfaceTexelArea,
                                    "원본 메시 표면에 텍셀별 월드 면적을 표시합니다.");
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Geometry"))
                {
                    SelectDebugView("Base Geometry",
                                    Rendering::ERenderViewMode::MacroGeometry,
                                    "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                    SelectDebugView("Meso Geometry",
                                    Rendering::ERenderViewMode::MesoHeight,
                                    "Meso 높이만 형상에 적용합니다. 표시 방식은 View Settings에서 선택합니다.");
                    SelectDebugView(
                        "State Height Heatmap",
                        Rendering::ERenderViewMode::SurfaceAccumulation,
                        "Meso + 선택 State 전체 높이로 형상을 변위합니다. 색은 선택한 적층 성분을 표시합니다.");
                    SelectDebugView("Meso + State Geometry",
                                    Rendering::ERenderViewMode::SurfaceFinalGeometry,
                                    "Meso + 선택 State 적층을 적용한 형상입니다. 선택 State 미리보기이며 모든 State의 "
                                    "최종 합성이 아닙니다.");
                    SelectDebugView("Total Simulation Height",
                                    Rendering::ERenderViewMode::TotalSimulationHeight,
                                    "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.");
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Solver"))
                {
                    SelectDebugView(SurfaceDebugViewNames[5],
                                    Rendering::ERenderViewMode::OutgoingFluxScale,
                                    "원본 메시 위에 State별 유출 제한값을 표시합니다.");
                    SelectDebugView("Solver Transfer Weights",
                                    Rendering::ERenderViewMode::SolverTransferWeight,
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

            if (ViewportState.ViewportPanels.size() == 1U)
            {
                if (ImGui::SmallButton("+ View"))
                {
                    const std::size_t   NewIndex = ViewportState.ViewportPanels.size();
                    const std::uint32_t NewId = ViewportState.NextViewportId++;
                    ViewportState.ViewportPanels.push_back({NewId, glm::vec4(0.0F)});
                    FrameRenderer->SetRenderViewMode(NewIndex, Rendering::ERenderViewMode::SurfaceStateHeatmap);
                }
            }
            else if (ImGui::SmallButton("Remove View"))
            {
                const std::size_t RemoveIndex =
                    ActiveViewportIndex > 0U ? ActiveViewportIndex : ViewportState.ViewportPanels.size() - 1U;
                const std::uint32_t RemoveId = ViewportState.ViewportPanels[RemoveIndex].Id;
                FrameRenderer->SetRenderViewMode(RemoveIndex, Rendering::ERenderViewMode::SurfaceStateHeatmap);
                ViewportState.ViewportPanels.erase(ViewportState.ViewportPanels.begin() +
                                                   static_cast<std::ptrdiff_t>(RemoveIndex));
                if (ViewportState.ActiveViewportId == RemoveId)
                    ViewportState.ActiveViewportId = ViewportState.ViewportPanels.front().Id;
            }

            if (!bViewComboOpen && bViewControlHovered)
            {
                SetDescriptionTooltip(GetViewGeometryDescription(CurrentMode));
            }

            ImGui::SameLine(0.0F, 10.0F);
            ImGui::TextDisabled("|");

            if (CurrentMode == Rendering::ERenderViewMode::SurfaceStateHeatmap ||
                CurrentMode == Rendering::ERenderViewMode::SurfaceAccumulation ||
                CurrentMode == Rendering::ERenderViewMode::SurfaceFinalGeometry)
            {
                ImGui::SameLine(0.0F, 8.0F);
                const SurfaceState::FSurfaceStateRegistry* Registry =
                    AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;

                if (StateCount == 0)
                {
                    ImGui::TextDisabled("State: none");
                }
                else
                {
                    if (Simulation.DebugState >= StateCount)
                    {
                        Simulation.DebugState = 0;
                    }

                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("State");
                    ImGui::SameLine(0.0F, 6.0F);
                    ImGui::SetNextItemWidth(145.0F);
                    const bool  bHeatmap = CurrentMode == Rendering::ERenderViewMode::SurfaceStateHeatmap;
                    const bool  bAllStates = bHeatmap && FrameRenderer->IsStateHeatmapAllStatesEnabled();
                    const char* StateComboId = bHeatmap ? "##ToolbarHeatmapState" : "##ToolbarHeightState";

                    if (ImGui::BeginCombo(StateComboId,
                                          bAllStates ? "All states (max)"
                                                     : Registry->GetStateName(Simulation.DebugState).c_str()))
                    {
                        if (bHeatmap && ImGui::Selectable("All states (max)", bAllStates))
                        {
                            FrameRenderer->SetStateHeatmapAllStatesEnabled(true);
                        }

                        for (std::size_t Index = 0; Index < StateCount; ++Index)
                        {
                            const SurfaceState::FStateId State = static_cast<SurfaceState::FStateId>(Index);
                            const bool                   bSelected = !bAllStates && State == Simulation.DebugState;

                            if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                            {
                                Simulation.DebugState = State;
                                FrameRenderer->SetDebugStateChannel(Simulation.DebugState);

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

                if (CurrentMode == Rendering::ERenderViewMode::SurfaceAccumulation)
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
            DrawViewSettingsPopup(ActiveViewportIndex, CurrentMode, CurrentName);
        }

        ImGui::End();
    }

#pragma endregion

} // namespace MDSS
