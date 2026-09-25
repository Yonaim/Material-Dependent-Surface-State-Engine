/**
 * @file SurfaceMappingValidation.h
 * @brief Surface mapping 결과의 texel graph 불변조건 검증.
 */

#pragma once

#include "SurfaceStateSystem/Mapping/SurfaceMappingData.h"

namespace MDSS
{
    /** @throws std::invalid_argument mapping range 또는 이웃 불변조건이 깨진 경우. */
    void ValidateSurfaceMapping(const SurfaceMappingData& Mapping);
} // namespace MDSS
