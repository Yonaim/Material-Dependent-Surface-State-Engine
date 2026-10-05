/**
 * @file MesoGeometryBuilder.h
 * @brief 텍셀 이웃 그래프에서 노멀 맵 기반 Meso 높이와 파생 형상을 복원한다.
 */

#pragma once

#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"

#include <cstddef>
#include <cstdint>

namespace MDSS::SurfaceState
{
    struct TMesoGeometryBuildReport
    {
        std::size_t   ActiveTexelCount = 0;
        std::size_t   ComponentCount = 0;
        std::uint32_t IterationCount = 0;
        float         RelativeEdgeResidual = 0.0F;
    };

    /** @brief 샘플링한 노멀 맵 normal을 적분해 텍셀별 Meso 형상 데이터를 제자리에서 만든다. */
    [[nodiscard]] TMesoGeometryBuildReport BuildMesoGeometry(TSharedSurfaceGeometryData& Geometry);
} // MDSS 네임스페이스
