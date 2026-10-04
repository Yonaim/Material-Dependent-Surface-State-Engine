/**
 * @file SurfaceGeometryBuilder.h
 * @brief Surface mapping 기반 공유 geometry 구성 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Mapping/SurfaceMappingData.h"

#include <cstdint>
#include <vector>

namespace MDSS::SurfaceState
{
    class TSurfaceGeometryBuilder final
    {
    public:
        /**
         * @brief Copy validated mapping samples into shared geometry and attach their Profile indices.
         * @throws std::invalid_argument for invalid geometry, Profile assignments, or texel values.
         */
        [[nodiscard]] static TSharedSurfaceGeometryData Build(const TSurfaceMappingData&        Mapping,
                                                              std::vector<TSurfaceProfileIndex> ProfileMap,
                                                              std::uint32_t                     ProfileCount);
    };
} // namespace MDSS::SurfaceState
