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
    using SurfaceLocalID = std::uint32_t;
    using LocalTexelIndex = std::uint32_t;

    inline constexpr SurfaceLocalID  InvalidSurfaceID = std::numeric_limits<SurfaceLocalID>::max();
    inline constexpr LocalTexelIndex InvalidTexelIndex = std::numeric_limits<LocalTexelIndex>::max();
    inline constexpr std::uint32_t   InvalidTriangleID = std::numeric_limits<std::uint32_t>::max();
    inline constexpr std::size_t     SurfaceNeighborCount = 8;

    struct SurfaceResolution
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        [[nodiscard]] bool operator==(const SurfaceResolution&) const = default;

        /**
         * @brief 해상도의 총 texel 수를 계산한다.
         * @throws std::invalid_argument 너비 또는 높이가 0인 경우.
         * @throws std::overflow_error texel 수가 LocalTexelIndex 범위를 넘는 경우.
         */
        [[nodiscard]] std::size_t GetTexelCount() const;
    };

    /** @brief 하나의 Surface ID와 해당 Surface의 simulation grid 해상도. */
    struct SurfaceDefinition
    {
        SurfaceLocalID    ID = InvalidSurfaceID;
        SurfaceResolution Resolution;
    };

    /** @brief mesh-local texel 배열 안에서 한 Surface가 차지하는 연속 구간. */
    struct SurfaceTexelRange
    {
        SurfaceLocalID    Surface = InvalidSurfaceID;
        SurfaceResolution Resolution;
        LocalTexelIndex   FirstTexel = InvalidTexelIndex;
        LocalTexelIndex   TexelCount = 0;
    };

    struct SurfaceGeometryScalar
    {
        float MesoVirtualHeight = 0.0F;
        float ConcavityWeight = 0.0F;
    };

    /**
     * @brief Texel별 CPU-side surface mapping과 geometry 데이터.
     * @note GPU ABI 구조체가 아니다. 업로드 layout은 별도로 정의한다.
     */
    struct SurfaceTexelGeometry
    {
        SurfaceLocalID                                    Surface = InvalidSurfaceID;
        std::uint32_t                                     Triangle = InvalidTriangleID;
        glm::vec3                                         Barycentric{0.0F};
        glm::vec3                                         Position{0.0F};
        glm::vec3                                         Normal{0.0F, 1.0F, 0.0F};
        SurfaceGeometryScalar                             Geometry;
        std::array<LocalTexelIndex, SurfaceNeighborCount> NeighborIndices = {
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
        };
        std::array<float, SurfaceNeighborCount> NeighborDistances{};

        /** @brief Surface와 triangle sentinel이 모두 유효한지 확인한다. */
        [[nodiscard]] bool IsValid() const noexcept;
    };

} // namespace MDSS
