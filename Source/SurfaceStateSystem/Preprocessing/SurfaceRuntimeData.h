/**
 * @file SurfaceRuntimeData.h
 * @brief Runtime 로딩 중 생성되는 메모리 상주 정적 Surface 데이터를 선언한다.
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
    struct TSurfaceRuntimeData
    {
        std::shared_ptr<const TSharedSurfaceGeometryData> Geometry;

        explicit TSurfaceRuntimeData(TSharedSurfaceGeometryData Geometry);
        [[nodiscard]] std::shared_ptr<const TSharedSurfaceGeometryData> GetSharedGeometry() const noexcept;
    };

    class TSurfacePreprocessor final
    {
    public:
        /** @brief Build in-memory shared geometry from a Runtime mapping and its texel Profile map. */
        [[nodiscard]] static TSurfaceRuntimeData Build(const TSurfaceMappingData&        Mapping,
                                                      std::vector<TSurfaceProfileIndex> ProfileMap,
                                                      std::uint32_t                    ProfileCount);
    };
} // namespace MDSS
