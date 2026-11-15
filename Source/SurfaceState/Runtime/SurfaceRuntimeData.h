/**
 * @file SurfaceRuntimeData.h
 * @brief Runtime 로딩 중 생성되는 메모리 상주 정적 Surface 데이터를 선언한다.
 */

#pragma once

#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Mapping/SurfaceMappingData.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace MDSS::SurfaceState
{
    using FSurfaceRuntimeDataHandle = std::uint32_t;
    inline constexpr FSurfaceRuntimeDataHandle InvalidSurfaceRuntimeDataHandle =
        std::numeric_limits<FSurfaceRuntimeDataHandle>::max();

    /** @brief Shared static mapping output for a Mesh/Profile Distribution input pair. */
    struct FSurfaceRuntimeData
    {
        std::shared_ptr<const FSharedSurfaceGeometryData> Geometry;

        explicit FSurfaceRuntimeData(FSharedSurfaceGeometryData Geometry);
        [[nodiscard]] std::shared_ptr<const FSharedSurfaceGeometryData> GetSharedGeometry() const noexcept;
    };

    class FSurfacePreprocessor final
    {
    public:
        /** @brief Build in-memory shared geometry from a Runtime mapping and its texel Profile map. */
        [[nodiscard]] static FSurfaceRuntimeData Build(const FSurfaceMappingData&        Mapping,
                                                       std::vector<FSurfaceProfileIndex> ProfileMap,
                                                       std::uint32_t                     ProfileCount);
    };
} // namespace MDSS::SurfaceState
