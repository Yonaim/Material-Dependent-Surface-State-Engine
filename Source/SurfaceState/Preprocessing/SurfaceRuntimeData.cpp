/**
 * @file SurfaceRuntimeData.cpp
 * @brief Runtime에서 공유 정적 Surface 데이터를 구성한다.
 */

#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"

#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"

#include <utility>

namespace MDSS::SurfaceState
{
#pragma region Runtime_Surface_Data

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
                                                    std::uint32_t                     ProfileCount)
    {
        return TSurfaceRuntimeData(TSurfaceGeometryBuilder::Build(Mapping, std::move(ProfileMap), ProfileCount));
    }
#pragma endregion
} // namespace MDSS::SurfaceState
