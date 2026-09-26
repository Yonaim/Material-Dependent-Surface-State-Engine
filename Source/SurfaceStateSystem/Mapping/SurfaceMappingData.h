/**
 * @file SurfaceMappingData.h
 * @brief UV rasterization으로 생성한 CPU-side texel graph 자료형.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <string>
#include <vector>

namespace MDSS
{
    inline constexpr std::uint32_t InvalidChartID = std::numeric_limits<std::uint32_t>::max();

    struct TSurfaceMappingTexel
    {
        TSurfaceLocalID                                    Surface = InvalidSurfaceID;
        std::uint32_t                                     Triangle = InvalidTriangleID;
        std::uint32_t                                     Chart = InvalidChartID;
        glm::vec3                                         Barycentric{0.0F};
        glm::vec3                                         Position{0.0F};
        glm::vec3                                         Normal{0.0F, 0.0F, 1.0F};
        std::array<TLocalTexelIndex, SurfaceNeighborCount> Neighbors = {
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
        };

        [[nodiscard]] bool IsValid() const noexcept
        {
            return Surface != InvalidSurfaceID && Triangle != InvalidTriangleID && Chart != InvalidChartID;
        }
    };

    struct TSurfaceMappingData
    {
        std::vector<TSurfaceTexelRange>   Surfaces;
        std::vector<TSurfaceMappingTexel> Texels;
        std::vector<std::string>         Warnings;
    };
} // namespace MDSS
