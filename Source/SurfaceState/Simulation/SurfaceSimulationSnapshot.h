/**
 * @file SurfaceSimulationSnapshot.h
 * @brief GPU에서 화면으로 전달할 표면 상태 스냅샷의 배치를 정의한다.
 */

#pragma once

#include "SurfaceState/Simulation/SurfaceSimulationState.h"
#include <cstddef>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Snapshot Layout
    // ============================================================
    #pragma region 00_Snapshot_Layout
        // 표시 버퍼의 상태 배열과 월류 기록의 바이트 배치를 정의한다.

        /** @brief 하나의 표시 구간에서 접촉 표본을 통과한 실제 질량과 최대 월류 수두. */
        struct FOverflowTransferRecord
        {
            float         MassKg = 0.F;
            float         HeadM = 0.F;
            std::uint32_t Contact = 0;
            std::uint32_t Receiver = 0;
            std::uint32_t DisplayInterval = 0;
            float         LastTransferSeconds = 0.F;
        };
        static_assert(sizeof(FOverflowTransferRecord) == 24 &&
                      offsetof(FOverflowTransferRecord, LastTransferSeconds) == 20);
        struct FSurfaceSnapshotLayout
        {
            explicit FSurfaceSnapshotLayout(std::size_t Cells,
                                            std::size_t Regions,
                                            std::size_t Contacts,
                                            std::size_t Layers = 0)
                : RegionOffset(Cells * sizeof(FSurfaceCellState)),
                  OverflowOffset(RegionOffset + Regions * sizeof(FPrimaryLiquidRegionState)),
                  FilmOffset(OverflowOffset + 2 * Contacts * sizeof(FOverflowTransferRecord)),
                  FilmStateOffset(FilmOffset + (Layers > 0 ? Regions * sizeof(std::array<float, 6>) : 0)),
                  ContactOffset(FilmStateOffset + Layers * Regions * sizeof(FFilmRegionState)),
                  Size(ContactOffset + Regions * sizeof(std::array<float, 4>))
            {
            }

            std::size_t RegionOffset;
            std::size_t OverflowOffset;
            std::size_t FilmOffset;
            std::size_t FilmStateOffset;
            std::size_t ContactOffset;
            std::size_t Size;
        };

        [[nodiscard]] std::vector<std::array<float, 6>> MakeFilmDisplay(const FSurfaceSimulationDomain &Geometry,
                                                                        const FSurfaceSimulationState  &State,
                                                                        FSurfaceTilt                    Pose = {});
    #pragma endregion

} // namespace MDSS::Liquid
