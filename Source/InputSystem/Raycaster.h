/**
 * @file Raycaster.h
 * @brief Scene 정적 mesh를 대상으로 하는 CPU ray query 인터페이스를 선언한다.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace MDSS::Asset
{
    class FAssetManager;
}

namespace MDSS
{

    class FScene;

    struct FSurfaceRayHit
    {
        bool          bHit = false;
        std::size_t   InstanceIndex = 0;
        std::uint32_t TriangleID = 0;
        glm::vec3     Barycentric{0.0F};
        glm::vec3     WorldPosition{0.0F};
        glm::vec3     WorldNormal{0.0F, 0.0F, 1.0F};
        glm::vec2     SimulationUV{0.0F};
        float         Distance = 0.0F;
    };

    class FRaycaster final
    {
    public:
        /** @brief Return the nearest front-facing mesh triangle hit by a normalized world-space ray. */
        [[nodiscard]] static FSurfaceRayHit
        Cast(const FScene& Scene, const Asset::FAssetManager& Assets, glm::vec3 WorldOrigin, glm::vec3 WorldDirection);
    };
} // namespace MDSS
