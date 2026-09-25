/**
 * @file SurfaceRuntimeData.h
 * @brief In-memory static Surface data generated during Runtime loading.
 */

#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingData.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace MDSS
{
    /** @brief Shared static mapping output for a Mesh/Profile Distribution input pair. */
    struct SurfaceRuntimeData
    {
        std::shared_ptr<const SharedSurfaceGeometryData> Geometry;

        explicit SurfaceRuntimeData(SharedSurfaceGeometryData Geometry);
        [[nodiscard]] std::shared_ptr<const SharedSurfaceGeometryData> GetSharedGeometry() const noexcept;
    };

    class SurfacePreprocessor final
    {
    public:
        /** @brief Build in-memory shared geometry from a Runtime mapping and its texel Profile map. */
        [[nodiscard]] static SurfaceRuntimeData Build(const SurfaceMappingData&        Mapping,
                                                      std::vector<SurfaceProfileIndex> ProfileMap,
                                                      std::uint32_t                    ProfileCount);
    };
} // namespace MDSS
