/**
 * @file DebugUIProfiling.cpp
 * @brief GPU·CPU 시간과 자원 메모리의 프로파일링 UI를 표시한다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSimulation.h"
#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Performance Overlays
// ============================================================
#pragma region 00_Performance_Overlays
    // 수집한 시간·메모리 지표를 현재 화면의 overlay에 표시한다.

    void FDebugUI::ResetProfilingAverages() noexcept
    {
        Profiling.Reset();
        DisplayedProfilingStats.reset();
    }

    void FDebugUI::DrawProfilingWindow()
    {
        if (!FrameRenderer || !ViewportState.bShowProfiling)
            return;

        ImGui::SetNextWindowSize({680.F, 475.F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Profiling", &ViewportState.bShowProfiling) && ImGui::BeginTabBar("ProfilingTabs"))
        {
            RefreshProfilingReadings();
            if (ImGui::BeginTabItem("Timings"))
            {
                DrawProfilingTimings();
                ImGui::EndTabItem();
            }
            if (UsesSurfaceSimulation() && ImGui::BeginTabItem("Workload"))
            {
                EditorLab->DrawProfilingWorkload();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GPU Memory"))
            {
                DrawMemoryProfilingContents();
                ImGui::EndTabItem();
            }
            if (!UsesSurfaceSimulation())
            {
                if (ImGui::BeginTabItem("Render State"))
                {
                    DrawRenderStateProfilingContents();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Solver Options"))
                {
                    DrawPerformanceTab(false);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Render Options"))
                {
                    DrawPerformanceTab(true);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

#pragma endregion

// ============================================================
// 01. Profiling Overlay Details
// ============================================================
#pragma region 01_Profiling_Overlay_Details
    // 시간·GPU 메모리·효과 활성 상태를 서로 독립된 화면으로 표시한다.

    static std::array<float, 40> CollectTimeProfilingMetrics(const Rendering::FRendererProfilingStats &Stats,
                                                             bool bFineRenderTimings)
    {
        const float StateDrivenGeometryUpdateGpuMilliseconds =
            Stats.AccumulationGeometryGpuMilliseconds >= 0.0F && Stats.TransferWeightGpuMilliseconds >= 0.0F
                ? Stats.AccumulationGeometryGpuMilliseconds + Stats.TransferWeightGpuMilliseconds
                : -1.0F;

        return {
            Stats.RenderPreparationGpuMilliseconds + Stats.SceneDrawGpuMilliseconds +
                (bFineRenderTimings ? Stats.UIDrawGpuMilliseconds + Stats.RenderPassEndGpuMilliseconds : 0.0F),
            Stats.SolverGpuMilliseconds,
            StateDrivenGeometryUpdateGpuMilliseconds,
            Stats.SolverPass1GpuMilliseconds,
            Stats.SolverPass2GpuMilliseconds,
            Stats.RenderPreparationGpuMilliseconds,
            Stats.SceneDrawGpuMilliseconds,
            Stats.TexelInspectorGpuMilliseconds,
            Stats.FrameFenceWaitCpuMilliseconds,
            Stats.AcquireCpuMilliseconds,
            Stats.CommandRecordCpuMilliseconds,
            Stats.QueueSubmitCpuMilliseconds,
            Stats.PresentCpuMilliseconds,
            Stats.BaseMeshDrawGpuMilliseconds,
            Stats.MudOverlayDrawGpuMilliseconds,
            Stats.ReservedOverlayDrawGpuMilliseconds,
            Stats.OverlayPreparationGpuMilliseconds,
            Stats.OverlayGeometryGpuMilliseconds,
            Stats.OverlaySmoothingGpuMilliseconds,
            Stats.OverlaySidesGpuMilliseconds,
            Stats.SceneSetupGpuMilliseconds,
            Stats.SceneBetweenDrawsGpuMilliseconds,
            Stats.SceneTailGpuMilliseconds,
            Stats.OverlaySidesPreBarrierGpuMilliseconds,
            Stats.OverlaySidesDispatchGpuMilliseconds,
            Stats.OverlaySidesPostBarrierGpuMilliseconds,
            Stats.RenderPassBeginGpuMilliseconds,
            Stats.ViewportSetupGpuMilliseconds,
            Stats.OverlayBoundarySearchGpuMilliseconds,
            Stats.OverlayTopCommandGpuMilliseconds,
            Stats.OverlaySidesInterPassBarrierGpuMilliseconds,
            Stats.OverlayCoverageSampleGpuMilliseconds,
            Stats.RenderPassColorStageGpuMilliseconds,
            Stats.RenderPassDepthStageGpuMilliseconds,
            Stats.UIDrawGpuMilliseconds,
            Stats.RenderPassEndGpuMilliseconds,
            Stats.AccumulationGeometryGpuMilliseconds,
            Stats.TransferWeightGpuMilliseconds,
            Stats.OverlayHeightGpuMilliseconds,
            Stats.OverlayNormalVertexGpuMilliseconds,
        };
    }

    void FDebugUI::RefreshProfilingReadings()
    {
        const bool bChart = UsesSurfaceSimulation();
        if (bProfilingChartScene != bChart)
        {
            ResetProfilingAverages();
            bProfilingChartScene = bChart;
        }

        const auto  &Latest = FrameRenderer->GetProfilingStats();
        const bool   bFineRenderTimings = FrameRenderer->AreRenderPassSubstageTimingsReliable();
        const double FrameSeconds = static_cast<double>(ImGui::GetIO().DeltaTime);
        if (!bHoldProfilingReadings && std::isfinite(FrameSeconds) && FrameSeconds > 0.0 &&
            (bChart || Latest.OverlayDisplayMode == FrameRenderer->GetOverlayDisplayMode()))
            Profiling.Accumulate(FrameSeconds, CollectTimeProfilingMetrics(Latest, bFineRenderTimings));

        const bool bPublished = Profiling.PublishWindow();
        const bool bRefresh = bPublished || !DisplayedProfilingStats;
        if (bRefresh)
        {
            DisplayedProfilingStats = Latest;
            DisplayedMeshSimulationTime = static_cast<float>(FrameRenderer->GetSimulatedSeconds());
            if (bChart)
                EditorLab->RefreshProfilingReadings();
        }
    }

    void FDebugUI::DrawProfilingTimings()
    {
        const bool bChart = UsesSurfaceSimulation();
        const bool bFineRenderTimings = FrameRenderer->AreRenderPassSubstageTimingsReliable();
        ImGui::Checkbox("Hold readings", &bHoldProfilingReadings);
        ImGui::SameLine();
        ImGui::TextDisabled("Display refresh: 1 s");
        if (!bChart)
        {
            DrawTimeProfilingContents(*DisplayedProfilingStats, bFineRenderTimings);
            return;
        }

        EditorLab->DrawPanel(SurfaceSimulation::EEditorPanel::Profiler);
        DrawSectionHeader("CPU Frame (ms)");
        if (Profiling.bAverageAvailable)
            ImGui::Text("FPS %.1f   Frame interval %.2f ms", Profiling.Averages[0], Profiling.Averages[1]);

        if (ImGui::BeginTable("ChartCpuTimings", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch, 2.2F);
            ImGui::TableSetupColumn("Latest", ImGuiTableColumnFlags_WidthStretch, 1.F);
            ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthStretch, 1.F);
            ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthStretch, 1.F);
            ImGui::TableHeadersRow();
            const auto      &Stats = *DisplayedProfilingStats;
            const std::array Rows{std::pair{"Frame fence wait", Stats.FrameFenceWaitCpuMilliseconds},
                                  std::pair{"Acquire swapchain image", Stats.AcquireCpuMilliseconds},
                                  std::pair{"Record commands", Stats.CommandRecordCpuMilliseconds},
                                  std::pair{"Submit queue work", Stats.QueueSubmitCpuMilliseconds},
                                  std::pair{"Present", Stats.PresentCpuMilliseconds}};
            for (std::size_t Index = 0; Index < Rows.size(); ++Index)
            {
                const auto &[Label, Duration] = Rows[Index];
                const auto MetricIndex = 10U + Index;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", Label);
                for (const auto Value : {Duration,
                                         Profiling.bAverageAvailable ? Profiling.Averages[MetricIndex] : -1.F,
                                         Profiling.bAverageAvailable ? Profiling.Maximums[MetricIndex] : -1.F})
                {
                    ImGui::TableNextColumn();
                    if (Value >= 0.F)
                        ImGui::Text("%.3f", Value);
                    else
                        ImGui::TextDisabled("-");
                }
            }
            ImGui::EndTable();
        }
    }

    void FDebugUI::DrawTimeProfilingContents(const Rendering::FRendererProfilingStats &Stats, bool bFineRenderTimings)
    {
        DrawSectionHeader("Frame Summary (ms)");
        {
            const float  FPS = Profiling.Averages[0];
            const ImVec4 FPSColor = FPS >= 55.0F   ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F)
                                    : FPS >= 30.0F ? ImVec4(1.0F, 0.84F, 0.47F, 1.0F)
                                                   : ImVec4(1.0F, 0.57F, 0.57F, 1.0F);
            if (Profiling.bAverageAvailable)
                ImGui::TextColored(FPSColor, "FPS: %.1f", FPS);
            else
                ImGui::TextDisabled("FPS: collecting 1s average");
            const auto SetupMetricTable = [](const char *Id)
            {
                constexpr int ColumnCount = 4;

                if (!ImGui::BeginTable(Id,
                                       ColumnCount,
                                       ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                                           ImGuiTableFlags_NoSavedSettings))
                    return false;
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch, 2.45F);
                ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthStretch, 0.9F);
                ImGui::TableSetupColumn("Peak", ImGuiTableColumnFlags_WidthStretch, 0.9F);
                ImGui::TableSetupColumn("100 ms", ImGuiTableColumnFlags_WidthStretch, 1.F);
                ImGui::TableHeadersRow();

                return true;
            };
            const auto DrawMetric =
                [this](std::size_t Index, const char *Label, ImVec4 Color, const char *Tooltip = nullptr)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", Label);

                if (Tooltip != nullptr && ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", Tooltip);
                int        Column = 1;
                const auto DrawValue = [&](float Value, bool bReady)
                {
                    ImGui::TableSetColumnIndex(Column++);

                    if (Value >= 0.0F)
                        ImGui::TextColored(Color, "%.2f", Value);
                    else
                        ImGui::TextDisabled(bReady ? "—" : "…");
                };
                DrawValue(Profiling.Averages[Index + 2], Profiling.bAverageAvailable);
                DrawValue(Profiling.Maximums[Index + 2], true);
                DrawValue(Profiling.PublishedRecentAverages[Index + 2], true);
            };

            if (SetupMetricTable("ViewportProfilingSummary"))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Frame time");
                const auto DrawFrameValue = [&](float Value, bool bReady)
                {
                    const int Column = ImGui::TableGetColumnIndex() + 1;
                    ImGui::TableSetColumnIndex(Column);

                    if (Value >= 0.0F)
                        ImGui::TextColored(FPSColor, "%.2f", Value);
                    else
                        ImGui::TextDisabled(bReady ? "—" : "…");
                };
                DrawFrameValue(Profiling.Averages[1], Profiling.bAverageAvailable);
                DrawFrameValue(Profiling.Maximums[1], true);
                DrawFrameValue(Profiling.PublishedRecentAverages[1], true);
                DrawMetric(0, "Render GPU", {0.82F, 0.87F, 0.94F, 1.0F});
                DrawMetric(1, "Solver GPU / frame", {0.82F, 0.87F, 0.94F, 1.0F});
                ImGui::EndTable();
            }

            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Header, {0.0F, 0.0F, 0.0F, 0.0F});
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.30F, 0.32F, 0.35F, 0.35F});
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0.0F, 0.0F, 0.0F, 0.0F});

            if (ImGui::TreeNodeEx("GPU: Simulation", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (SetupMetricTable("ViewportProfilingSimulation"))
                {
                    DrawMetric(2, "Accumulation Height + Geometry / frame", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(3, "Pass 1 / frame", {0.72F, 0.78F, 0.87F, 1.0F});
                    DrawMetric(4, "Pass 2 / frame", {0.72F, 0.78F, 0.87F, 1.0F});
                    DrawMetric(36, "Height + geometry build / frame", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(37, "Transfer weights / frame", {0.82F, 0.87F, 0.94F, 1.0F});
                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("WORKLOAD & SETTINGS");

                if (ImGui::BeginTable("ViewportProfilingWorkload",
                                      2,
                                      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
                {
                    ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, 130.0F);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 165.0F);
                    const auto AddStateRow = [](const char *Label, const char *Format, auto... Values)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("%s", Label);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled(Format, Values...);
                    };
                    AddStateRow("Steps per frame", "%u", Stats.SimulationSteps);
                    AddStateRow("Maximum step", "%.3f ms", 1000.0F * FrameRenderer->GetMaximumSimulationStep());
                    AddStateRow("Simulated this frame", "%.2f ms", Stats.SimulatedThisFrameSeconds * 1000.0F);
                    AddStateRow("Simulation time", "%.2f s", DisplayedMeshSimulationTime);
                    AddStateRow("Pending simulation", "%.2f ms", Stats.PendingSimulationSeconds * 1000.0F);
                    AddStateRow("Time skipped", "%.2f ms", Stats.DroppedSimulationSeconds * 1000.0F);
                    AddStateRow("Instances", "%u", Stats.SimulationInstances);
                    AddStateRow("Texels / Ch.",
                                "%llu / %u",
                                static_cast<unsigned long long>(Stats.SimulationTexels),
                                Stats.StateChannels);
                    AddStateRow("Resolution", "%u x %u", Stats.SimulationResolution, Stats.SimulationResolution);
                    AddStateRow("Surface mesh", "%u", Stats.SurfaceTexelMeshResolution);
                    AddStateRow("Overlay mesh", "%u", Stats.OverlayTexelMeshResolution);
                    AddStateRow("State-Driven Geometry",
                                "%s",
                                FrameRenderer->IsStateDrivenGeometryUpdateEnabled() ? "ON" : "OFF");
                    AddStateRow("Fixed timestep", "%s", Simulation.bFixedSimulationTimestep ? "ON" : "OFF");
                    AddStateRow("Auto substepping", "%s", Simulation.bAutoSubstepping ? "ON" : "OFF");
                    AddStateRow("Time scale", "%.2fx", Simulation.SimulationTimeScale);
                    const auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
                    AddStateRow("Height Smoothing", "%s", Effects.bHeightFieldSmoothing ? "ON" : "OFF");
                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("GPU: Rendering", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (Stats.OverlayTotalTopTriangles > 0 &&
                    Stats.OverlayDisplayMode == FrameRenderer->GetOverlayDisplayMode())
                {
                    if (Stats.OverlayTotalTiles > 0)
                    {
                        const double ActiveTilePercent = 100.0 * static_cast<double>(Stats.OverlayActiveTiles) /
                                                         static_cast<double>(Stats.OverlayTotalTiles);
                        ImGui::TextDisabled("Active overlay tiles: %.1f%%", ActiveTilePercent);

                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Overlay occupancy scan에서 하나 이상의 상태값이 있는 것으로 "
                                              "판정된 UV 타일입니다.");
                        ImGui::TextDisabled("%llu / %llu tiles",
                                            static_cast<unsigned long long>(Stats.OverlayActiveTiles),
                                            static_cast<unsigned long long>(Stats.OverlayTotalTiles));
                    }

                    const double ActivePercent = 100.0 * static_cast<double>(Stats.OverlayActiveTopTriangles) /
                                                 static_cast<double>(Stats.OverlayTotalTopTriangles);
                    ImGui::TextDisabled("Active top triangles: %.1f%%", ActivePercent);

                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("정점 하나 이상이 2%% 커버리지 기준을 넘으면 활성 삼각형으로 계산합니다. "
                                          "활성화된 레이어는 각각 집계하며, 비율은 면적 가중치가 적용되지 않습니다.");
                    ImGui::TextDisabled("%llu / %llu triangles",
                                        static_cast<unsigned long long>(Stats.OverlayActiveTopTriangles),
                                        static_cast<unsigned long long>(Stats.OverlayTotalTopTriangles));
                }

                if (SetupMetricTable("ViewportProfilingRendering"))
                {
                    DrawMetric(5, "Render prep", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(6, bFineRenderTimings ? "Scene draw" : "Render pass", {0.82F, 0.87F, 0.94F, 1.0F});

                    if (bFineRenderTimings)
                    {
                        DrawMetric(34, "UI draw", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(35, "Render pass end", {0.82F, 0.87F, 0.94F, 1.0F});
                    }

                    DrawMetric(7, "Texel inspector", {0.82F, 0.87F, 0.94F, 1.0F});
                    ImGui::EndTable();
                }

                if (BeginTextTreeNode("Render prep breakdown"))
                {
                    if (SetupMetricTable("ViewportProfilingRenderPrepBreakdown"))
                    {
                        DrawMetric(16, "Overlay prep", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(17, "Overlay geometry", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(38,
                                   "  Height pass",
                                   {0.82F, 0.87F, 0.94F, 1.0F},
                                   "Height dispatch and its dependency barrier.");
                        DrawMetric(39,
                                   "  Normal + vertex pass",
                                   {0.82F, 0.87F, 0.94F, 1.0F},
                                   "Normal/vertex dispatch and its output barrier.");
                        DrawMetric(18, "Height smoothing", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(19, "Overlay sides", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(23, "  Pre-write barrier", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(24, "  Total dispatches", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(31, "    Coverage sample", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(28, "    Boundary + segment", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(29,
                                   "    Top command (merged)",
                                   {0.82F, 0.87F, 0.94F, 1.0F},
                                   "Top surface activation now runs inside Boundary + segment; this slot should be "
                                   "near zero.");
                        DrawMetric(30, "    Coverage barrier", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(25,
                                   "  Pre-draw sync",
                                   {0.82F, 0.87F, 0.94F, 1.0F},
                                   "Includes activity counter readback and draw barriers.");
                        ImGui::EndTable();
                    }

                    ImGui::TreePop();
                }

                if (!bFineRenderTimings)
                {
                    TextDescriptionWrapped("MoltenVK on Apple GPUs records in-pass timestamps at the end "
                                           "of the render pass. Per-draw GPU timings are unavailable here.");
                }

                if (bFineRenderTimings && BeginTextTreeNode("Scene draw breakdown"))
                {
                    if (SetupMetricTable("ViewportProfilingSceneDrawBreakdown"))
                    {
                        DrawMetric(20, "Render pass setup", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(26,
                                   "  Pass begin total",
                                   {0.72F, 0.78F, 0.87F, 1.0F},
                                   "Time around render-pass begin; may include image availability waits.");
                        DrawMetric(32,
                                   "    Color stage",
                                   {0.72F, 0.78F, 0.87F, 1.0F},
                                   "Color-output timestamps around pass begin. This overlaps the total.");
                        DrawMetric(33,
                                   "    Depth stage",
                                   {0.72F, 0.78F, 0.87F, 1.0F},
                                   "Early-depth timestamps around pass begin. This overlaps the total.");
                        DrawMetric(27, "  Viewport state setup", {0.72F, 0.78F, 0.87F, 1.0F});
                        DrawMetric(13, "Base mesh draw", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(14, "Mud overlay draw", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(15, "Other overlay draw", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(21, "Between draws", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(22, "Grid / gizmo", {0.82F, 0.87F, 0.94F, 1.0F});
                        ImGui::EndTable();
                    }

                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("CPU Frame", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (SetupMetricTable("ViewportProfilingCPU"))
                {
                    DrawMetric(8, "Fence wait", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(9, "Acquire", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(10, "Command record", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(11, "Queue submit", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(12, "Present call", {0.82F, 0.87F, 0.94F, 1.0F});
                    ImGui::EndTable();
                }

                ImGui::TreePop();
            }

            ImGui::PopStyleColor(3);
        }
    }

    void FDebugUI::DrawMemoryProfilingContents()
    {
        const GPU::FGPUResourceMemorySnapshot Memory = GPU::FGPUResourceMemoryStats::GetSnapshot();
        constexpr double                      BytesPerMiB = 1024.0 * 1024.0;
        const ImVec4                          TotalColor{1.0F, 0.78F, 0.24F, 1.0F};
        const ImVec4                          GroupColor{0.60F, 0.80F, 0.98F, 1.0F};
        const auto                            DrawMemoryRow = [&](const char *Label, std::uint64_t CurrentBytes)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%s", Label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f MiB", static_cast<double>(CurrentBytes) / BytesPerMiB);
        };
        const auto DrawMemoryTotalRow = [&](const char *Label, std::uint64_t CurrentBytes, ImVec4 Color)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(Color, "%s", Label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(Color, "%.2f MiB", static_cast<double>(CurrentBytes) / BytesPerMiB);
        };
        const auto DrawMemoryTreeRow = [&](const char *Label, std::uint64_t CurrentBytes)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, GroupColor);
            const bool bOpen = ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(GroupColor, "%.2f MiB", static_cast<double>(CurrentBytes) / BytesPerMiB);

            return bOpen;
        };

        if (ImGui::BeginTable("ViewportMemoryProfiling",
                              2,
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed, 200.0F);
            ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.0F);
            const auto CategoryBytes = [&](GPU::EGPUBufferMemoryCategory Category)
            { return Memory.BufferCategoryCurrentBytes[static_cast<std::size_t>(Category)]; };
            const std::uint64_t RenderMeshBytes = CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh64) +
                                                  CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh128) +
                                                  CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh256) +
                                                  CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh512);
            const std::uint64_t SurfaceCoreBytes = CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceState) +
                                                   CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceGeometry) +
                                                   CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceProfile);
            const std::uint64_t SurfaceRuntimeBytes = CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceRuntime);
            const std::uint64_t SurfaceSystemBytes = SurfaceCoreBytes + RenderMeshBytes + SurfaceRuntimeBytes;
            const std::uint64_t SceneEditorBufferBytes = CategoryBytes(GPU::EGPUBufferMemoryCategory::MeshAsset) +
                                                         CategoryBytes(GPU::EGPUBufferMemoryCategory::Renderer);
            DrawMemoryTotalRow("GPU total", Memory.CurrentTotalBytes, TotalColor);
            DrawMemoryRow("GPU peak", Memory.PeakTotalBytes);

            if (DrawMemoryTreeRow("MDSS Surface System", SurfaceSystemBytes))
            {
                if (DrawMemoryTreeRow("Core data", SurfaceCoreBytes))
                {
                    DrawMemoryRow("Surface State", CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceState));
                    DrawMemoryRow("Surface Geometry", CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceGeometry));
                    DrawMemoryRow("Profile Tables", CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceProfile));
                    ImGui::TreePop();
                }

                if (DrawMemoryTreeRow("Surface rendering", RenderMeshBytes))
                {
                    DrawMemoryRow("512 resolution", CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh512));
                    DrawMemoryRow("256 resolution", CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh256));
                    DrawMemoryRow("128 resolution", CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh128));
                    DrawMemoryRow("64 resolution", CategoryBytes(GPU::EGPUBufferMemoryCategory::RenderMesh64));
                    ImGui::TreePop();
                }

                DrawMemoryRow("Surface runtime buffers", SurfaceRuntimeBytes);
                ImGui::TreePop();
            }

            if (DrawMemoryTreeRow("Scene & Editor", SceneEditorBufferBytes))
            {
                DrawMemoryRow("Mesh Assets", CategoryBytes(GPU::EGPUBufferMemoryCategory::MeshAsset));
                DrawMemoryRow("Renderer buffers", CategoryBytes(GPU::EGPUBufferMemoryCategory::Renderer));
                ImGui::TreePop();
            }

            DrawMemoryRow("Debug", CategoryBytes(GPU::EGPUBufferMemoryCategory::Debug));
            DrawMemoryRow("Other / Staging", CategoryBytes(GPU::EGPUBufferMemoryCategory::Other));
            DrawMemoryTotalRow("Images", Memory.CurrentImageBytes, GroupColor);
            ImGui::EndTable();
        }

        ImGui::TextDisabled("Tracked Vulkan allocations · MiB");
        ImGui::TextDisabled("MDSS totals include surface rendering and runtime buffers.");

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "MDSS Surface System includes core surface data, generated surface render meshes, "
                "and runtime buffers for smoothing and accumulation overlays. Scene & Editor "
                "includes source mesh assets and general renderer buffers. Values use Vulkan "
                "memory requirements for app-owned TGPUBuffer and TGPUImage allocations; "
                "swapchain and driver allocations are excluded. Other includes transient staging buffers.");
        if (!UsesSurfaceSimulation())
        {
            const auto PrecisionMemory = EstimatePerformanceMemory(FrameRenderer->GetSurfaceGPUResources());
            ImGui::TextDisabled(
                "Performance options use preallocated buffers; toggles do not change current allocations.");
            ImGui::TextDisabled("FP16 compact allocation estimate");
            ImGui::Text("Raw Flux savings: %.2f MiB", PrecisionMemory.RawFluxSavingsBytes / BytesPerMiB);
            ImGui::Text("TransferWeight savings: %.2f MiB", PrecisionMemory.DynamicWeightSavingsBytes / BytesPerMiB);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Estimate from the loaded instances' texel and channel counts. These bytes are not "
                                  "subtracted from Current because the A/B buffers retain their full allocations.");
        }
    }

    void FDebugUI::DrawRenderStateProfilingContents()
    {
        const auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
        const bool bLitView =
            FrameRenderer->GetRenderViewMode(GetActiveViewportIndex()) == Rendering::ERenderViewMode::Lit;
        const bool bEffectsActive = bLitView && Effects.bEnabled;
        const auto StatusText = [](bool bEnabled, bool bActive)
        {
            if (!bActive)
                return bEnabled ? "ON · inactive" : "OFF · inactive";
            return bEnabled ? "ON" : "OFF";
        };
        const auto StatusColor = [](bool bEnabled, bool bActive)
        {
            if (!bActive)
                return ImVec4(0.62F, 0.64F, 0.68F, 1.0F);
            return bEnabled ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F) : ImVec4(0.70F, 0.72F, 0.75F, 1.0F);
        };
        if (ImGui::BeginTable("RenderStateFlags", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
        {
            ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed, 128.0F);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 112.0F);
            const auto DrawState = [&](const char *Label, bool bEnabled, bool bActive)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(StatusColor(bEnabled, bActive), "%s", StatusText(bEnabled, bActive));
            };
            DrawState("Lit effects", Effects.bEnabled, bLitView);
            DrawState("Heat effect", Effects.bHeatLayer, bEffectsActive);
            DrawState("Mud layer", Effects.bMudDisplacement, bEffectsActive);
            DrawState("Ice layer", Effects.bIceDisplacement, bEffectsActive);
            DrawState("WaterFilm layer", Effects.bWaterFilmDisplacement, bEffectsActive);
            DrawState("Lava layer", Effects.bLavaDisplacement, bEffectsActive);
            const bool bSmoothingActive =
                bEffectsActive && (Effects.bMudDisplacement || Effects.bWaterFilmDisplacement ||
                                   Effects.bLavaDisplacement || Effects.bIceDisplacement);
            DrawState("Height-field smoothing", Effects.bHeightFieldSmoothing, bSmoothingActive);
            DrawState("Coverage smoothing", Effects.bCoverageSmoothing, bEffectsActive);
            ImGui::EndTable();
        }
    }

#pragma endregion
} // namespace MDSS
