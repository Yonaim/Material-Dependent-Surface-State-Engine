/**
 * @file SurfaceSimulationReference.cpp
 * @brief 표면 시뮬레이션의 CPU 기준 수송과 반응을 계산한다.
 */

#include "SurfaceState/Simulation/SurfaceSimulationReference.h"
#include "SurfaceState/Simulation/SurfaceFilmState.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Face Transport Helpers
    // ============================================================
    #pragma region 00_Face_Transport_Helpers
        // 면을 건너는 후보 질량을 계산한다.

        namespace
        {
            double FaceDistance(const FSurfaceSimulationDomain &Geometry, FTransportFace Face, bool bX)
            {
                const auto   Left = Geometry.Cells[Geometry.Regions[Face.Left].Cell];
                const auto   Right = Face.Right < 0 ? Left : Geometry.Cells[Geometry.Regions[Face.Right].Cell];
                const int    LeftCenter = bX ? Left.FirstColumn + Left.EndColumn : Left.FirstRow + Left.EndRow;
                const int    RightCenter = bX ? Right.FirstColumn + Right.EndColumn : Right.FirstRow + Right.EndRow;
                const double Separation =
                    Face.Right < 0 ? (Left.EndColumn - Left.FirstColumn) * 0.5 : std::abs(RightCenter - LeftCenter) * 0.5;
                return (bX ? Geometry.Shape.PitchX() : Geometry.Shape.PitchY()) * (Separation > 0 ? Separation : 1);
            }

            double
            AcrossFace(const FSurfaceSimulationDomain &Geometry, const FSurfaceSimulationState &State, int Region, bool bX)
            {
                double Sum = 0;
                int    Count = 0;
                for (const int Index : Geometry.RegionFaceRange(Region).In(Geometry.RegionFaces))
                {
                    const auto Face = Geometry.Faces[Index];
                    if (Face.Right < 0 || (Geometry.Contacts[Face.FirstContact] % 2 == 0) == bX)
                        continue;
                    const auto Left = State.Regions[Face.Left];
                    const auto Right = State.Regions[Face.Right];
                    if (Left.Mass <= 0.F || Right.Mass <= 0.F)
                        continue;
                    Sum += (Left.Level - Right.Level) / FaceDistance(Geometry, Face, !bX);
                    ++Count;
                }
                return Count ? Sum / Count : 0;
            }

            double FaceMass(const FSurfaceSimulationDomain &Geometry,
                            const FSurfaceSimulationState  &Water,
                            const FLiquidMaterial          &Material,
                            FTransportFace                  Face,
                            FSurfaceTilt                    Pose,
                            float                           Seconds,
                            bool                           *bHeadLimited = nullptr)
            {
                const float Density = Material.DensityKgM3;
                const auto &Shape = Geometry.Shape;
                const bool  bOutlet = Face.Right < 0;
                double      Integral = 0.0;
                double      LowestSill = std::numeric_limits<double>::max();
                for (const int Contact : FIndexRange{Face.FirstContact, Face.ContactCount}.In(Geometry.Contacts))
                {
                    const bool   bX = (Contact % 2) == 0;
                    const double Span = bX ? Shape.PitchY() : Shape.PitchX();
                    const double Distance = FaceDistance(Geometry, Face, bX);
                    const double Sill = SillElevation(Geometry, Face, Contact, Pose);
                    LowestSill = std::min(LowestSill, Sill);
                    const double LeftLevel = Water.Regions[Face.Left].Level;
                    const double RightLevel = bOutlet ? Sill : Water.Regions[Face.Right].Level;
                    const double Depth = std::max(0.0, std::max(LeftLevel, RightLevel) - Sill);
                    const double Drop = std::max(LeftLevel, Sill) - std::max(RightLevel, Sill);
                    double       Mobility = 1;
                    if (Material.Friction > 0 || Material.YieldStressPa > 0)
                    {
                        const double Cross = bOutlet ? AcrossFace(Geometry, Water, Face.Left, bX)
                                                     : 0.5 * (AcrossFace(Geometry, Water, Face.Left, bX) +
                                                              AcrossFace(Geometry, Water, Face.Right, bX));
                        const double Slope = std::hypot(Drop / Distance, Cross);
                        const double Excess = Slope - Material.Friction;
                        const double Stress = Density * Gravity * Depth * Excess;
                        if (Excess <= 0 || Stress <= Material.YieldStressPa)
                            continue;
                        const double Ratio = Material.YieldStressPa / Stress;
                        Mobility = Excess / Slope * (1 - Ratio) * (1 - Ratio) * (1 + 0.5 * Ratio);
                    }
                    Integral += Span / Distance * Depth * Depth * Depth * Drop * Mobility;
                }
                const double Proposed = static_cast<double>(Density) * Density * Gravity * Seconds * Integral /
                                        (3.0 * Material.DynamicViscosityPaS);
                if (Proposed == 0.0)
                    return 0.0;

                // 접촉이 여럿인 영역도 같은 질량과 저장량을 반복해서 사용하지 않는다.
                const int  Donor = Proposed > 0.0 ? Face.Left : Face.Right;
                const int  Receiver = Proposed > 0.0 ? Face.Right : Face.Left;
                const auto From = Water.Regions[Donor];
                const int  Degree =
                    std::max({4, Geometry.FaceCounts[Donor], Receiver < 0 ? 0 : Geometry.FaceCounts[Receiver]});
                const double RetainAt =
                    Receiver < 0 ? LowestSill : std::max(LowestSill, 0.5 * (From.Level + Water.Regions[Receiver].Level));
                double Available = From.Mass - RegionVolume(Geometry, Donor, Pose, RetainAt) * Density;
                if (Receiver >= 0)
                {
                    const auto   To = Water.Regions[Receiver];
                    const double Take = RegionVolume(Geometry, Receiver, Pose, RetainAt) * Density - To.Mass;
                    Available = std::min(Available, Take);
                }
                const double Limit = std::max(0.0, Available) / Degree;
                if (bHeadLimited)
                    *bHeadLimited = std::abs(Proposed) > Limit;
                return std::copysign(std::min(std::abs(Proposed), Limit), Proposed);
            }
        } // namespace
    #pragma endregion

    // ============================================================
    // 01. Donor Outflow Limit
    // ============================================================
    #pragma region 01_Donor_Outflow_Limit
        // 공급 분지마다 가용 질량을 기준으로 유출을 제한한다.

        std::vector<float> LimitOutflow(const FSurfaceSimulationDomain &Geometry,
                                        const FSurfaceSimulationState  &Water,
                                        const std::vector<float>       &FaceCandidateMassKg)
        {
            std::vector<float> Outgoing(Geometry.Regions.size());
            for (std::size_t Index = 0; Index < Geometry.Faces.size(); ++Index)
            {
                const FTransportFace Face = Geometry.Faces[Index];
                const float          Candidate = FaceCandidateMassKg[Index];
                const int            Donor = Candidate >= 0.F ? Face.Left : Face.Right;
                if (Donor >= 0)
                    Outgoing[Donor] += std::abs(Candidate);
            }
            for (std::size_t Cell = 0; Cell < Geometry.Regions.size(); ++Cell)
                Outgoing[Cell] =
                    Outgoing[Cell] > Water.Regions[Cell].Mass ? Water.Regions[Cell].Mass / Outgoing[Cell] : 1.F;
            return Outgoing;
        }
    #pragma endregion

    // ============================================================
    // 02. Layered Reference Helpers
    // ============================================================
    #pragma region 02_Layered_Reference_Helpers
        // 층별 수송과 국소 갱신을 계산한다.

        namespace
        {
            void UpdateTemperatures(const FSurfaceSimulationDomain &Geometry,
                                    FSurfaceSimulationState        &State,
                                    float                           Seconds,
                                    FThermalInput                   Thermal,
                                    const FLocalStepInput          *LocalInput)
            {
                const auto Combustion = CombustionTemperatures(Geometry, State.Cells, Thermal, Seconds);
                for (std::size_t Cell = 0; Cell < State.Cells.size(); ++Cell)
                {
                    const float EditorTemperature = LocalInput ? LocalInput->TemperatureK[Cell] : 0.F;
                    if (EditorTemperature > 0.F)
                        State.Cells[Cell].TemperatureK = EditorTemperature;
                    else if (Geometry.SubstrateQuench.ArealHeatCapacityJm2K <= 0.F)
                        State.Cells[Cell].TemperatureK =
                            Combustion.empty() ? PrescribedTemperatureK(Geometry.Materials[Cell].TemperatureK, Thermal)
                                               : Combustion[Cell];
                }
            }

            // 계산 셀의 남은 흡수 용량은 하위 저장 영역이 공유한다.
            void UpdateSubstrate(const FSurfaceSimulationDomain &Geometry,
                                 FSurfaceReferenceStepResult    &Result,
                                 std::span<FFilmRegionState>     Top,
                                 FSurfaceTilt                    Pose,
                                 float                           Seconds,
                                 const FLocalStepInput          *LocalInput)
            {
                auto      &Next = Result.State;
                const auto Last = Geometry.FilmLayers.empty() ? 0 : Geometry.FilmLayers.size() - 1;
                auto       Support = Last > 0 ? Geometry : FSurfaceSimulationDomain{};
                auto      &Bed = Last > 0 ? Support : Geometry;
                for (int Cell = 0; Cell < static_cast<int>(Geometry.Cells.size()); ++Cell)
                {
                    auto      &State = Next.Cells[Cell];
                    const auto Span = Geometry.CellRegions[Cell];
                    auto       Regions = Top.subspan(Span.First, Span.Count);
                    float      Transported = 0.F;
                    for (auto &Region : Regions)
                    {
                        Region.Mass = std::max(0.F, Region.Mass);
                        Transported += Region.Mass;
                    }
                    State.Mass = Transported;
                    float Melted = 0.F;
                    if (UsesWaterMaterial(Geometry))
                    {
                        if (LocalInput)
                            State.Ice += LocalInput->IceKg[Cell];
                        double     ContactArea = 0.0;
                        double     ThermalContactArea = 0.0;
                        const bool bQuench = Geometry.SubstrateQuench.ArealHeatCapacityJm2K > 0.F;
                        if (Geometry.Materials[Cell].AbsorbPerSecond > 0.F || bQuench)
                        {
                            for (int Offset = 0; Offset < Span.Count; ++Offset)
                            {
                                const int   Id = Span.First + Offset;
                                const float Mass = Regions[Offset].Mass;
                                if (Mass <= 0.F)
                                    continue;
                                FilmState::RestoreFilmFloor(Geometry, Support, Last, Id, Pose, Next.FilmTime, Next.Film);
                                const float Level = RestoreRegionLevel(Bed, Id, Pose, Mass);
                                ContactArea += RegionWetArea(Bed, Id, Pose, Level);
                                if (bQuench)
                                    ThermalContactArea += RegionWetArea(Bed, Id, Pose, Level, ThermalContactDepthM);
                            }
                        }
                        Result.ContactAreaM2 += ContactArea;
                        Melted = UpdateWaterMaterial(Geometry,
                                                     Cell,
                                                     State,
                                                     Pose,
                                                     Seconds,
                                                     static_cast<float>(bQuench ? ThermalContactArea : ContactArea));
                    }
                    // 녹은 물은 균일한 초기 얼음 면밀도를 기준으로, 제거량은 접촉한 액체에 비례하여 배분한다.
                    const float BeforeRemoval = Transported + Melted;
                    const float Retained = BeforeRemoval > 0.F ? State.Mass / BeforeRemoval : 0.F;
                    const float MeltPerSample =
                        Melted / static_cast<float>(std::accumulate(Geometry.Regions.begin() + Span.First,
                                                                    Geometry.Regions.begin() + Span.First + Span.Count,
                                                                    0,
                                                                    [](int Sum, const FStorageRegion &Region)
                                                                    { return Sum + Region.SampleCount; }));
                    for (int Offset = 0; Offset < Span.Count; ++Offset)
                    {
                        auto &Region = Regions[Offset];
                        Region.Mass =
                            Span.Count == 1
                                ? State.Mass
                                : (Region.Mass + MeltPerSample * Geometry.Regions[Span.First + Offset].SampleCount) *
                                      Retained;
                    }
                }
            }

            FSurfaceReferenceStepResult AdvanceFilm(const FSurfaceSimulationDomain &Geometry,
                                                    FSurfaceSimulationState         State,
                                                    FSurfaceTilt                    Pose,
                                                    float                           Seconds,
                                                    float                           InputMass,
                                                    FThermalInput                   Thermal,
                                                    const FLocalStepInput          *LocalInput)
            {
                FSurfaceReferenceStepResult Result{std::move(State), std::vector<float>(Geometry.Faces.size()), {}};
                auto                       &Next = Result.State;
                const auto                  Count = Geometry.Regions.size();
                const auto                  Default = PrimaryFilmMaterial(Geometry.Liquid);
                const auto Layers = Geometry.FilmLayers.empty() ? std::span<const FFilmLayer>(&Default, 1)
                                                                : std::span<const FFilmLayer>(Geometry.FilmLayers);
                const auto Last = Layers.size() - 1;
                auto       Top = std::span(Next.Film).subspan(Last * Count, Count);
                for (std::size_t I = 0; I < Count; ++I)
                {
                    const float Weight = Geometry.ScheduledInletWeights.empty()
                                             ? static_cast<float>(I == Geometry.SourceRegion())
                                             : Geometry.ScheduledInletWeights[I];
                    Top[I].Mass = Next.Regions[I].Mass + InputMass * Weight + (LocalInput ? LocalInput->LiquidKg[I] : 0.F);
                }
                FilmState::RestoreFilm(Geometry, Next, Pose);
                const int   Substeps = std::max(1, Geometry.TransportSubsteps);
                const float TransportSeconds = Seconds / static_cast<float>(Substeps);
                Result.PrimaryFaceTransferredMassKg.resize(Geometry.Faces.size());
                for (int Substep = 0; Substep < Substeps; ++Substep)
                {
                    const auto              Before = Next.Film;
                    auto                    Support = Last > 0 ? Geometry : FSurfaceSimulationDomain{};
                    FSurfaceSimulationState LayerState;
                    LayerState.Regions.resize(Count);
                    for (std::size_t Layer = 0; Layer <= Last; ++Layer)
                    {
                        const auto &Material = Layers[Layer];
                        if (Last > 0 && Layer == Last)
                            FilmState::SelectTopSupport(Geometry, Support, Pose, Next.FilmTime, Before);
                        const auto     &Bed = Last > 0 ? Support : Geometry;
                        FLiquidMaterial Properties = Geometry.Liquid;
                        Properties.DensityKgM3 = Material.DensityKgM3;
                        const auto OldLayer = std::span(Before).subspan(Layer * Count, Count);
                        std::ranges::transform(OldLayer,
                                               LayerState.Regions.begin(),
                                               [](const FFilmRegionState &Film)
                                               { return FPrimaryLiquidRegionState{Film.Mass, Film.Level}; });
                        for (std::size_t I = 0; I < Geometry.Faces.size(); ++I)
                        {
                            const auto Face = Geometry.Faces[I];
                            const auto Donor = Face.Right < 0 || OldLayer[Face.Left].Level >= OldLayer[Face.Right].Level
                                                   ? Face.Left
                                                   : Face.Right;
                            auto       Effective = Material;
                            if (Material.WetWaterFraction > 0.F)
                            {
                                const float Wet = FilmState::FilmWetness(Material, OldLayer[Donor]);
                                Effective.YieldStressPa = std::lerp(Material.DryYieldStressPa, Material.YieldStressPa, Wet);
                                Effective.ViscosityPaS *= std::lerp(Material.DryViscosityMultiplier, 1.F, Wet);
                            }
                            Properties.DynamicViscosityPaS = Effective.ViscosityPaS;
                            Properties.Friction = Effective.Friction;
                            Properties.YieldStressPa = Effective.YieldStressPa;
                            bool bLimited = false;
                            Result.FaceCandidateMassKg[I] =
                                (Face.Right >= 0 ? Geometry.bTransport : Geometry.bExternalExchange)
                                    ? static_cast<float>(
                                          FaceMass(Bed, LayerState, Properties, Face, Pose, TransportSeconds, &bLimited))
                                    : 0.F;
                            Result.HeadLimitedFaces += static_cast<unsigned>(bLimited);
                        }
                        Result.RegionOutflowScale = LimitOutflow(Geometry, LayerState, Result.FaceCandidateMassKg);
                        Result.DonorLimitedRegions += static_cast<unsigned>(
                            std::ranges::count_if(Result.RegionOutflowScale, [](float Scale) { return Scale < 1.F; }));
                        for (std::size_t I = 0; I < Geometry.Faces.size(); ++I)
                        {
                            const auto  Face = Geometry.Faces[I];
                            const float Candidate = Result.FaceCandidateMassKg[I];
                            const int   Donor = Candidate >= 0.F ? Face.Left : Face.Right;
                            const float Amount = Donor >= 0 ? Candidate * Result.RegionOutflowScale[Donor] : 0.F;
                            const float MixedWater = Donor >= 0 && OldLayer[Donor].Mass > 0.F
                                                         ? Amount * OldLayer[Donor].MixedWater / OldLayer[Donor].Mass
                                                         : 0.F;
                            if (Material.WetWaterFraction > 0.F && Face.Right >= 0)
                                Result.FilmSolidMovedKg += std::abs(Amount - MixedWater);
                            auto &Left = Next.Film[Layer * Count + Face.Left];
                            Left.Mass -= Amount;
                            Left.MixedWater -= MixedWater;
                            if (Face.Right >= 0)
                            {
                                auto &Right = Next.Film[Layer * Count + Face.Right];
                                Right.Mass += Amount;
                                Right.MixedWater += MixedWater;
                            }
                            else
                            {
                                Next.Cells[Geometry.Regions[Face.Left].Cell].Outflow += Amount;
                                Result.FilmWaterOutKg += Layer == Last ? Amount : MixedWater;
                            }
                            if (Layer == Last)
                                Result.PrimaryFaceTransferredMassKg[I] += Amount;
                        }
                        for (auto &Film : std::span(Next.Film).subspan(Layer * Count, Count))
                        {
                            Film.Mass = std::max(0.F, Film.Mass);
                            Film.MixedWater = std::clamp(Film.MixedWater, 0.F, Film.Mass);
                        }
                        if (Layer < Last)
                            FilmState::AddFilmSupport(Support, Layer, Before);
                    }
                    if (Substep + 1 < Substeps)
                        FilmState::RestoreFilm(Geometry, Next, Pose);
                }
                UpdateTemperatures(Geometry, Next, Seconds, Thermal, LocalInput);
                auto ReactionBed = Geometry.FilmReactions.empty() ? FSurfaceSimulationDomain{} : Geometry;
                for (std::size_t Region = 0; Region < Count && !Geometry.FilmReactions.empty(); ++Region)
                {
                    const float Temperature = Next.Cells[Geometry.Regions[Region].Cell].TemperatureK;
                    for (const auto Rule : Geometry.FilmReactions)
                    {
                        auto &From = Next.Film[Rule.From * Count + Region].Mass;
                        auto &To = Next.Film[Rule.To * Count + Region];
                        auto &Water = Next.Film[Rule.Water * Count + Region].Mass;
                        if (Rule.WaterPerSoil == 0.F)
                        {
                            const float Area = static_cast<float>(FilmState::FilmArea(Geometry, static_cast<int>(Region)));
                            const float Amount = std::min(
                                From,
                                Rule.Rate * Area * Seconds *
                                    std::max(0.F,
                                             Rule.TemperatureDirection * (Temperature - Rule.EquilibriumTemperatureK)));
                            From -= Amount;
                            To.Mass += Amount;
                            continue;
                        }

                        const float Dry = To.MixedWater * (1.F - std::exp(-Rule.DryPerSecond * Seconds));
                        To.Mass -= Dry;
                        To.MixedWater -= Dry;
                        Next.Cells[Geometry.Regions[Region].Cell].Evaporated += Dry;
                        if (Water <= 0.F || (From <= 0.F && To.Mass <= 0.F))
                            continue;

                        const int Id = static_cast<int>(Region);
                        FilmState::RestoreFilmFloor(Geometry, ReactionBed, Rule.Water, Id, Pose, Next.FilmTime, Next.Film);
                        ReactionBed.Liquid.DensityKgM3 = Geometry.FilmLayers[Rule.Water].DensityKgM3;
                        const float Level = RestoreRegionLevel(ReactionBed, Id, Pose, Water);
                        const float ContactArea = static_cast<float>(RegionWetArea(ReactionBed, Id, Pose, Level));
                        Result.ContactAreaM2 += ContactArea;
                        if (Next.FilmTime + 1.e-6 >= Rule.RewetStartS)
                        {
                            const float Capacity =
                                std::max(0.F, (To.Mass - To.MixedWater) * Rule.WaterPerSoil - To.MixedWater);
                            const float Rewet = std::min({Water, Capacity, Rule.RewetKgM2Second * ContactArea * Seconds});
                            Water -= Rewet;
                            To.Mass += Rewet;
                            To.MixedWater += Rewet;
                            Result.FilmRewetKg += Rewet;
                        }
                        const float Soil = std::min({From, Rule.Rate * ContactArea * Seconds, Water / Rule.WaterPerSoil});
                        const float Mixed = Soil * Rule.WaterPerSoil;
                        From -= Soil;
                        Water = std::max(0.F, Water - Mixed);
                        To.Mass += Soil + Mixed;
                        To.MixedWater += Mixed;
                    }
                }
                UpdateSubstrate(Geometry, Result, Top, Pose, Seconds, LocalInput);
                Next.FilmTime += Seconds;
                FilmState::RestoreFilm(Geometry, Next, Pose);
                return Result;
            }
        } // namespace
    #pragma endregion

    // ============================================================
    // 03. Reference Step
    // ============================================================
    #pragma region 03_Reference_Step
        // 한 고정 스텝의 상태와 진단을 반환한다.

        FSurfaceReferenceStepResult StepReference(const FSurfaceSimulationDomain &Geometry,
                                                  FSurfaceSimulationState         Current,
                                                  FSurfaceTilt                    Pose,
                                                  float                           Seconds,
                                                  float                           InputMass,
                                                  FThermalInput                   Thermal,
                                                  const FLocalStepInput          *LocalInput)
        {
            if (!Geometry.bExternalExchange)
            {
                InputMass = 0.F;
                Thermal = {0.F, Geometry.InitialTemperatureWeight};
                LocalInput = nullptr;
            }
            return AdvanceFilm(Geometry, std::move(Current), Pose, Seconds, InputMass, Thermal, LocalInput);
        }
    #pragma endregion

} // namespace MDSS::Liquid
