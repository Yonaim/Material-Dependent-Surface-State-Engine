/** @brief Shared C++/GLSL transfer-rate constants; Profile factors remain in [0, 1]. */
#ifndef MDSS_SURFACE_SOLVER_RATES_H
#define MDSS_SURFACE_SOLVER_RATES_H

#define MDSS_BASE_SATURATION_TRANSFER_RATE 1.0
#define MDSS_BASE_GEOMETRY_TRANSFER_RATE 6000.0
#define MDSS_SURFACE_SIMULATION_ACCUMULATION_HEIGHT_REFERENCE 0.01

#ifdef __cplusplus
namespace MDSS
{
    inline constexpr float BaseSaturationTransferRate = MDSS_BASE_SATURATION_TRANSFER_RATE;
    inline constexpr float BaseGeometryTransferRate = MDSS_BASE_GEOMETRY_TRANSFER_RATE;
    inline constexpr float SurfaceSimulationAccumulationHeightReference =
        MDSS_SURFACE_SIMULATION_ACCUMULATION_HEIGHT_REFERENCE;
}
#endif

#endif
