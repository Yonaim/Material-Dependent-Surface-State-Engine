/**
 * @file DebugUIProfiling.cpp
 * @brief GPU·CPU 시간과 자원 메모리의 프로파일링 UI를 표시한다.
 */

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
    }

    void FDebugUI::DrawViewportStatsOverlay()
    {
        if (FrameRenderer == nullptr || !bViewportOverlaysVisible)
        {
            return;
        }

        ImGuiDockNode* ViewportNode =
            ViewportState.DockspaceID != 0 ? ImGui::DockBuilderGetCentralNode(ViewportState.DockspaceID) : nullptr;
        if (ViewportNode == nullptr)
        {
            return;
        }

        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;

        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }

        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const glm::vec4      Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + Rect.x * DisplaySize.x,
                                            MainViewport->Pos.y + Rect.y * DisplaySize.y};
        const ImVec2         ViewportSize{Rect.z * DisplaySize.x, Rect.w * DisplaySize.y};

        if (ViewportSize.x < 340.0F || ViewportSize.y < 240.0F)
            return;

        float NextOverlayY = DrawTimeProfilingOverlay(ViewportOrigin);
        NextOverlayY = DrawMemoryProfilingOverlay(ViewportOrigin, NextOverlayY);
        DrawRenderStateProfilingOverlay(ViewportOrigin, NextOverlayY);
    }

#pragma endregion

// ============================================================
// 01. Profiling Overlay Details
// ============================================================
#pragma region 01_Profiling_Overlay_Details
// 시간·GPU 메모리·효과 활성 상태를 서로 독립된 화면으로 표시한다.

    static std::array<float, 40> CollectTimeProfilingMetrics(const Rendering::FRendererProfilingStats& Stats,
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

    float FDebugUI::DrawTimeProfilingOverlay(const ImVec2& ViewportOrigin)
    {
        constexpr float OverlayWindowWidth = 320.0F;
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        float RenderStateY = ViewportOrigin.y + 10.0F;

        ImGui::SetNextWindowPos({ViewportOrigin.x + 10.0F, ViewportOrigin.y + 10.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({OverlayWindowWidth, 0.0F}, {OverlayWindowWidth, 800.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0F, 7.0F});

        if (ImGui::Begin("Profiling##Overlay", nullptr, Flags))
        {
            const ImGuiIO&                            IO = ImGui::GetIO();
            const double                              FrameSeconds = static_cast<double>(IO.DeltaTime);
            const Rendering::FRendererProfilingStats& Stats = FrameRenderer->GetProfilingStats();
            const bool  bFineRenderTimings = FrameRenderer->AreRenderPassSubstageTimingsReliable();
            const std::array<float, 40> MetricValues = CollectTimeProfilingMetrics(Stats, bFineRenderTimings);

            // GPU timestamps arrive after the frame fence. Do not mix samples from the previous draw mode.
            if (std::isfinite(FrameSeconds) && FrameSeconds > 0.0 &&
                Stats.OverlayDisplayMode == FrameRenderer->GetOverlayDisplayMode())
            {
                Profiling.Accumulate(FrameSeconds, MetricValues);
            }

            Profiling.PublishWindow();

            DrawTimeProfilingContents(Stats, bFineRenderTimings);

            RenderStateY = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + 6.0F;
        }

        const ImVec2 ProfilingPos = ImGui::GetWindowPos();
        const ImVec2 ProfilingSize = ImGui::GetWindowSize();
        const ImVec2 Mouse = ImGui::GetIO().MousePos;
        ViewportState.bMouseOverViewportOverlay |= Mouse.x >= ProfilingPos.x && Mouse.y >= ProfilingPos.y &&
                                                   Mouse.x < ProfilingPos.x + ProfilingSize.x &&
                                                   Mouse.y < ProfilingPos.y + ProfilingSize.y;
        ImGui::End();
        ImGui::PopStyleVar(3);

        return RenderStateY;
    }

    void FDebugUI::DrawTimeProfilingContents(
        const Rendering::FRendererProfilingStats& Stats, bool bFineRenderTimings)
    {
            if (ImGui::CollapsingHeader("Time Profiling", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const float  FPS = Profiling.Averages[0];
                const ImVec4 FPSColor = FPS >= 55.0F   ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F)
                                        : FPS >= 30.0F ? ImVec4(1.0F, 0.84F, 0.47F, 1.0F)
                                                       : ImVec4(1.0F, 0.57F, 0.57F, 1.0F);
                if (Profiling.bAverageAvailable)
                    ImGui::TextColored(FPSColor, "FPS: %.1f", FPS);
                else
                    ImGui::TextDisabled("FPS: collecting 1s average");
                const auto SetupMetricTable = [](const char* Id)
                {
                    constexpr int ColumnCount = 4;

                    if (!ImGui::BeginTable(
                            Id, ColumnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
                        return false;
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 134.0F);
                    ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 44.0F);
                    ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 44.0F);
                    ImGui::TableSetupColumn("100ms", ImGuiTableColumnFlags_WidthFixed, 50.0F);

                    return true;
                };
                const auto DrawMetric =
                    [this](std::size_t Index, const char* Label, ImVec4 Color, const char* Tooltip = nullptr)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", Label);

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
                    DrawValue(Profiling.Recent100msAverages[Index + 2], true);
                };

                if (SetupMetricTable("ViewportProfilingSummary"))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Metric");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("Avg ms");
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextDisabled("Max ms");
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextDisabled("100ms");
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
                    DrawFrameValue(Profiling.Recent100msAverages[1], true);
                    DrawMetric(0, "Render GPU", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(1, "Solver GPU / frame", {0.82F, 0.87F, 0.94F, 1.0F});
                    ImGui::EndTable();
                }

                ImGui::Separator();

                ImGui::PushStyleColor(ImGuiCol_Header, {0.0F, 0.0F, 0.0F, 0.0F});
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.30F, 0.32F, 0.35F, 0.35F});
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0.0F, 0.0F, 0.0F, 0.0F});

                if (BeginTextTreeNode("GPU: Simulation"))
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
                        const auto AddStateRow = [](const char* Label, const char* Format, auto... Values)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextDisabled("%s", Label);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextDisabled(Format, Values...);
                        };
                        AddStateRow("Sim Steps", "%u", Stats.SimulationSteps);
                        AddStateRow("Maximum step", "%.3f ms", 1000.0F * FrameRenderer->GetMaximumSimulationStep());
                        AddStateRow("Simulated this frame", "%.2f ms", Stats.SimulatedThisFrameSeconds * 1000.0F);
                        AddStateRow("Simulation time", "%.2f s", FrameRenderer->GetSimulatedSeconds());
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

                if (BeginTextTreeNode("GPU: Rendering"))
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
                            ImGui::SetTooltip(
                                "정점 하나 이상이 2%% 커버리지 기준을 넘으면 활성 삼각형으로 계산합니다. "
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
                            DrawMetric(21, "Between draws", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(22, "Grid / gizmo", {0.82F, 0.87F, 0.94F, 1.0F});
                            ImGui::EndTable();
                        }

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }

                if (BeginTextTreeNode("CPU"))
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



    float FDebugUI::DrawMemoryProfilingOverlay(const ImVec2& ViewportOrigin, float RenderStateY)
    {
        constexpr float OverlayWindowWidth = 320.0F;
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;

        ImGui::SetNextWindowPos({ViewportOrigin.x + 10.0F, RenderStateY}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({OverlayWindowWidth, 0.0F}, {OverlayWindowWidth, 800.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0F, 7.0F});

        if (ImGui::Begin("Memory Profiling##Overlay", nullptr, Flags) &&
            ImGui::CollapsingHeader("Memory Profiling", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const GPU::FGPUResourceMemorySnapshot Memory = GPU::FGPUResourceMemoryStats::GetSnapshot();
            const auto       PrecisionMemory = EstimatePerformanceMemory(FrameRenderer->GetSurfaceGPUResources());
            constexpr double BytesPerMiB = 1024.0 * 1024.0;
            const ImVec4     TotalColor{1.0F, 0.78F, 0.24F, 1.0F};
            const ImVec4     GroupColor{0.60F, 0.80F, 0.98F, 1.0F};
            const auto       DrawMemoryRow = [&](const char* Label, std::uint64_t CurrentBytes)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f MiB", static_cast<double>(CurrentBytes) / BytesPerMiB);
            };
            const auto DrawMemoryTotalRow = [&](const char* Label, std::uint64_t CurrentBytes, ImVec4 Color)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(Color, "%s", Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(Color, "%.2f MiB", static_cast<double>(CurrentBytes) / BytesPerMiB);
            };
            const auto DrawMemoryTreeRow = [&](const char* Label, std::uint64_t CurrentBytes)
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

            if (ImGui::BeginTable(
                    "ViewportMemoryProfiling", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
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

                if (DrawMemoryTreeRow("MDSS Surface System", SurfaceSystemBytes))
                {
                    if (DrawMemoryTreeRow("Core data", SurfaceCoreBytes))
                    {
                        DrawMemoryRow("Surface State", CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceState));
                        DrawMemoryRow("Surface Geometry",
                                      CategoryBytes(GPU::EGPUBufferMemoryCategory::SurfaceGeometry));
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
            ImGui::TextDisabled(
                "Performance options use preallocated buffers; toggles do not change current allocations.");
            ImGui::TextDisabled("FP16 compact allocation estimate");
            ImGui::Text("Raw Flux savings: %.2f MiB", PrecisionMemory.RawFluxSavingsBytes / BytesPerMiB);
            ImGui::Text("TransferWeight savings: %.2f MiB", PrecisionMemory.DynamicWeightSavingsBytes / BytesPerMiB);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Estimate from the loaded instances' texel and channel counts. These bytes are not "
                                  "subtracted from Current because the A/B buffers retain their full allocations.");
        }

        const ImVec2 MemoryProfilePos = ImGui::GetWindowPos();
        const ImVec2 MemoryProfileSize = ImGui::GetWindowSize();
        const ImVec2 MemoryMouse = ImGui::GetIO().MousePos;
        ViewportState.bMouseOverViewportOverlay |= MemoryMouse.x >= MemoryProfilePos.x &&
                                                   MemoryMouse.y >= MemoryProfilePos.y &&
                                                   MemoryMouse.x < MemoryProfilePos.x + MemoryProfileSize.x &&
                                                   MemoryMouse.y < MemoryProfilePos.y + MemoryProfileSize.y;
        RenderStateY = MemoryProfilePos.y + MemoryProfileSize.y + 6.0F;
        ImGui::End();
        ImGui::PopStyleVar(3);

        return RenderStateY;
    }

    void FDebugUI::DrawRenderStateProfilingOverlay(const ImVec2& ViewportOrigin, float RenderStateY)
    {
        constexpr float OverlayWindowWidth = 320.0F;
        const ImVec2 Mouse = ImGui::GetIO().MousePos;

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
        ImGui::SetNextWindowPos({ViewportOrigin.x + 10.0F, RenderStateY}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({OverlayWindowWidth, 0.0F}, {OverlayWindowWidth, 800.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0F, 7.0F});
        constexpr ImGuiWindowFlags StateFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("Render State##Overlay", nullptr, StateFlags) &&
            ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable(
                    "RenderStateFlags", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed, 128.0F);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 112.0F);
                const auto DrawState = [&](const char* Label, bool bEnabled, bool bActive)
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

        const ImVec2 StatePos = ImGui::GetWindowPos();
        const ImVec2 StateSize = ImGui::GetWindowSize();
        ViewportState.bMouseOverViewportOverlay |= Mouse.x >= StatePos.x && Mouse.y >= StatePos.y &&
                                                   Mouse.x < StatePos.x + StateSize.x &&
                                                   Mouse.y < StatePos.y + StateSize.y;
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

#pragma endregion
} // namespace MDSS
