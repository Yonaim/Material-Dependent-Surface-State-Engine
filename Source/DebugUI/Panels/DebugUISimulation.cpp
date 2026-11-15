/**
 * @file DebugUISimulation.cpp
 * @brief Solver 시간·step·저장 형식·sparse 실행 옵션 패널을 그린다.
 */

#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Simulation Debug Panels
// ============================================================
#pragma region 00_Simulation_Debug_Panels
// Solver 시간·storage·sparse 옵션과 진단 값을 한 화면에 연결한다.

    void FDebugUI::DrawSimulationDebugWindow(FScene& SceneData)
    {
        if (AssetManager == nullptr || FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiViewport*  Viewport = ImGui::GetMainViewport();
        constexpr float Width = 360.0F;
        const ImVec2    WindowSize{Width, 520.0F};
        const ImVec2    WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - Width - 10.0F,
                                       Viewport->WorkPos.y + 150.0F};
        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("Simulation Debug", nullptr, Flags))
        {
            DrawSimulationCommonControls();
            ImGui::Dummy({0.0F, 10.0F});
            ImGui::Separator();
            ImGui::Dummy({0.0F, 10.0F});

            // 본문 배경은 부모 패널을 그대로 사용하고, 각 탭의 본문만 스크롤한다.
            if (ImGui::BeginTabBar("SimulationTabs"))
            {
                if (ImGui::BeginTabItem("Settings"))
                {
                    if (ImGui::BeginChild("SimulationSettingsContent",
                                          {0.0F, 0.0F},
                                          ImGuiChildFlags_None,
                                          ImGuiWindowFlags_NoBackground))
                    {
                        DrawSimulationSettingsTab(SceneData);
                        DrawPerformanceTab(false);
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Solver"))
                {
                    if (ImGui::BeginChild(
                            "SolverContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        DrawSolverTab();
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Input"))
                {
                    if (ImGui::BeginChild(
                            "ContactInputContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        DrawContactInputTab();
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Profiles"))
                {
                    if (ImGui::BeginChild(
                            "ProfileTuningContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        DrawProfileTuningTab(SceneData);
                    }

                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        ImGui::End();
    }

    void FDebugUI::DrawDebugStateSelector()
    {
        const auto& Registry = SurfaceDataManager->GetSurfaceStateRegistry();

        if (Registry.GetStateCount() == 0)
        {
            ImGui::TextDisabled("등록된 State 없음");
            return;
        }

        Simulation.DebugState = FrameRenderer->GetDebugStateChannel();
        ImGui::SetNextItemWidth(125.0F);

        if (ImGui::BeginCombo("##InspectorState", Registry.GetStateName(Simulation.DebugState).c_str()))
        {
            for (std::size_t Index = 0; Index < Registry.GetStateCount(); ++Index)
            {
                const auto State = static_cast<SurfaceState::FStateId>(Index);

                if (ImGui::Selectable(Registry.GetStateName(State).c_str(), State == Simulation.DebugState))
                {
                    Simulation.DebugState = State;
                    FrameRenderer->SetDebugStateChannel(State);
                }
            }

            ImGui::EndCombo();
        }
    }

    void FDebugUI::DrawTexelInspectorTab()
    {
        DrawSectionHeader("Texel Inspector");
        TextDescriptionWrapped(
            "Shift + left click on the surface to select a texel. Pause / Step controls remain available above.");
        TextDescriptionWrapped("Picking uses the Macro mesh. Accumulation is a selected-State preview; it does not "
                               "update Solver geometry.");
        DrawDebugStateSelector();
        const auto& Selection = FrameRenderer->GetInspectedTexel();

        if (!Selection)
        {
            ImGui::TextDisabled("선택된 texel 없음");
            return;
        }

        if (ImGui::Button("Clear selection"))
        {
            FrameRenderer->ClearInspectedTexel();
            return;
        }

        ImGui::Text("Instance %zu / Surface %u", Selection->Instance, Selection->Surface);
        ImGui::Text("Texel %u (%u, %u)", Selection->Texel, Selection->XY.x, Selection->XY.y);
        ImGui::Text("Hit triangle %u", Selection->Triangle);
        ImGui::TextWrapped("Profile: %s", Selection->Profile.c_str());
        const auto& Snapshot = FrameRenderer->GetTexelSnapshot();

        if (!Snapshot)
        {
            ImGui::TextDisabled("GPU snapshot 대기 중");
            return;
        }

        const auto& S = *Snapshot;
        ImGui::Text("Completed sample / Step %llu / Buffer %s",
                    static_cast<unsigned long long>(S.Step),
                    S.bStateAB ? "A" : "B");
        const auto                           Status = static_cast<std::uint32_t>(S.Values[2].w);
        constexpr std::array<const char*, 5> StatusNames{
            "Invalid texel", "Unassigned Profile", "Unsupported State", "Valid", "Invalid value / area"};
        ImGui::TextColored(Status == 3 ? ImVec4{0.35F, 0.85F, 0.55F, 1} : ImVec4{1, 0.65F, 0.25F, 1},
                           "%s",
                           StatusNames[std::min(Status, 4U)]);
        if (Status != 3)
            return;
        TextDescriptionWrapped("Values are calculated on the GPU with the same formula used by the debug views. "
                               "Heights use mesh-local units and include the Lit height display scale.");
        if (ImGui::BeginTable("TexelValues", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 1.4F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0F);
            const auto Row = [](const char* Label, float Value)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Label);
                ImGui::TableNextColumn();
                ImGui::Text("%.7g", Value);
            };
            Row("Raw State (total)", S.Values[0].x);
            Row("Reaction substeps", S.Values[6].x);
            Row("Supply-limited reactions", S.Values[6].y);
            Row("Reaction budget exhausted", S.Values[6].z);
            Row("Reaction arithmetic failure", S.Values[6].w);
            Row("Profile Capacity", S.Values[0].y / S.Values[4].y);
            Row("Texel Capacity", S.Values[0].y);
            Row("Saturation", S.Values[0].z);
            Row("Reference-area amount", S.Values[0].w);
            Row("World area", S.Values[4].x);
            Row("Area / reference area", S.Values[4].y);
            Row("Accumulation factor", S.Values[1].x);
            Row("Cavity fill factor", S.Values[1].y);
            Row("Meso height", S.Values[1].z);
            Row("Thickness per amount (world)", S.Values[1].w);
            Row("Lit height display scale", S.Values[4].z);
            Row("World-to-local height", S.Values[4].w);
            Row("Depth to Meso peak", S.Values[2].x);
            Row("Cavity fill (0-1)", S.Values[2].y);
            Row("Cavity excess", S.Values[2].z);
            Row("Meso fill height", S.Values[3].x);
            Row("Following height", S.Values[3].y);
            Row("Accumulation height", S.Values[3].z);
            Row("Final height", S.Values[3].w);
            ImGui::EndTable();
        }

        ImGui::Text("Final local normal (scale 1): %.4g, %.4g, %.4g", S.Values[5].x, S.Values[5].y, S.Values[5].z);
    }

    void FDebugUI::DrawSimulationCommonControls()
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

        BeginLabeledControlRow("Playback");

        if (ImGui::RadioButton("Running", !Simulation.bSimulationPaused))
        {
            Simulation.bSimulationPaused = false;
        }

        ContinueControlRow(RadioWidth("Paused"));

        if (ImGui::RadioButton("Paused", Simulation.bSimulationPaused))
        {
            Simulation.bSimulationPaused = true;
        }

        BeginLabeledControlRow("Controls");
        ImGui::BeginDisabled(!Simulation.bSimulationPaused);

        if (ImGui::Button("Step"))
        {
            Simulation.bSolverStepRequested = true;
        }

        ImGui::EndDisabled();
        ContinueControlRow(ImGui::CalcTextSize("Reset State").x + 2.0F * ImGui::GetStyle().FramePadding.x);

        if (ImGui::Button("Reset State"))
        {
            Simulation.bSolverResetRequested = true;
            Simulation.bFrameTimeResetRequested = true;
        }

        BeginLabeledControlRow("Speed");
        constexpr std::array<std::pair<const char*, float>, 6> SpeedPresets = {
            {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F}, {"2x", 2.0F}, {"4x", 4.0F}, {"8x", 8.0F}}};
        bool bFirstPreset = true;

        for (const auto& [Label, Scale] : SpeedPresets)
        {
            if (!bFirstPreset)
            {
                ContinueControlRow(RadioWidth(Label));
            }

            if (ImGui::RadioButton(Label, std::abs(Simulation.SimulationTimeScale - Scale) < 0.001F))
            {
                Simulation.SimulationTimeScale = Scale;
            }

            bFirstPreset = false;
        }

        ContinueControlRow(160.0F);
        ImGui::SetNextItemWidth(-1.0F);
        SliderFloatWithDoubleClickInput("##Speed", &Simulation.SimulationTimeScale, 0.05F, 8.0F, "%.2fx");
    }

    void FDebugUI::DrawSimulationSettingsTab(FScene& SceneData)
    {
        DrawSectionHeader("Resolution & Time", 0.0F, 2.0F);
        const std::uint32_t CurrentResolution = FrameRenderer->GetSimulationResolution();
        const std::string CurrentLabel = std::to_string(CurrentResolution) + " x " + std::to_string(CurrentResolution);
        const bool        bResolutionOpen = BeginLabeledCombo("Resolution", CurrentLabel.c_str());

        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("표면당 시뮬레이션 해상도입니다. 바꾸면 현재 State가 초기화됩니다.");
        if (bResolutionOpen)
        {
            for (const auto& Preset : SurfaceState::SurfaceSimulationResolutionPresets)
            {
                const bool        bSelected = Preset.Resolution == CurrentResolution;
                const std::string PresetLabel =
                    std::to_string(Preset.Resolution) + " x " + std::to_string(Preset.Resolution);
                if (ImGui::Selectable(PresetLabel.c_str(), bSelected) && !bSelected)
                {
                    Simulation.bFrameTimeResetRequested = true;
                    try
                    {
                        FrameRenderer->SetSimulationResolution(SceneData, Preset.Resolution);
                        ProfileEditing.ResolutionStatus.clear();
                        ResetProfilingAverages();
                    }
                    catch (const std::exception& Error)
                    {
                        ProfileEditing.ResolutionStatus = std::string("Resolution change failed: ") + Error.what();
                        FLogger::Warning("TDebugUI", ProfileEditing.ResolutionStatus);
                    }
                }

                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }

            EndLabeledCombo();
        }

        if (ProfileEditing.ResolutionStatus.starts_with("Resolution change failed:"))
        {
            ImGui::TextWrapped("%s", ProfileEditing.ResolutionStatus.c_str());
        }

        if (ImGui::Checkbox("Fixed timestep", &Simulation.bFixedSimulationTimestep))
        {
            Simulation.bFrameTimeResetRequested = true;
            ResetProfilingAverages();
        }

        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("경과 시간을 누적해 1/60초 고정 간격으로 처리합니다. Profile 배율은 이 간격을 바꾸지 "
                                  "않습니다. 자동 세부 단계 분할은 각 간격을 나눠 처리할 수 있습니다.");
        if (ImGui::Checkbox("Auto substepping", &Simulation.bAutoSubstepping))
        {
            Simulation.bFrameTimeResetRequested = true;
            ResetProfilingAverages();
        }

        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "수송 한계를 기준으로 시뮬레이션 시간을 더 작은 Solver 단계로 나눕니다. GPU 작업량이 늘어납니다. 끄면 "
                "고정 시간 간격이 켜진 경우 1/60초 간격을 사용하고, 꺼진 경우 경과 시간을 사용합니다.");
    }

    void FDebugUI::DrawSolverTab()
    {
        const auto DrawSolverTerm =
            [this](SurfaceState::ESurfaceSolverTerm Term, const char* Label, const char* Description)
        {
            bool bEnabled = FrameRenderer->IsDebugSolverTermEnabled(Term);

            if (ImGui::Checkbox(Label, &bEnabled))
            {
                Simulation.bFrameTimeResetRequested = true;
                FrameRenderer->SetDebugSolverTermEnabled(Term, bEnabled);
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip(Description);
            }
        };

        DrawSectionHeader("State-Driven Geometry", 0.0F, 2.0F);
        bool bStateDrivenGeometryUpdate = FrameRenderer->IsStateDrivenGeometryUpdateEnabled();

        if (ImGui::Checkbox("State-Driven Geometry", &bStateDrivenGeometryUpdate))
        {
            Simulation.bFrameTimeResetRequested = true;
            FrameRenderer->SetStateDrivenGeometryUpdateEnabled(bStateDrivenGeometryUpdate);
            ResetProfilingAverages();
        }

        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "켜짐: 적층 높이가 있는 모든 State를 합산해 이후 수송의 위치·법선·거리 계산에 반영합니다. "
                "꺼짐: 기존 정적 지형으로 수송합니다. 시뮬레이션 두께는 .SRProfile의 thicknessPerAmount를 사용합니다.");
        ImGui::Dummy({0.0F, 6.0F});

        DrawSectionHeader("Transport");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::SaturationDrive,
                       "Saturation spreading",
                       "ON: 포화도 차이에 따른 이웃 전달을 적용합니다.\n"
                       "OFF: 포화도 차이 전달량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::GeometryDrive,
                       "Gravity-guided flow",
                       "ON: 높이와 중력 방향에 따른 전달에 출발 포화도를 곱합니다. 포화도는 1을 넘을 수 있습니다.\n"
                       "OFF: 기하 전달량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::MesoDirectionNormal,
                       "DirectionDrive: MesoNormal",
                       "ON: 중력을 표면에 투영할 때 복원된 MesoNormal을 사용합니다.\n"
                       "OFF: 중력을 표면에 투영할 때 기본 mesh normal을 사용합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::DistanceWeight,
                       "DistanceWeight",
                       "ON: 이웃 간 실제 표면 거리 기반 가중치를 적용합니다.\n"
                       "OFF: 거리 가중치를 1.0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::NormalWeight,
                       "NormalWeight",
                       "ON: 이웃 표면 법선의 방향 일치도 가중치를 적용합니다.\n"
                       "OFF: 법선 가중치를 1.0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::ProfileBoundaryWeight,
                       "ProfileBoundaryWeight",
                       "ON: 서로 다른 Profile 사이 전달 가중치를 0.5로 낮춥니다.\n"
                       "OFF: Profile 경계 가중치를 1.0으로 설정합니다.");

        DrawSectionHeader("Decay");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::Decay,
                       "Decay",
                       "ON: Profile DecayRate에 따른 State 감소를 적용합니다.\n"
                       "OFF: 감소량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::ESurfaceSolverTerm::ConcavityRetention,
                       "Cavity decay protection",
                       "ON: 오목도와 Profile 계수로 Decay 보유량을 조절합니다.\n"
                       "OFF: Decay 보유율을 1.0으로 설정합니다.");
    }

#pragma endregion
} // namespace MDSS
