/**
 * @file SurfaceMappingTypes.h
 * @brief Surface mapping 해상도·texel·neighbor 자료형과 sentinel 값.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace MDSS
{
    using TSurfaceLocalID = std::uint32_t;
    using TLocalTexelIndex = std::uint32_t;

    inline constexpr TSurfaceLocalID  InvalidSurfaceID = std::numeric_limits<TSurfaceLocalID>::max();
    inline constexpr TLocalTexelIndex InvalidTexelIndex = std::numeric_limits<TLocalTexelIndex>::max();
    inline constexpr std::uint32_t   InvalidTriangleID = std::numeric_limits<std::uint32_t>::max();
    inline constexpr std::size_t     SurfaceNeighborCount = 8;
    inline constexpr std::uint32_t   SurfaceSimulationResolution = 512;

    struct TSurfaceResolution
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        [[nodiscard]] bool operator==(const TSurfaceResolution&) const = default;

        /**
         * @brief 해상도의 총 texel 수를 계산한다.
         * @throws std::invalid_argument 너비 또는 높이가 0인 경우.
         * @throws std::overflow_error texel 수가 TLocalTexelIndex 범위를 넘는 경우.
         */
        [[nodiscard]] std::size_t GetTexelCount() const;
    };

    inline constexpr TSurfaceResolution DefaultSurfaceResolution{SurfaceSimulationResolution,
                                                                 SurfaceSimulationResolution};

    /** @brief 하나의 Surface ID와 해당 Surface의 simulation grid 해상도. */
    struct TSurfaceDefinition
    {
        TSurfaceLocalID    ID = InvalidSurfaceID;
        TSurfaceResolution Resolution;
    };

    /** @brief mesh-local texel 배열 안에서 한 Surface가 차지하는 연속 구간. */
    struct TSurfaceTexelRange
    {
        TSurfaceLocalID    Surface = InvalidSurfaceID;
        TSurfaceResolution Resolution;
        TLocalTexelIndex   FirstTexel = InvalidTexelIndex;
        TLocalTexelIndex   TexelCount = 0;
    };

    struct TSurfaceGeometryScalar
    {
        float MesoVirtualHeight = 0.0F;
        float ConcavityWeight = 0.0F;
    };

    /**
     * @brief Texel별 CPU-side surface mapping과 geometry 데이터.
     * @note GPU ABI 구조체가 아니다. 업로드 layout은 별도로 정의한다.
     */
    struct TSurfaceTexelGeometry
    {
        TSurfaceLocalID                                    Surface = InvalidSurfaceID;
        std::uint32_t                                     Triangle = InvalidTriangleID;
        glm::vec3                                         Barycentric{0.0F};
        glm::vec3                                         Position{0.0F};
        glm::vec3                                         Normal{0.0F, 1.0F, 0.0F};
        TSurfaceGeometryScalar                             Geometry;
        std::array<TLocalTexelIndex, SurfaceNeighborCount> NeighborIndices = {
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
        };

        /** @brief Surface와 triangle sentinel이 모두 유효한지 확인한다. */
        [[nodiscard]] bool IsValid() const noexcept;
    };

} // namespace MDSS
