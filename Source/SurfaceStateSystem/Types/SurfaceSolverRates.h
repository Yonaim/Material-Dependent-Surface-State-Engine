/**
 * @file SurfaceSolverRates.h
 * @brief C++와 GLSL이 공유하는 전달 속도 상수를 정의하며 Profile 계수 범위는 [0, 1]이다.
 */
#ifndef MDSS_SURFACE_SOLVER_RATES_H
#define MDSS_SURFACE_SOLVER_RATES_H

#define MDSS_BASE_SATURATION_TRANSFER_RATE 1.0
#define MDSS_BASE_GEOMETRY_TRANSFER_RATE 6000.0

#ifdef __cplusplus
namespace MDSS
{
    inline constexpr float BaseSaturationTransferRate = MDSS_BASE_SATURATION_TRANSFER_RATE;
    inline constexpr float BaseGeometryTransferRate = MDSS_BASE_GEOMETRY_TRANSFER_RATE;
}
#endif

#endif
