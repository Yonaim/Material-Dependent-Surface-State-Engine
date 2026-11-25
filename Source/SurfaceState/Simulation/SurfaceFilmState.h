/**
 * @file SurfaceFilmState.h
 * @brief CPU 측 층 상태의 기하 및 젖음 계산을 공유한다.
 */

#pragma once

#include "SurfaceState/Simulation/SurfaceSimulationState.h"

namespace MDSS::Liquid::FilmState
{
    // ============================================================
    // 00. Shared Film State
    // ============================================================
    #pragma region 00_Shared_Film_State
        // 초기화와 기준 스텝, 표시 데이터 생성이 같은 층 상태를 해석한다.

        /** @brief 저장 영역을 차트에 투영한 면적을 계산한다. */
        [[nodiscard]] double FilmArea(const FSurfaceSimulationDomain &Geometry, int Region);

        /** @brief 물질이 있는 아래층의 수위를 위층 바닥의 하한으로 합친다. */
        void AddFilmSupport(FSurfaceSimulationDomain &Support, std::size_t Layer, std::span<const FFilmRegionState> State);

        /** @brief 현재 질량과 대조군 조건에서 한 영역의 아래층 점유 높이를 복원한다. */
        void RestoreFilmFloor(const FSurfaceSimulationDomain   &Geometry,
                              FSurfaceSimulationDomain         &Support,
                              std::size_t                       Layer,
                              int                               Region,
                              FSurfaceTilt                      Pose,
                              double                            Time,
                              std::span<const FFilmRegionState> State);

        /** @brief 시간과 대조군 설정에 따라 대표 액체 아래 형상을 선택한다. */
        void SelectTopSupport(const FSurfaceSimulationDomain   &Geometry,
                              FSurfaceSimulationDomain         &Support,
                              FSurfaceTilt                      Pose,
                              double                            Time,
                              std::span<const FFilmRegionState> State);

        /** @brief 혼합수가 만드는 박막의 젖음 비율을 구한다. */
        [[nodiscard]] float FilmWetness(const FFilmLayer &Material, const FFilmRegionState &Film);

        /** @brief 층별 수위와 대표 액체 및 셀 상태의 질량·수위를 동기화한다. */
        void RestoreFilm(const FSurfaceSimulationDomain &Geometry, FSurfaceSimulationState &State, FSurfaceTilt Pose);
    #pragma endregion

} // namespace MDSS::Liquid::FilmState
