/**
 * @file SurfaceSimulationSnapshot.cpp
 * @brief 초기 층 상태에서 그래픽 표시 배열을 구성한다.
 */

#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include "SurfaceState/Simulation/SurfaceFilmState.h"
#include <algorithm>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Film Snapshot
    // ============================================================
    #pragma region 00_Film_Snapshot
        // 실제 퇴적 표면과 대조군에서 액체가 읽는 지지 표면을 따로 기록한다.

        std::vector<std::array<float, 6>>
        MakeFilmDisplay(const FSurfaceSimulationDomain &Geometry, const FSurfaceSimulationState &State, FSurfaceTilt Pose)
        {
            if (Geometry.FilmLayers.empty())
                return {};
            const auto                        Count = Geometry.Regions.size();
            std::vector<std::array<float, 6>> Result(Count);
            auto                              Support = Geometry;
            for (std::size_t Layer = 0; Layer + 1 < Geometry.FilmLayers.size(); ++Layer)
                FilmState::AddFilmSupport(Support, Layer, State.Film);
            FilmState::SelectTopSupport(Geometry, Support, Pose, State.FilmTime, State.Film);
            for (std::size_t Region = 0; Region < Count; ++Region)
            {
                auto &Value = Result[Region];
                Value[3] = -1.e10F;
                for (std::size_t Layer = 0; Layer + 1 < Geometry.FilmLayers.size(); ++Layer)
                {
                    const auto &Material = Geometry.FilmLayers[Layer];
                    const auto &Film = State.Film[Layer * Count + Region];
                    if (Film.Mass > 0.F)
                    {
                        std::ranges::copy(Material.Color, Value.begin());
                        Value[3] = std::max(Value[3], Film.Level);
                        Value[4] = FilmState::FilmWetness(Material, Film);
                    }
                }
                Value[5] = Support.RegionBedLevels.empty() ? -1.e10F : Support.RegionBedLevels[Region];
            }
            return Result;
        }
    #pragma endregion

} // namespace MDSS::Liquid
