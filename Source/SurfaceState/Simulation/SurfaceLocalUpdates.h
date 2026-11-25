/**
 * @file SurfaceLocalUpdates.h
 * @brief Surface Simulation의 물 상변화와 바닥 재질 반응 경계를 정의한다.
 */

#pragma once

#include "SurfaceState/Simulation/SurfaceSimulationState.h"

namespace MDSS::Liquid
{

    [[nodiscard]] inline bool UsesWaterMaterial(const FSurfaceSimulationDomain &Geometry)
    {
        return Geometry.Liquid.LocalRule == ELocalMaterialRule::Water;
    }

    /** @brief 주변 온도와 장면 목표를 보간하고 외부 입력의 온도차를 더한다. */
    [[nodiscard]] float PrescribedTemperatureK(float MaterialTargetK, FThermalInput Thermal);

    /** @brief 이전 슬롯의 연소 활성도로 이웃의 온도를 갱신한다. 전파를 끄면 빈 벡터를 반환한다. */
    [[nodiscard]] std::vector<float> CombustionTemperatures(const FSurfaceSimulationDomain    &Geometry,
                                                            std::span<const FSurfaceCellState> Previous,
                                                            FThermalInput                      Thermal,
                                                            float                              Seconds);

    void  InitializeWaterMaterial(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceCellState &State);
    float UpdateWaterMaterial(const FSurfaceSimulationDomain &Geometry,
                              int                             Cell,
                              FSurfaceCellState              &State,
                              FSurfaceTilt                    Pose,
                              float                           Seconds,
                              float                           ContactArea);
} // namespace MDSS::Liquid
