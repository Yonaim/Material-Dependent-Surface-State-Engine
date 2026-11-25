/**
 * @file SurfaceLocalUpdates.cpp
 * @brief 물의 상변화, 바닥 재질의 반응 및 두 상의 표시 높이를 계산한다.
 */

#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include <algorithm>
#include <cmath>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Water and Substrate Reactions
    // ============================================================
    #pragma region 00_Water_and_Substrate_Reactions
        // 바닥 온도를 읽어 셀 안에서 용융, 동결, 흡수, 건조, 증발, 탄화를 순서대로 반영한다.

        namespace
        {
            float UpdateWaterReactions(FSurfaceCellState           &Water,
                                       const FCellUpdateParameters &Material,
                                       float                        Area,
                                       float                        ContactArea,
                                       float                        Seconds,
                                       bool                         bEvaporateAbsorbedWater)
            {
                const float Melt =
                    std::min(Water.Ice,
                             Material.MeltKgM2SecondK * std::max(0.F, Water.TemperatureK - 273.15F) * Area * Seconds);
                Water.Ice -= Melt;
                Water.Mass += Melt;
                const float Freeze =
                    std::min(Water.Mass,
                             Material.FreezeKgM2SecondK * std::max(0.F, 273.15F - Water.TemperatureK) * Area * Seconds);
                Water.Mass -= Freeze;
                Water.Ice += Freeze;
                const float WetFraction = std::clamp(ContactArea / Area, 0.F, 1.F);
                const float Absorb = std::min(-Water.Mass * std::expm1(-Material.AbsorbPerSecond * Seconds) * WetFraction,
                                              std::max(0.F, Material.CapacityKgM2 * Area - Water.Absorbed));
                Water.Absorbed += Absorb;
                Water.Mass -= Absorb;
                const float Dry = -Water.Absorbed * std::expm1(-Material.DryPerSecond * Seconds);
                Water.Absorbed -= Dry;
                const float VaporThresholdK = bEvaporateAbsorbedWater ? 373.15F : 323.15F;
                const float VaporBudgetKg =
                    Material.EvaporateKgM2SecondK * std::max(0.F, Water.TemperatureK - VaporThresholdK) * Area * Seconds;
                const float SurfaceVaporKg = std::min(Water.Mass, VaporBudgetKg);
                Water.Mass -= SurfaceVaporKg;
                // 표면층과 흡수수에서 빼는 양은 한 기화 한도를 공유한다.
                const float AbsorbedVaporKg =
                    bEvaporateAbsorbedWater ? std::min(Water.Absorbed, VaporBudgetKg - SurfaceVaporKg) : 0.F;
                Water.Absorbed -= AbsorbedVaporKg;
                const float VaporKg = SurfaceVaporKg + AbsorbedVaporKg;
                Water.Evaporated += VaporKg + Dry;
                Water.SteamRate = VaporKg / Seconds;
                const float Wet = std::clamp((Water.Mass + Water.Absorbed) / (Area * 0.4F), 0.F, 1.F);
                const float Heat = std::clamp((Water.TemperatureK - 475.F) / 200.F, 0.F, 1.F);
                const float Activity = Material.CharPerSecond * (1.F - Water.Char) * Heat * (1.F - Wet);
                Water.Char = std::min(1.F, Water.Char + Activity * Seconds);
                Water.Flame = std::clamp(Activity, 0.F, 1.F);
                return Melt;
            }

            void QuenchSubstrate(FSurfaceCellState      &State,
                                 const FSubstrateQuench &Material,
                                 float                   Area,
                                 float                   ContactArea,
                                 float                   Seconds)
            {
                constexpr float BoilingK = 373.15F;
                const float     ExcessK = std::max(0.F, State.TemperatureK - BoilingK);
                const float     CapacityJPerK = Material.ArealHeatCapacityJm2K * Area;
                // 실제 접촉 면적을 쓰되 가용 열과 물 질량으로 전달량을 제한한다.
                const float HeatJ = std::min({Material.ContactConductanceWm2K * ContactArea * ExcessK * Seconds,
                                              CapacityJPerK * ExcessK,
                                              State.Mass * WaterVaporizationJPerKg});
                const float VaporKg = std::min(State.Mass, HeatJ / WaterVaporizationJPerKg);
                State.Mass -= VaporKg;
                State.Evaporated += VaporKg;
                State.SteamRate = VaporKg / Seconds;
                State.TemperatureK -= VaporKg * WaterVaporizationJPerKg / CapacityJPerK;
            }

            void RestorePhaseLevel(const FSurfaceSimulationDomain &Geometry,
                                   int                             Cell,
                                   FSurfaceTilt                    Pose,
                                   FSurfaceCellState              &State)
            {
                if (Geometry.Materials[Cell].FreezeKgM2SecondK > 0.F)
                    State.PhaseLevel = RestoreLevel(
                        Geometry,
                        Cell,
                        Pose,
                        State.Mass + State.Ice * (Geometry.Liquid.DensityKgM3 / Geometry.Liquid.IceDensityKgM3));
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Substrate Temperature and Material Rule
    // ============================================================
    #pragma region 01_Substrate_Temperature_and_Material_Rule
        float PrescribedTemperatureK(float MaterialTargetK, FThermalInput Thermal)
        {
            const float TargetK =
                Thermal.TargetWeight == 1.F
                    ? MaterialTargetK
                    : AmbientTemperatureK + (MaterialTargetK - AmbientTemperatureK) * Thermal.TargetWeight;
            return TargetK + Thermal.OffsetK;
        }

        std::vector<float> CombustionTemperatures(const FSurfaceSimulationDomain    &Geometry,
                                                  std::span<const FSurfaceCellState> Previous,
                                                  FThermalInput                      Thermal,
                                                  float                              Seconds)
        {
            const int Columns = Geometry.CombustionGridColumns;
            if (Columns == 0)
                return {};
            const int          Count = static_cast<int>(Previous.size());
            const float        Relaxation = std::min(1.F, 8.F * Seconds);
            std::vector<float> Temperatures(Previous.size());
            for (int Cell = 0; Cell < Count; ++Cell)
            {
                const int   Column = Cell % Columns;
                const auto &State = Previous[Cell];
                float       Flame = State.Flame;
                if (Column > 0)
                    Flame = std::max(Flame, Previous[Cell - 1].Flame);
                if (Column + 1 < Columns)
                    Flame = std::max(Flame, Previous[Cell + 1].Flame);
                if (Cell >= Columns)
                    Flame = std::max(Flame, Previous[Cell - Columns].Flame);
                if (Cell + Columns < Count)
                    Flame = std::max(Flame, Previous[Cell + Columns].Flame);
                // 연소 활성도가 온도장을 구동하는 시연용 근사다. 이웃은 모두 이전 슬롯에서 읽는다.
                const float CombustionK = AmbientTemperatureK + 500.F * std::min(1.F, 2.F * Flame);
                const float PrescribedK = PrescribedTemperatureK(Geometry.Materials[Cell].TemperatureK, Thermal);
                Temperatures[Cell] =
                    State.TemperatureK + (std::max(CombustionK, PrescribedK) - State.TemperatureK) * Relaxation;
            }
            return Temperatures;
        }

        void InitializeWaterMaterial(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceCellState &State)
        {
            State.Ice = Geometry.InitialIceKgM2[Cell] * static_cast<float>(CellArea(Geometry, Cell));
            RestorePhaseLevel(Geometry, Cell, {}, State);
        }

        float UpdateWaterMaterial(const FSurfaceSimulationDomain &Geometry,
                                  int                             Cell,
                                  FSurfaceCellState              &State,
                                  FSurfaceTilt                    Pose,
                                  float                           Seconds,
                                  float                           ContactArea)
        {
            if (Geometry.SubstrateQuench.ArealHeatCapacityJm2K > 0.F)
            {
                QuenchSubstrate(State,
                                Geometry.SubstrateQuench,
                                static_cast<float>(CellArea(Geometry, Cell)),
                                ContactArea,
                                Seconds);
                return 0.F;
            }
            const float Melted = UpdateWaterReactions(State,
                                                      Geometry.Materials[Cell],
                                                      static_cast<float>(CellArea(Geometry, Cell)),
                                                      ContactArea,
                                                      Seconds,
                                                      Geometry.bEvaporateAbsorbedWater);
            RestorePhaseLevel(Geometry, Cell, Pose, State);
            return Melted;
        }

    #pragma endregion
} // namespace MDSS::Liquid
