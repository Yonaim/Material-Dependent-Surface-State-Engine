/**
 * @file SurfaceSimulationState.cpp
 * @brief 초기 셀과 층 상태를 구성하고 층 상태 해석을 공유한다.
 */

#include "SurfaceState/Simulation/SurfaceSimulationState.h"
#include "SurfaceState/Simulation/SurfaceFilmState.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include <algorithm>
#include <limits>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Shared Film State
    // ============================================================
    #pragma region 00_Shared_Film_State
        // 층별 지지 형상과 젖음 비율을 계산한다.

        namespace FilmState
        {
            double FilmArea(const FSurfaceSimulationDomain &Geometry, int Region)
            {
                return static_cast<double>(Geometry.Regions[Region].SampleCount) * Geometry.Shape.PitchX() *
                       Geometry.Shape.PitchY();
            }

            void
            AddFilmSupport(FSurfaceSimulationDomain &Support, std::size_t Layer, std::span<const FFilmRegionState> State)
            {
                const auto Count = Support.Regions.size();
                if (Support.RegionBedLevels.empty())
                    Support.RegionBedLevels.resize(Count, -std::numeric_limits<float>::infinity());
                for (std::size_t Region = 0; Region < Count; ++Region)
                {
                    const auto &Film = State[Layer * Count + Region];
                    if (Film.Mass > 0.F)
                        Support.RegionBedLevels[Region] = std::max(Support.RegionBedLevels[Region], Film.Level);
                }
            }

            void SelectTopSupport(const FSurfaceSimulationDomain   &Geometry,
                                  FSurfaceSimulationDomain         &Support,
                                  FSurfaceTilt                      Pose,
                                  double                            Time,
                                  std::span<const FFilmRegionState> State)
            {
                if (Time + 1.e-6 < Geometry.FilmBedFreezeStart ||
                    (Geometry.bFilmBedCoupling && Geometry.FilmOmittedSupport < 0))
                    return;
                for (int Region = 0; Region < static_cast<int>(Geometry.Regions.size()); ++Region)
                    RestoreFilmFloor(Geometry, Support, Geometry.FilmLayers.size() - 1, Region, Pose, Time, State);
            }

            void RestoreFilmFloor(const FSurfaceSimulationDomain   &Geometry,
                                  FSurfaceSimulationDomain         &Support,
                                  std::size_t                       Layer,
                                  int                               Region,
                                  FSurfaceTilt                      Pose,
                                  double                            Time,
                                  std::span<const FFilmRegionState> State)
            {
                if (Layer == 0)
                    return;
                const auto Count = Geometry.Regions.size();
                if (Support.RegionBedLevels.empty())
                    Support.RegionBedLevels.resize(Count, -std::numeric_limits<float>::infinity());
                auto &Floor = Support.RegionBedLevels[Region];
                Floor = -std::numeric_limits<float>::infinity();
                const bool bControl =
                    Layer + 1 == Geometry.FilmLayers.size() && Time + 1.e-6 >= Geometry.FilmBedFreezeStart;
                const float Density = Support.Liquid.DensityKgM3;
                for (std::size_t Lower = 0; Lower < Layer; ++Lower)
                {
                    if (bControl && static_cast<int>(Lower) == Geometry.FilmOmittedSupport)
                        continue;
                    const auto  Index = Lower * Count + Region;
                    const float Mass =
                        bControl && !Geometry.bFilmBedCoupling ? Geometry.InitialFilmKg[Index] : State[Index].Mass;
                    if (Mass <= 0.F)
                        continue;
                    Support.Liquid.DensityKgM3 = Geometry.FilmLayers[Lower].DensityKgM3;
                    Floor = RestoreRegionLevel(Support, Region, Pose, Mass);
                }
                Support.Liquid.DensityKgM3 = Density;
            }

            float FilmWetness(const FFilmLayer &Material, const FFilmRegionState &Film)
            {
                return Material.WetWaterFraction > 0.F && Film.Mass > 0.F
                           ? std::clamp(Film.MixedWater / (Film.Mass * Material.WetWaterFraction), 0.F, 1.F)
                           : 0.F;
            }

            void RestoreFilm(const FSurfaceSimulationDomain &Geometry, FSurfaceSimulationState &State, FSurfaceTilt Pose)
            {
                const auto Count = Geometry.Regions.size();
                const auto Layers = std::max<std::size_t>(1, Geometry.FilmLayers.size());
                auto       Support = Layers == 1 ? FSurfaceSimulationDomain{} : Geometry;
                for (std::size_t Layer = 0; Layer < Layers; ++Layer)
                {
                    auto Current = std::span(State.Film).subspan(Layer * Count, Count);
                    if (Layers > 1)
                    {
                        if (Layer + 1 == Layers)
                            SelectTopSupport(Geometry, Support, Pose, State.FilmTime, State.Film);
                        Support.Liquid.DensityKgM3 = Geometry.FilmLayers[Layer].DensityKgM3;
                    }
                    const auto &Bed = Layers == 1 ? Geometry : Support;
                    for (std::size_t Region = 0; Region < Count; ++Region)
                        Current[Region].Level =
                            RestoreRegionLevel(Bed, static_cast<int>(Region), Pose, Current[Region].Mass);
                    if (Layer + 1 == Layers)
                        std::ranges::transform(Current,
                                               State.Regions.begin(),
                                               [](const FFilmRegionState &Film)
                                               { return FPrimaryLiquidRegionState{Film.Mass, Film.Level}; });
                    else
                        AddFilmSupport(Support, Layer, State.Film);
                }
                for (auto &Cell : State.Cells)
                {
                    Cell.Mass = 0.F;
                    Cell.Level = -std::numeric_limits<float>::infinity();
                }
                for (std::size_t Region = 0; Region < Count; ++Region)
                {
                    auto &Cell = State.Cells[Geometry.Regions[Region].Cell];
                    Cell.Mass += State.Regions[Region].Mass;
                    Cell.Level = std::max(Cell.Level, State.Regions[Region].Level);
                }
            }
        } // namespace FilmState
    #pragma endregion

    // ============================================================
    // 01. Initialization
    // ============================================================
    #pragma region 01_Initialization
        // 초기 질량과 지정 온도 및 기판 상태를 배치한다.

        FSurfaceSimulationState InitialState(const FSurfaceSimulationDomain &Geometry)
        {
            FSurfaceSimulationState Result{std::vector<FSurfaceCellState>(Geometry.Cells.size()),
                                           std::vector<FPrimaryLiquidRegionState>(Geometry.Regions.size())};
            const bool bSpreadInitialMass = Geometry.InitialLiquidKg > 0.F && !Geometry.ScheduledInletWeights.empty();
            if (!bSpreadInitialMass)
                Result.Regions[Geometry.SourceRegion()].Mass = Geometry.InitialLiquidKg;
            for (int Region = 0; Region < static_cast<int>(Result.Regions.size()); ++Region)
            {
                auto &Water = Result.Regions[Region];
                if (bSpreadInitialMass)
                    Water.Mass = Geometry.InitialLiquidKg * Geometry.ScheduledInletWeights[Region];
                Water.Level = RestoreRegionLevel(Geometry, Region, {}, Water.Mass);
                Result.Cells[Geometry.Regions[Region].Cell].Mass += Water.Mass;
            }
            for (int Cell = 0; Cell < static_cast<int>(Result.Cells.size()); ++Cell)
            {
                auto      &State = Result.Cells[Cell];
                const auto Span = Geometry.CellRegions[Cell];
                State.Level = Result
                                  .Regions[!bSpreadInitialMass && Cell == Geometry.SourceCell() ? Geometry.SourceRegion()
                                                                                                : Span.First]
                                  .Level;
                if (bSpreadInitialMass)
                    for (const auto &Water : std::span(Result.Regions).subspan(Span.First, Span.Count))
                        if (Water.Mass > 0.F)
                            State.Level = std::max(State.Level, Water.Level);
                State.InverseArea = 1.F / static_cast<float>(CellArea(Geometry, Cell));
                State.TemperatureK =
                    PrescribedTemperatureK(Geometry.Materials[Cell].TemperatureK, {0.F, Geometry.InitialTemperatureWeight});
                if (UsesWaterMaterial(Geometry))
                    InitializeWaterMaterial(Geometry, Cell, State);
            }
            const auto LayerCount = std::max<std::size_t>(1, Geometry.FilmLayers.size());
            const auto RegionCount = Geometry.Regions.size();
            Result.Film.resize(LayerCount * RegionCount);
            for (std::size_t Index = 0; Index < Geometry.InitialFilmKg.size(); ++Index)
                Result.Film[Index].Mass = Geometry.InitialFilmKg[Index];
            for (std::size_t Region = 0; Region < RegionCount; ++Region)
                Result.Film[(LayerCount - 1) * RegionCount + Region].Mass += Result.Regions[Region].Mass;
            FilmState::RestoreFilm(Geometry, Result, {});
            return Result;
        }
    #pragma endregion

} // namespace MDSS::Liquid
