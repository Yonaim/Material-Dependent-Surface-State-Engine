/**
 * @file DebugUIRender.cpp
 * @brief 렌더 진단 모드와 Surface 외형·overlay 표시 설정 패널을 그린다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSimulation.h"
#include "DebugUI/Core/DebugUIShared.h"
#include "DebugUI/Panels/EditorLayoutStyle.h"

namespace MDSS
{
// ============================================================
// 00. Render Options and Viewport Panels
// ============================================================
#pragma region 00_Render_Options_and_Viewport_Panels
    // viewport별 진단·외형 옵션을 현재 렌더 설정에 반영한다.

    void FDebugUI::DrawViewportToolbar(FScene &SceneData)
    {
        const auto *Node = ImGui::DockBuilderGetCentralNode(ViewportState.DockspaceID);
        const auto *Main = ImGui::GetMainViewport();
        const auto  Size = ImGui::GetIO().DisplaySize;
        if (!Node || Size.x <= 0.F || Size.y <= 0.F)
            return;

        ImGui::PushFont(nullptr, EditorLayout::TitleSize);
        const float TitleHeight = ImGui::GetFontSize();
        ImGui::PopFont();
        const auto &Style = ImGui::GetStyle();
        const bool  bChart = UsesSurfaceSimulation();
        const float ContentWidth = std::max(0.F, Node->Size.x - 2.F * Style.WindowPadding.x);
        const float ShowWidth = ImGui::CalcTextSize("Show").x + 2.F * Style.FramePadding.x;
        const float LayoutWidth = ImGui::CalcTextSize("Layout").x + 2.F * Style.FramePadding.x;
        const float ViewModeWidth =
            std::max(ImGui::GetFrameHeight(),
                     std::min(ContentWidth - ShowWidth - LayoutWidth - 2.F * Style.ItemSpacing.x, 180.F));
        const float LayoutX = Style.WindowPadding.x + std::max(0.F, ContentWidth - LayoutWidth);
        const float Height =
            ImGui::GetFrameHeight() + TitleHeight + Style.ItemSpacing.y + 2.F * Style.WindowPadding.y + 2.F;
        const float TimelineHeight = ImGui::GetFrameHeight() + 2.F * Style.WindowPadding.y;
        ViewportState.ViewportWorkspaceRectNormalized = {(Node->Pos.x - Main->Pos.x) / Size.x,
                                                         (Node->Pos.y + Height + TimelineHeight - Main->Pos.y) / Size.y,
                                                         Node->Size.x / Size.x,
                                                         std::max(1.F, Node->Size.y - Height - TimelineHeight) /
                                                             Size.y};

        ImGui::SetNextWindowPos(Node->Pos);
        ImGui::SetNextWindowSize({Node->Size.x, Height});
        constexpr auto Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                               ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("Viewport toolbar", nullptr, Flags))
        {
            const auto &Path = bChart ? EditorLab->GetScenePath() : SceneData.GetSourcePath();
            const auto  Filename = Path.filename().u8string();
            const auto  Comparison = bChart ? EditorLab->GetComparisonTitle() : std::string_view{};
            const auto  Name = !Comparison.empty() ? std::string(Comparison)
                               : Filename.empty()  ? std::string("Scene")
                                                   : std::string(reinterpret_cast<const char *>(Filename.c_str()));
            const auto  DrawCenteredText = [](const std::string &Text)
            {
                const float  Available = ImGui::GetContentRegionAvail().x;
                const ImVec2 TextSize = ImGui::CalcTextSize(Text.c_str());
                const ImVec2 RowStart = ImGui::GetCursorScreenPos();
                const ImVec2 Start{RowStart.x + std::max(0.F, (Available - TextSize.x) * 0.5F), RowStart.y};
                const ImVec2 End{RowStart.x + Available, RowStart.y + TextSize.y};
                ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),
                                          Start,
                                          End,
                                          End.x,
                                          Text.data(),
                                          Text.data() + Text.size(),
                                          &TextSize);
                ImGui::Dummy({Available, TextSize.y});
                return TextSize.x > Available;
            };
            ImGui::PushFont(nullptr, EditorLayout::TitleSize);
            DrawCenteredText(Name);
            ImGui::PopFont();
            if (!Path.empty() && ImGui::IsItemHovered())
            {
                const auto FullPath = Path.u8string();
                ImGui::SetTooltip("%s", reinterpret_cast<const char *>(FullPath.c_str()));
            }

            ImGui::SetNextItemWidth(ViewModeWidth);
            if (bChart)
                EditorLab->DrawQuickViewControls();
            else
            {
                const auto Active = GetActiveViewportIndex();
                auto       Mode = FrameRenderer->GetRenderViewMode(Active);
                (void)DrawViewModeCombo(Active, Mode, ResolveViewModeName(Mode));
                ImGui::SameLine();
                if (ImGui::Button("Show"))
                    ImGui::OpenPopup("Mesh overlays");
                if (ImGui::BeginPopup("Mesh overlays"))
                {
                    bool bGrid = FrameRenderer->IsWorldGridVisible();
                    bool bAxes = FrameRenderer->IsWorldAxisVisible();
                    if (ImGui::Checkbox("World Grid", &bGrid))
                        FrameRenderer->SetWorldGridVisible(bGrid);
                    if (ImGui::Checkbox("World Axes", &bAxes))
                        FrameRenderer->SetWorldAxisVisible(bAxes);
                    ImGui::EndPopup();
                }
            }

            ImGui::SameLine(LayoutX);
            if (ImGui::Button("Layout"))
                ImGui::OpenPopup("Viewport layout");
            if (ImGui::BeginPopup("Viewport layout"))
            {
                if (bChart)
                    EditorLab->DrawCompareMenu();
                else
                    DrawMeshCompareMenu();
                ImGui::EndPopup();
            }
        }
        ImGui::End();

        // 타임라인에서 클릭해도 뷰포트의 카메라와 주입 영역을 건드리지 않도록 화면 영역을 나눈다.
        ImGui::SetNextWindowPos({Node->Pos.x, Node->Pos.y + Height});
        ImGui::SetNextWindowSize({Node->Size.x, TimelineHeight});
        if (ImGui::Begin("Viewport timeline", nullptr, Flags))
        {
            DrawWorkspaceTimeline(SceneData);
            if (bChart)
            {
                ImGui::SameLine();
                if (ImGui::Button("Scene Tracks"))
                    ImGui::OpenPopup("Active scene tracks");
                const float StatusStart = ImGui::GetItemRectMax().x + Style.ItemSpacing.x;
                const auto  SourceStatus = EditorLab->GetWaterSourceStatus();
                const auto *OutletStatus = EditorLab->GetOutletStatus();
                if (ImGui::BeginPopup("Active scene tracks"))
                {
                    EditorLab->DrawPanel(SurfaceSimulation::EEditorPanel::SceneTracks);
                    ImGui::Separator();
                    if (!SourceStatus.empty())
                        ImGui::TextDisabled("%s", SourceStatus.c_str());
                    ImGui::TextDisabled("%s", OutletStatus);
                    ImGui::EndPopup();
                }
                const auto Summary =
                    SourceStatus.empty() ? std::string(OutletStatus) : SourceStatus + "  |  " + OutletStatus;
                const float SummaryEnd = StatusStart + ImGui::CalcTextSize(Summary.c_str()).x;
                if (SummaryEnd <= Node->Pos.x + Node->Size.x - Style.WindowPadding.x)
                {
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("%s", Summary.c_str());
                }
            }
        }
        ImGui::End();
    }

    void FDebugUI::DrawRenderOptionsWindow(FScene &SceneData)
    {
        if (!FrameRenderer || !ViewportState.bShowRenderOptions)
            return;
        if (ImGui::Begin("Render Options", &ViewportState.bShowRenderOptions))
        {
            if (UsesSurfaceSimulation())
                EditorLab->DrawRenderOptions();
            else
            {
                const auto  Active = GetActiveViewportIndex();
                auto        Mode = FrameRenderer->GetRenderViewMode(Active);
                const char *Name = ResolveViewModeName(Mode);
                DrawSectionHeader(("Active view " + std::to_string(Active + 1)).c_str());
                if (DrawToolbarViewSettingsButton() || ImGui::IsPopupOpen("ViewSettingsPopup"))
                    ImGui::SetNextWindowSizeConstraints({360.F, 0.F}, {550.F, 630.F});
                DrawViewSettingsPopup(Active, Mode, Name, SceneData);
            }
        }
        ImGui::End();
    }

#pragma endregion

// ============================================================
// 01. Toolbar Control Details
// ============================================================
#pragma region 01_Toolbar_Control_Details
    // view 선택, State 선택, 설정 버튼과 카메라 제어를 독립적으로 그린다.

    // 01.00. View Mode Selection
    #pragma region 01_00_View_Mode_Selection
        // 선택된 표시 모드의 이름과 메뉴 항목을 구성하고 Renderer에 선택을 전달한다.

        const char *FDebugUI::ResolveViewModeName(Rendering::ERenderViewMode CurrentMode) const
        {
            const int   CurrentIndex = static_cast<int>(CurrentMode);
            const char *CurrentName = "Solver Transfer Weights";

            if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(RenderViewModeNames.size()))
            {
                CurrentName = RenderViewModeNames[CurrentIndex];
            }
            else if (CurrentMode >= Rendering::ERenderViewMode::SurfaceStateHeatmap &&
                     CurrentMode < Rendering::ERenderViewMode::SolverTransferWeight)
            {
                CurrentName =
                    SurfaceDebugViewNames[CurrentIndex - static_cast<int>(Rendering::ERenderViewMode::SurfaceStateHeatmap)];
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

            return CurrentName;
        }

        bool FDebugUI::DrawViewModeCombo(std::size_t                 ActiveViewportIndex,
                                         Rendering::ERenderViewMode &CurrentMode,
                                         const char                 *CurrentName)
        {
            if (!ImGui::BeginCombo("##View", CurrentName, ImGuiComboFlags_HeightLarge))
                return false;

            using enum Rendering::ERenderViewMode;
            const auto Select = [&](const char *Label, Rendering::ERenderViewMode Mode)
            {
                const bool bSelected = CurrentMode == Mode || (Mode == MesoHeight && CurrentMode == MesoOffset);
                if (ImGui::MenuItem(Label, nullptr, bSelected) && !bSelected)
                {
                    CurrentMode = Mode;
                    FrameRenderer->SetRenderViewMode(ActiveViewportIndex, Mode);
                }
                if (ImGui::IsItemHovered())
                    SetDescriptionTooltip(GetViewGeometryDescription(Mode));
            };
            ImGui::SeparatorText("Appearance");
            Select("Lit", Lit);
            Select("Unlit", Unlit);
            Select("Wireframe", Wireframe);
            ImGui::SeparatorText("Geometry");
            Select("Base Geometry", MacroGeometry);
            Select("Mesogeometry", MesoHeight);
            Select("Selected State Geometry", SurfaceFinalGeometry);
            Select("Deformed Surface", TotalSimulationHeight);
            ImGui::SeparatorText("State");
            Select("Heatmap", SurfaceStateHeatmap);
            Select("Accumulation Height", SurfaceAccumulation);
            ImGui::SeparatorText("Mapping");
            Select("Simulation Cells", SurfaceTexelGrid);
            Select("Validity", SurfaceValidity);
            Select("Surface ID", SurfaceID);
            Select("UV Seams", SurfaceSeam);
            Select("Neighbor Count", NeighborCount);
            Select("Texel Area", SurfaceTexelArea);
            ImGui::SeparatorText("Normals");
            Select("Vertex normal", VertexNormalWS);
            Select("Texture normal", NormalTextureTS);
            Select("Mapped normal", MappedNormalWS);
            ImGui::SeparatorText("Solver");
            Select("Outgoing Flux Scale", OutgoingFluxScale);
            Select("Transfer Weights", SolverTransferWeight);
            ImGui::EndCombo();
            return true;
        }

    #pragma endregion

    // 01.01. Viewport Collection
    #pragma region 01_01_Viewport_Collection
        // 현재 뷰포트 구성을 기준으로 화면 추가·삭제를 처리한다.

        void FDebugUI::SetWorkspaceViewCount(int Count)
        {
            const auto Target = static_cast<std::size_t>(std::clamp(Count, 1, 4));
            auto      &Panels = ViewportState.ViewportPanels;
            while (Panels.size() < Target)
            {
                const auto Index = Panels.size();
                Panels.push_back({ViewportState.NextViewportId++, glm::vec4(0.F)});
                FrameRenderer->SetRenderViewMode(Index, Rendering::ERenderViewMode::SurfaceStateHeatmap);
            }
            Panels.resize(Target);
            if (std::ranges::none_of(Panels, [&](const auto &Panel) { return Panel.Id == ViewportState.ActiveViewportId; }))
                ViewportState.ActiveViewportId = Panels.front().Id;
        }

    #pragma endregion

    void FDebugUI::DrawToolbarStateSelector(Rendering::ERenderViewMode CurrentMode)
    {
        if (CurrentMode == Rendering::ERenderViewMode::SurfaceStateHeatmap ||
            CurrentMode == Rendering::ERenderViewMode::SurfaceAccumulation ||
            CurrentMode == Rendering::ERenderViewMode::SurfaceFinalGeometry)
        {
            const SurfaceState::FSurfaceStateRegistry *Registry =
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
                ImGui::TextUnformatted("Source");
                ImGui::SameLine(0.0F, 6.0F);
                ImGui::SetNextItemWidth(145.0F);
                const bool  bHeatmap = CurrentMode == Rendering::ERenderViewMode::SurfaceStateHeatmap;
                const bool  bAllStates = bHeatmap && FrameRenderer->IsStateHeatmapAllStatesEnabled();
                const char *StateComboId = bHeatmap ? "##ToolbarHeatmapState" : "##ToolbarHeightState";

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
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Color");
                ImGui::SameLine(0.0F, 6.0F);
                auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                int  Component = static_cast<int>(Settings.AccumulationComponent);
                ImGui::SetNextItemWidth(185.0F);

                if (ImGui::Combo("##ToolbarHeightComponent",
                                 &Component,
                                 "Total Height\0Cavity Height\0Following Height\0Cavity Fill\0Filled Cavities Only\0"))
                {
                    Settings.AccumulationComponent = static_cast<std::uint32_t>(Component);
                    FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                }
            }
        }
    }

    bool FDebugUI::DrawToolbarViewSettingsButton()
    {
        const bool bOpenViewSettings = ImGui::Button("Settings");
        if (bOpenViewSettings)
        {
            ImGui::OpenPopup("ViewSettingsPopup");
        }
        return bOpenViewSettings;
    }

    void FDebugUI::DrawMeshCompareMenu()
    {
        using enum Rendering::ERenderViewMode;
        if (ImGui::MenuItem("Exit Compare", nullptr, false, ViewportState.ViewportPanels.size() > 1))
            SetWorkspaceViewCount(1);
        if (ImGui::MenuItem("Geometry & State"))
        {
            SetWorkspaceViewCount(4);
            FrameRenderer->SetRenderViewMode(0, Lit);
            FrameRenderer->SetRenderViewMode(1, MacroGeometry);
            FrameRenderer->SetRenderViewMode(2, MesoHeight);
            FrameRenderer->SetRenderViewMode(3, SurfaceStateHeatmap);
        }
        if (ImGui::MenuItem("Grid & Mapping"))
        {
            SetWorkspaceViewCount(4);
            FrameRenderer->SetRenderViewMode(0, Lit);
            FrameRenderer->SetRenderViewMode(1, SurfaceTexelGrid);
            FrameRenderer->SetRenderViewMode(2, SurfaceValidity);
            FrameRenderer->SetRenderViewMode(3, SurfaceSeam);
        }
    }

#pragma endregion

} // namespace MDSS
