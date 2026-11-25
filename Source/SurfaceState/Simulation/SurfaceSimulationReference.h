/**
 * @file SurfaceSimulationReference.h
 * @brief CPU 기준 스텝과 수치 검증용 진단값을 정의한다.
 */

#pragma once

#include "SurfaceState/Simulation/SurfaceSimulationState.h"

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Reference Step
    // ============================================================
    #pragma region 00_Reference_Step
        // CPU에서 박막 수송과 반응을 계산하고 검증 항목을 반환한다.

        struct FSurfaceReferenceStepResult
        {
            FSurfaceSimulationState State;
            std::vector<float>      FaceCandidateMassKg;
            std::vector<float>      RegionOutflowScale;
            std::vector<float>      PrimaryFaceTransferredMassKg;
            double                  FilmWaterOutKg = 0.0;
            double                  FilmSolidMovedKg = 0.0;
            double                  FilmRewetKg = 0.0;
            double                  ContactAreaM2 = 0.0;
            unsigned                HeadLimitedFaces = 0;
            unsigned                DonorLimitedRegions = 0;
        };

        /** @brief 부호 있는 면별 이동 요청(kg)을 공급 분지에 합산해 분지당 공통 배율(0~1)을 반환한다. */
        [[nodiscard]] std::vector<float> LimitOutflow(const FSurfaceSimulationDomain &Geometry,
                                                      const FSurfaceSimulationState  &Water,
                                                      const std::vector<float>       &FaceCandidateMassKg);
        /**
         * @brief 주입, 셀 간 수송, 국소 갱신, 수위 복원을 한 스텝 진행한다.
         * @param Seconds 스텝 길이(s).
         * @param InputMass 이번 스텝에 유입되는 질량(kg).
         * @param Thermal 재질 온도의 장면 보간 비율과 오프셋(K).
         */
        [[nodiscard]] FSurfaceReferenceStepResult StepReference(const FSurfaceSimulationDomain &Geometry,
                                                                FSurfaceSimulationState         Current,
                                                                FSurfaceTilt                    Pose,
                                                                float                           Seconds,
                                                                float                           InputMass,
                                                                FThermalInput                   Thermal = {},
                                                                const FLocalStepInput          *LocalInput = nullptr);
    #pragma endregion

} // namespace MDSS::Liquid
