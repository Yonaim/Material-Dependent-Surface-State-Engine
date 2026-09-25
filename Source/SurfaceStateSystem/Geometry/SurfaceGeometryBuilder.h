/**
 * @file SurfaceGeometryBuilder.h
 * @brief Build shared CPU geometry and texel Profile mapping from Surface mapping.
 */

#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingData.h"

#include <cstdint>
#include <vector>

namespace MDSS
{
    class TSurfaceGeometryBuilder final
    {
    public:
        /**
         * @brief Copy validated mapping samples into shared geometry and attach their Profile indices.
         * @throws std::invalid_argument for invalid geometry, Profile assignments, or texel values.
         */
        [[nodiscard]] static TSharedSurfaceGeometryData Build(const TSurfaceMappingData&              Mapping,
                                                             std::vector<TSurfaceProfileIndex>       ProfileMap,
                                                             std::uint32_t                          ProfileCount);
    };
} // namespace MDSS
