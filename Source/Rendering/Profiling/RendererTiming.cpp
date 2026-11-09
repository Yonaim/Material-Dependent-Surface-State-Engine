/**
 * @file RendererTiming.cpp
 * @brief 완료된 frame의 GPU query를 회수해 계측과 benchmark 결과를 갱신한다.
 */
#include "Rendering/Core/RendererShared.h"
namespace MDSS::Rendering
{
// ============================================================
// 00. Completed Frame Timing Readback
// ============================================================
#pragma region 00_Completed_Frame_Timing_Readback

    void FRenderer::CollectCompletedFrameTimings(std::uint32_t FrameIndex)
    {
        if (TimestampQueryPool != VK_NULL_HANDLE && bTimestampQueriesSubmitted[FrameIndex])
        {
            const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
            const std::uint32_t OverlayQueryOffset =
                FixedTimestampQueryCount + SolverTimestampStepsSubmitted[FrameIndex] * SolverTimestampGroupCount * 8U;
            const std::uint32_t QueryCount =
                OverlayQueryOffset + OverlayTimestampLayersSubmitted[FrameIndex] * OverlayTimestampQueriesPerLayer;
            std::vector<std::uint64_t> Timestamps(QueryCount, 0U);
            const VkResult             QueryResult = vkGetQueryPoolResults(Context.GetDevice(),
                                                               TimestampQueryPool,
                                                               QueryBase,
                                                               QueryCount,
                                                               Timestamps.size() * sizeof(std::uint64_t),
                                                               Timestamps.data(),
                                                               sizeof(std::uint64_t),
                                                               VK_QUERY_RESULT_64_BIT);
            if (QueryResult == VK_SUCCESS && TimestampPeriodNanoseconds > 0.0F)
            {
                ProfilingStats.OverlayDisplayMode = OverlayDisplayModesSubmitted[FrameIndex];
                const auto ToMilliseconds = [this](std::uint64_t Start, std::uint64_t End)
                {
                    std::uint64_t ElapsedTicks = End - Start;
                    if (TimestampValidBits < 64U)
                    {
                        ElapsedTicks &= (std::uint64_t{1} << TimestampValidBits) - 1U;
                    }
                    return static_cast<float>(static_cast<double>(ElapsedTicks) * TimestampPeriodNanoseconds / 1.0e6);
                };
                ProfilingStats.RenderPreparationGpuMilliseconds = ToMilliseconds(Timestamps[0], Timestamps[1]);
                ProfilingStats.SceneDrawGpuMilliseconds =
                    ToMilliseconds(Timestamps[2], Timestamps[bRenderPassSubstageTimingsReliable ? 3U : 20U]);
                ProfilingStats.TexelInspectorGpuMilliseconds = ToMilliseconds(Timestamps[4], Timestamps[5]);
                ProfilingStats.BaseMeshDrawGpuMilliseconds = ToMilliseconds(Timestamps[6], Timestamps[7]);
                ProfilingStats.MudOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[8], Timestamps[9]);
                ProfilingStats.ReservedOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[10], Timestamps[11]);
                ProfilingStats.OverlayPreparationGpuMilliseconds = ToMilliseconds(Timestamps[12], Timestamps[13]);
                ProfilingStats.SceneSetupGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[6]);
                ProfilingStats.RenderPassBeginGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[14]);
                ProfilingStats.RenderPassColorStageGpuMilliseconds = ToMilliseconds(Timestamps[15], Timestamps[16]);
                ProfilingStats.RenderPassDepthStageGpuMilliseconds = ToMilliseconds(Timestamps[17], Timestamps[18]);
                ProfilingStats.ViewportSetupGpuMilliseconds = ToMilliseconds(Timestamps[14], Timestamps[6]);
                ProfilingStats.UIDrawGpuMilliseconds = ToMilliseconds(Timestamps[3], Timestamps[19]);
                ProfilingStats.RenderPassEndGpuMilliseconds = ToMilliseconds(Timestamps[19], Timestamps[20]);
                ProfilingStats.SceneBetweenDrawsGpuMilliseconds =
                    ToMilliseconds(Timestamps[7], Timestamps[8]) + ToMilliseconds(Timestamps[9], Timestamps[10]);
                ProfilingStats.SceneTailGpuMilliseconds = ToMilliseconds(Timestamps[11], Timestamps[3]);
                ProfilingStats.OverlayGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayHeightGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayNormalVertexGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySmoothingGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesDispatchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayBoundarySearchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayTopCommandGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayCoverageSampleGpuMilliseconds = 0.0F;
                for (std::uint32_t Layer = 0; Layer < OverlayTimestampLayersSubmitted[FrameIndex]; ++Layer)
                {
                    const std::uint32_t LayerQuery = OverlayQueryOffset + Layer * OverlayTimestampQueriesPerLayer;
                    ProfilingStats.OverlayGeometryGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlayHeightGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 12U]);
                    ProfilingStats.OverlayNormalVertexGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 12U], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlaySmoothingGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 2U], Timestamps[LayerQuery + 3U]);
                    ProfilingStats.OverlaySidesGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 11U]);
                    ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 5U]);
                    ProfilingStats.OverlayCoverageSampleGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]);
                    ProfilingStats.OverlayBoundarySearchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]);
                    ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 6U], Timestamps[LayerQuery + 7U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 8U], Timestamps[LayerQuery + 9U]);
                    ProfilingStats.OverlayTopCommandGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesDispatchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 10U], Timestamps[LayerQuery + 11U]);
                }
                LastRenderGpuMilliseconds =
                    ProfilingStats.RenderPreparationGpuMilliseconds + ProfilingStats.SceneDrawGpuMilliseconds;
                if (bRenderPassSubstageTimingsReliable)
                    LastRenderGpuMilliseconds +=
                        ProfilingStats.UIDrawGpuMilliseconds + ProfilingStats.RenderPassEndGpuMilliseconds;
                LastSolverPass1GpuMilliseconds = 0.0F;
                LastSolverPass2GpuMilliseconds = 0.0F;
                ProfilingStats.AccumulationGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.TransferWeightGpuMilliseconds = 0.0F;
                if (SolverTimestampStepsSubmitted[FrameIndex] > 0U)
                {
                    for (std::uint32_t Slot = 0;
                         Slot < SolverTimestampGroupCount * SolverTimestampStepsSubmitted[FrameIndex];
                         ++Slot)
                    {
                        const std::uint32_t SlotQuery = FixedTimestampQueryCount + Slot * 8U;
                        ProfilingStats.AccumulationGeometryGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery], Timestamps[SlotQuery + 1U]);
                        ProfilingStats.TransferWeightGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 2U], Timestamps[SlotQuery + 3U]);
                        LastSolverPass1GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 4U], Timestamps[SlotQuery + 5U]);
                        LastSolverPass2GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 6U], Timestamps[SlotQuery + 7U]);
                    }
                }
                ProfilingStats.SolverPass1GpuMilliseconds = LastSolverPass1GpuMilliseconds;
                ProfilingStats.SolverPass2GpuMilliseconds = LastSolverPass2GpuMilliseconds;
                ProfilingStats.SolverGpuMilliseconds = LastSolverPass1GpuMilliseconds + LastSolverPass2GpuMilliseconds +
                                                       ProfilingStats.AccumulationGeometryGpuMilliseconds +
                                                       ProfilingStats.TransferWeightGpuMilliseconds;
                LastSolverGpuMilliseconds = ProfilingStats.SolverGpuMilliseconds;
                Benchmark.BenchmarkGpuElapsed = ToMilliseconds(Timestamps[21], Timestamps[23]);
                Benchmark.BenchmarkSimulationElapsed = ToMilliseconds(Timestamps[21], Timestamps[22]);
                WriteBenchmarkSample(FrameIndex);
            }
            bTimestampQueriesSubmitted[FrameIndex] = false;
            SolverTimestampStepsSubmitted[FrameIndex] = 0;
            OverlayTimestampLayersSubmitted[FrameIndex] = 0;
        }
    }
#pragma endregion
}
