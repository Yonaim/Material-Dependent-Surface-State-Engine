/**
 * @file SurfaceRuntimeData.cpp
 * @brief Runtime-only construction of shared static Surface data.
 */

#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"

#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"

#include <utility>

namespace MDSS
{
    TSurfaceRuntimeData::TSurfaceRuntimeData(TSharedSurfaceGeometryData Geometry)
        : Geometry(std::make_shared<const TSharedSurfaceGeometryData>(std::move(Geometry)))
    {
    }

    std::shared_ptr<const TSharedSurfaceGeometryData> TSurfaceRuntimeData::GetSharedGeometry() const noexcept
    {
        return Geometry;
    }

    TSurfaceRuntimeData TSurfacePreprocessor::Build(const TSurfaceMappingData&        Mapping,
                                                  std::vector<TSurfaceProfileIndex> ProfileMap,
                                                  std::uint32_t                    ProfileCount)
    {
        return TSurfaceRuntimeData(TSurfaceGeometryBuilder::Build(Mapping, std::move(ProfileMap), ProfileCount));
    }
} // namespace MDSS
