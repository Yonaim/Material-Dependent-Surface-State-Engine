/**
 * @file Raycaster.h
 * @brief CPU ray queries against static mesh instances in a scene.
 */

#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

namespace MDSS
{
    class TAssetManager;
    class TScene;

    struct TSurfaceRayHit
    {
        bool         Hit = false;
        std::size_t  InstanceIndex = 0;
        std::uint32_t TriangleID = 0;
        glm::vec3    Barycentric{0.0F};
        glm::vec3    WorldPosition{0.0F};
        glm::vec2    SimulationUV{0.0F};
        float        Distance = 0.0F;
    };

    class TRaycaster final
    {
    public:
        /** @brief Return the nearest front-facing mesh triangle hit by a normalized world-space ray. */
        [[nodiscard]] static TSurfaceRayHit Cast(const TScene&       Scene,
                                                 const TAssetManager& Assets,
                                                 glm::vec3           WorldOrigin,
                                                 glm::vec3           WorldDirection);
    };
} // namespace MDSS
