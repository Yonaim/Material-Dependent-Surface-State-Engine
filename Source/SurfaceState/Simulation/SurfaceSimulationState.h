/**
 * @file SurfaceSimulationState.h
 * @brief 표면 시뮬레이션 상태, 고정 스텝 입력 및 초기화를 정의한다.
 */

#pragma once

#include "SurfaceState/Simulation/SurfaceSimulationDomain.h"

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Simulation State
    // ============================================================
    #pragma region 00_Simulation_State
        // 셀 상태와 대표 액체 및 층별 저장 상태를 묶는다.

        /** @brief 독립 액체 질량 [kg]과 중력축 수위 [m]. */
        struct FPrimaryLiquidRegionState
        {
            float Mass = 0.F;
            float Level = 0.F;
        };

        /** @brief 셀의 액체 질량 장부를 kg, 수위를 m, 온도를 K로 저장한다. Char는 0~1 비율이다. */
        struct alignas(16) FSurfaceCellState
        {
            float Mass = 0.F;
            float Level = 0.F;
            float Outflow = 0.F;
            float Evaporated = 0.F;
            float Absorbed = 0.F;
            float Ice = 0.F;
            float Char = 0.F;
            float SteamRate = 0.F;
            float Flame = 0.F;
            float TemperatureK = AmbientTemperatureK;
            float InverseArea = 0.F;
            float PhaseLevel = 0.F;
        };

        /** @brief 총질량과 혼합수는 kg, 수위는 m. 고형분은 두 질량의 차이다. */
        struct FFilmRegionState
        {
            float Mass = 0.F;
            float Level = 0.F;
            float MixedWater = 0.F;
        };

        struct FSurfaceSimulationState
        {
            std::vector<FSurfaceCellState>         Cells;
            std::vector<FPrimaryLiquidRegionState> Regions;
            // 선언 순서로 층을 연속 배치하고 각 층은 동일한 저장 영역 인덱스를 쓴다.
            std::vector<FFilmRegionState> Film;
            double                        FilmTime = 0.0;
        };
    #pragma endregion

    // ============================================================
    // 01. Step Inputs
    // ============================================================
    #pragma region 01_Step_Inputs
        // 고정 스텝에 들어가는 표면 자세와 외부 입력을 정의한다.

        /** @brief 저장 영역 하나에 더하는 수동 주입 질량이다. */
        struct FLiquidDose
        {
            float MassKg = 0.F;
            int   Region = -1;
        };

        /** @brief 재질별 지정 온도를 장면에서 시간에 따라 조절하는 입력이다. */
        struct FThermalInput
        {
            float OffsetK = 0.F;
            float TargetWeight = 1.F; // 주변 온도에서 지정 온도로 향하는 보간 비율(0~1).
        };

        /** @brief 스텝 시작 시 영역별 액체·셀별 고체 질량(kg), 반응 중 적용할 셀별 목표 온도(K). */
        struct FLocalStepInput
        {
            std::vector<float> LiquidKg;
            std::vector<float> IceKg;
            std::vector<float> TemperatureK;

            void Reset(std::size_t Regions, std::size_t Cells)
            {
                LiquidKg.assign(Regions, 0.F);
                IceKg.assign(Cells, 0.F);
                TemperatureK.assign(Cells, 0.F);
            }
        };

        /** @brief 고정 스텝의 자세, 주입 질량(kg), 재질 온도 입력을 묶는다. */
        struct FSurfaceStepInput
        {
            FSurfaceTilt           Pose;
            float                  InputMass = 0.F;
            FThermalInput          Thermal;
            FLiquidDose            Dose;
            const FLocalStepInput *LocalInput = nullptr;
        };
    #pragma endregion

    // ============================================================
    // 02. Initialization
    // ============================================================
    #pragma region 02_Initialization
        // 초기 질량, 온도와 저장 수위를 구성한다.

        /** @brief GPU 및 CPU 경로가 공유하는 시뮬레이션의 초기 상태를 만든다. */
        [[nodiscard]] FSurfaceSimulationState InitialState(const FSurfaceSimulationDomain &Geometry);
    #pragma endregion

} // namespace MDSS::Liquid
