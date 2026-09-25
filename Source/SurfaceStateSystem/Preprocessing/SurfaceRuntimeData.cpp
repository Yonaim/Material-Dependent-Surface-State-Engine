/**
 * @file SurfaceRuntimeData.cpp
 * @brief Runtime-only construction of shared static Surface data.
 */

#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"

#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"

#include <utility>

namespace MDSS
{
    SurfaceRuntimeData::SurfaceRuntimeData(SharedSurfaceGeometryData Geometry)
        : Geometry(std::make_shared<const SharedSurfaceGeometryData>(std::move(Geometry)))
    {
    }

    std::shared_ptr<const SharedSurfaceGeometryData> SurfaceRuntimeData::GetSharedGeometry() const noexcept
    {
        return Geometry;
    }

    SurfaceRuntimeData SurfacePreprocessor::Build(const SurfaceMappingData&        Mapping,
                                                  std::vector<SurfaceProfileIndex> ProfileMap,
                                                  std::uint32_t                    ProfileCount)
    {
        return SurfaceRuntimeData(SurfaceGeometryBuilder::Build(Mapping, std::move(ProfileMap), ProfileCount));
    }
} // namespace MDSS
