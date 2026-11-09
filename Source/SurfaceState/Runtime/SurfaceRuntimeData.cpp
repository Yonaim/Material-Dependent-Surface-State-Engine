/**
 * @file SurfaceRuntimeData.cpp
 * @brief Runtime에서 공유 정적 Surface 데이터를 구성한다.
 */

#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"

#include <utility>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Runtime Surface Data
// ============================================================
#pragma region 00_Runtime_Surface_Data

    FSurfaceRuntimeData::FSurfaceRuntimeData(FSharedSurfaceGeometryData Geometry)
        : Geometry(std::make_shared<const FSharedSurfaceGeometryData>(std::move(Geometry)))
    {
    }

    std::shared_ptr<const FSharedSurfaceGeometryData> FSurfaceRuntimeData::GetSharedGeometry() const noexcept
    {
        return Geometry;
    }

    FSurfaceRuntimeData FSurfacePreprocessor::Build(const FSurfaceMappingData&        Mapping,
                                                    std::vector<FSurfaceProfileIndex> ProfileMap,
                                                    std::uint32_t                     ProfileCount)
    {
        return FSurfaceRuntimeData(FSurfaceGeometryBuilder::Build(Mapping, std::move(ProfileMap), ProfileCount));
    }
#pragma endregion
} // namespace MDSS::SurfaceState
