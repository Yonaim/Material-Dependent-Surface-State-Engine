#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
    void FDebugUI::DrawPerformanceTab(bool bRendering)
    {
        const auto       PrecisionMemory = EstimatePerformanceMemory(FrameRenderer->GetSurfaceGPUResources());
        constexpr double BytesPerMiB = 1024.0 * 1024.0;
        const auto       DrawReservedEstimate = [&](double Bytes)
        {
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::TextDisabled("(reserved: %s)", FormatMemoryBytes(Bytes).c_str());

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("이 영역은 공유 GPU 버퍼에 미리 할당되어 Current 총량에 이미 포함됩니다. 옵션을 "
                                      "꺼도 버퍼를 해제하지 않습니다.");
        };
        const auto DrawSparseSubheading = [&](const char* Label, float TopGap)
        {
            ImGui::Dummy({0.0F, TopGap});
            ImGui::PushStyleColor(ImGuiCol_Text, {0.78F, 0.86F, 0.96F, 1.0F});

            if (ViewportState.SectionHeaderFont != nullptr)
                ImGui::PushFont(ViewportState.SectionHeaderFont);
            ImGui::TextUnformatted(Label);

            if (ViewportState.SectionHeaderFont != nullptr)
                ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::Dummy({0.0F, 5.0F});
        };
        auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
        bool bEffectsChanged = false;

        if (bRendering)
        {
            const auto DrawTexelMeshResolutionControl =
                [&](const char* Label, std::uint32_t ActiveResolution, auto&& SetResolution)
            {
                const std::string Preview = std::to_string(ActiveResolution);
                const bool        bOpen = BeginLabeledCombo(Label, Preview.c_str());

                if (ImGui::IsItemHovered())
                    SetDescriptionTooltip(
                        std::string_view(Label) == "Surface mesh"
                            ? "Overlay가 켜져 있을 때 표면을 그리는 Texel Mesh 해상도입니다."
                            : "State overlay를 그리는 Texel Mesh 해상도입니다. 바꾸면 시뮬레이션 State는 유지됩니다.");
                if (bOpen)
                {
                    for (const auto& Preset : SurfaceState::SurfaceRenderMeshResolutionPresets)
                    {
                        const bool bSelected = Preset.Resolution == ActiveResolution;

                        if (ImGui::Selectable(Preset.Label, bSelected) && !bSelected)
                        {
                            SetResolution(Preset.Resolution);
                            Simulation.bFrameTimeResetRequested = true;
                            ResetProfilingAverages();
                        }

                        if (bSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    EndLabeledCombo();
                }
            };
            DrawTexelMeshResolutionControl("Surface mesh",
                                           FrameRenderer->GetSurfaceTexelMeshResolution(),
                                           [&](std::uint32_t Resolution)
                                           { FrameRenderer->SetSurfaceTexelMeshResolution(Resolution); });
            DrawTexelMeshResolutionControl("Overlay mesh",
                                           FrameRenderer->GetOverlayTexelMeshResolution(),
                                           [&](std::uint32_t Resolution)
                                           { FrameRenderer->SetOverlayTexelMeshResolution(Resolution); });

            bool bOverlayTileCullingEnabled = FrameRenderer->IsOverlayTileCullingEnabled();

            if (ImGui::Checkbox("Overlay tile culling", &bOverlayTileCullingEnabled))
            {
                FrameRenderer->SetOverlayTileCullingEnabled(bOverlayTileCullingEnabled);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("Coverage를 변경된 render tile 목록으로 indirect dispatch합니다. 끄면 매 프레임 "
                                      "전체 정점을 계산합니다.");

            const auto  OccupancyTileSize = FrameRenderer->GetOverlayOccupancyTileSize();
            const char* OccupancyTileLabel = OccupancyTileSize == Rendering::EOverlayOccupancyTileSize::Tile8 ? "8 x 8"
                                             : OccupancyTileSize == Rendering::EOverlayOccupancyTileSize::Tile16
                                                 ? "16 x 16"
                                                 : "32 x 32";
            ImGui::BeginDisabled(!bOverlayTileCullingEnabled);
            const bool bOccupancyTileOpen = BeginLabeledCombo("Render tile size", OccupancyTileLabel);

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "Occupancy, Height, Normal, smoothing, Coverage가 함께 사용하는 render tile 크기입니다.");
            if (bOccupancyTileOpen)
            {
                const std::array<std::pair<const char*, Rendering::EOverlayOccupancyTileSize>, 3> TileOptions{{
                    {"8 x 8", Rendering::EOverlayOccupancyTileSize::Tile8},
                    {"16 x 16", Rendering::EOverlayOccupancyTileSize::Tile16},
                    {"32 x 32", Rendering::EOverlayOccupancyTileSize::Tile32},
                }};

                for (const auto& [Label, Size] : TileOptions)
                {
                    const bool bSelected = OccupancyTileSize == Size;

                    if (ImGui::Selectable(Label, bSelected) && !bSelected)
                    {
                        FrameRenderer->SetOverlayOccupancyTileSize(Size);
                        Simulation.bFrameTimeResetRequested = true;
                        ResetProfilingAverages();
                    }

                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }

                EndLabeledCombo();
            }

            ImGui::EndDisabled();
            ImGui::Dummy({0.0F, 6.0F});
            bEffectsChanged |= ImGui::Checkbox("Height smoothing", &Effects.bHeightFieldSmoothing);

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("Mud/Water/Lava 적층 높이와 normal을 렌더링 전용 3x3 필터로 부드럽게 합니다.");
            bEffectsChanged |= ImGui::Checkbox("Coverage smoothing", &Effects.bCoverageSmoothing);

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("State coverage의 시각적 경계를 5x5 Gaussian으로 부드럽게 합니다. Simulation "
                                      "State는 바꾸지 않습니다.");
        }

        bool bRawFluxCache = FrameRenderer->IsRawFluxCacheEnabled();

        if (!bRendering)
        {
            DrawSectionHeader("Cache & Precision");

            if (ImGui::Checkbox("Raw Flux Cache", &bRawFluxCache))
            {
                FrameRenderer->SetRawFluxCacheEnabled(bRawFluxCache);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("Pass 1의 8방향 raw flux를 저장해 Pass 2 재계산을 줄입니다. 캐시 영역은 공유 "
                                      "버퍼에 미리 예약되어 있습니다.");
            DrawReservedEstimate(PrecisionMemory.RawFluxCacheBytes);
        }

        if (!bRendering)
        {
            bool bCoalescedRawFluxLayout = FrameRenderer->IsCoalescedRawFluxLayoutEnabled();
            ImGui::BeginDisabled(!bRawFluxCache);

            if (ImGui::Checkbox("Coalesced Raw Flux Layout", &bCoalescedRawFluxLayout))
            {
                FrameRenderer->SetCoalescedRawFluxLayoutEnabled(bCoalescedRawFluxLayout);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "flux 저장 순서를 바꿔 Pass 1의 연속 write를 비교합니다. 메모리 크기는 바뀌지 않습니다.");

            bool bHalfRawFlux = FrameRenderer->IsHalfRawFluxCacheEnabled();

            if (ImGui::Checkbox("FP16 Raw Flux", &bHalfRawFlux))
            {
                FrameRenderer->SetHalfRawFluxCacheEnabled(bHalfRawFlux);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("8방향 flux를 FP16으로 pack해 읽기·쓰기 대역폭을 줄입니다. 기본 ON이며 현재 "
                                      "버퍼 할당량은 유지됩니다.");
            ImGui::EndDisabled();
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::TextDisabled("(예상 절감: %.2f MiB)", PrecisionMemory.RawFluxSavingsBytes / BytesPerMiB);

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("컴팩트 재할당 시 예상 절감량입니다. 현재 GPU 버퍼 할당량은 줄지 않습니다.");

            bool bHalfDynamicWeights = FrameRenderer->IsHalfDynamicWeightsEnabled();

            if (ImGui::Checkbox("FP16 Weights", &bHalfDynamicWeights))
            {
                FrameRenderer->SetHalfDynamicWeightsEnabled(bHalfDynamicWeights);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("8방향 TransferWeight를 FP16으로 pack해 Solver 읽기 대역폭을 비교합니다.");
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::TextDisabled("(예상 절감: %.2f MiB)", PrecisionMemory.DynamicWeightSavingsBytes / BytesPerMiB);

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("컴팩트 재할당 시 예상 절감량입니다. 현재 GPU 버퍼 할당량은 줄지 않습니다.");

            DrawSectionHeader("Sparse Execution");

            DrawSparseSubheading("Active", 8.0F);
            bool bSparseSolver = FrameRenderer->IsSparseSolverEnabled();

            if (ImGui::Checkbox("Sparse Solver Workgroup Scheduling", &bSparseSolver))
            {
                FrameRenderer->SetSparseSolverEnabled(bSparseSolver);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "다음 Solver 계산이 필요한 active workgroup만 목록으로 만들어 간접 dispatch합니다.");
            DrawReservedEstimate(PrecisionMemory.SparseSolverWorkgroupBytes);

            bool bActiveChannelMask = FrameRenderer->IsActiveChannelMaskEnabled();

            if (ImGui::Checkbox("Active Channel Mask", &bActiveChannelMask))
            {
                FrameRenderer->SetActiveChannelMaskEnabled(bActiveChannelMask);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "다음 Solver 단계에서 활성 상태 channel만 처리합니다. 개별 채널의 active 여부를 표시합니다.");
            DrawReservedEstimate(PrecisionMemory.ActiveChannelMaskBytes);

            bool bPerWorkgroupChannelMask = FrameRenderer->IsPerWorkgroupChannelMaskEnabled();
            ImGui::BeginDisabled(!bActiveChannelMask);

            if (ImGui::Checkbox("Per-WG Active Channel Mask", &bPerWorkgroupChannelMask))
            {
                FrameRenderer->SetPerWorkgroupChannelMaskEnabled(bPerWorkgroupChannelMask);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("각 64-texel workgroup 안에서 처리할 active channel을 더 좁혀 선택합니다.");
            ImGui::EndDisabled();
            DrawReservedEstimate(PrecisionMemory.PerWorkgroupChannelMaskBytes);

            DrawSparseSubheading("Dirty", 10.0F);
            bool bSparseAccumulationHeight = FrameRenderer->IsSparseAccumulationHeightEnabled();

            if (ImGui::Checkbox("Sparse Accumulation Height", &bSparseAccumulationHeight))
            {
                FrameRenderer->SetSparseAccumulationHeightEnabled(bSparseAccumulationHeight);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("누적 높이가 달라진 dirty texel group만 Height buffer에서 다시 계산합니다.");
            DrawReservedEstimate(PrecisionMemory.SparseAccumulationHeightBytes);

            bool bSparseSimulation = FrameRenderer->IsSparseSimulationGeometryEnabled();

            if (ImGui::Checkbox("Sparse Simulation Geometry", &bSparseSimulation))
            {
                FrameRenderer->SetSparseSimulationGeometryEnabled(bSparseSimulation);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "현재 적층 높이와 built geometry가 달라진 dirty group 및 2-hop topology 이웃만 다시 계산합니다.");
            DrawReservedEstimate(PrecisionMemory.SparseSimulationGeometryBytes);
        }

        if (bRendering)
        {
            DrawSectionHeader("Advanced");
            bool bSparseHeight = FrameRenderer->IsSparseHeightSmoothingEnabled();
            ImGui::BeginDisabled(!Effects.bHeightFieldSmoothing);

            if (ImGui::Checkbox("Sparse Height Smoothing", &bSparseHeight))
            {
                FrameRenderer->SetSparseHeightSmoothingEnabled(bSparseHeight);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("변경된 Normal tile만 처리합니다.");
            bool bSharedHeight = FrameRenderer->IsSharedHeightSmoothingEnabled();
            ImGui::BeginDisabled(!bSparseHeight || !FrameRenderer->IsOverlayTileCullingEnabled());

            if (ImGui::Checkbox("Shared Height Smoothing Halo", &bSharedHeight))
            {
                FrameRenderer->SetSharedHeightSmoothingEnabled(bSharedHeight);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                SetDescriptionTooltip("sparse 8x8 workgroup의 3x3 입력을 shared memory halo에 모아 재사용합니다. "
                                      "Sparse Height Smoothing과 Overlay tile culling이 필요합니다.");
            ImGui::EndDisabled();
            ImGui::EndDisabled();

            bool bSharedOverlay = FrameRenderer->IsSharedOverlayCoverageEnabled();
            ImGui::BeginDisabled(!Effects.bCoverageSmoothing || !FrameRenderer->IsOverlayTileCullingEnabled());

            if (ImGui::Checkbox("Shared Overlay Coverage Halo", &bSharedOverlay))
            {
                FrameRenderer->SetSharedOverlayCoverageEnabled(bSharedOverlay);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                SetDescriptionTooltip(
                    "sparse overlay tile의 5x5 coverage 입력을 shared memory에 모읍니다. Coverage Smoothing과 Overlay "
                    "tile culling이 필요합니다. 전체 갱신은 기존 경로를 사용합니다.");
            ImGui::EndDisabled();

            bool bPrecompute = FrameRenderer->IsPrecomputeCoverageSmoothingEnabled();
            ImGui::BeginDisabled(!Effects.bCoverageSmoothing);

            if (ImGui::Checkbox("Precompute Coverage Smoothing", &bPrecompute))
            {
                FrameRenderer->SetPrecomputeCoverageSmoothingEnabled(bPrecompute);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("State texture를 compute에서 미리 smoothing해 fragment 샘플을 줄입니다. "
                                      "켜면 Render State Texture Sampling의 Texture/SSBO 비교는 비활성화됩니다.");

            bool bSeparable = FrameRenderer->IsSeparableCoverageSmoothingEnabled();
            ImGui::BeginDisabled(!bPrecompute);

            if (ImGui::Checkbox("Separable Coverage Smoothing", &bSeparable))
            {
                FrameRenderer->SetSeparableCoverageSmoothingEnabled(bSeparable);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered())
                SetDescriptionTooltip(
                    "5x5 필터를 가로·세로 두 pass로 처리합니다. 경계에서 결과가 조금 달라질 수 있습니다.");
            bool bSharedState = FrameRenderer->IsSharedStateTextureSmoothingEnabled();
            ImGui::BeginDisabled(!bPrecompute || !bSeparable);

            if (ImGui::Checkbox("Shared Render State Halo", &bSharedState))
            {
                FrameRenderer->SetSharedStateTextureSmoothingEnabled(bSharedState);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                SetDescriptionTooltip(
                    "가로·세로 separable pass의 입력을 8x8 workgroup과 shared memory halo로 재사용합니다.");
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::EndDisabled();

            const bool bTextureForcedByPrecompute = Effects.bCoverageSmoothing && bPrecompute;
            bool       bRenderStateTexture =
                FrameRenderer->IsRenderStateTextureSamplingEnabled() || bTextureForcedByPrecompute;
            ImGui::BeginDisabled(bTextureForcedByPrecompute);

            if (ImGui::Checkbox("Render State Texture Sampling", &bRenderStateTexture))
            {
                FrameRenderer->SetRenderStateTextureSamplingEnabled(bRenderStateTexture);
                Simulation.bFrameTimeResetRequested = true;
                ResetProfilingAverages();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                SetDescriptionTooltip(
                    bTextureForcedByPrecompute
                        ? "Precompute Coverage Smoothing이 켜져 있으면 미리 계산한 State texture를 사용하므로 "
                          "Texture/SSBO A/B 비교를 할 수 없습니다. 비교하려면 Precompute를 끄세요."
                        : "fragment의 RenderStateTexture 샘플링과 Simulation SSBO bilinear sampling을 비교합니다.");
            ImGui::EndDisabled();
        }

        if (bEffectsChanged)
        {
            FrameRenderer->SetDemoSurfaceEffectSettings(Effects);
            Simulation.bFrameTimeResetRequested = true;
            ResetProfilingAverages();
        }
    }

} // namespace MDSS
