#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>

namespace MDSS::Rendering
{
    // Vulkan's zero-to-one clip depth: near is row 2, far is row 3 minus row 2.
    struct TViewFrustum
    {
        std::array<glm::vec4, 6> Planes{};

        explicit TViewFrustum(const glm::mat4& ViewProjection)
        {
            const glm::mat4 Rows = glm::transpose(ViewProjection);
            Planes = {
                Rows[3] + Rows[0], Rows[3] - Rows[0], Rows[3] + Rows[1], Rows[3] - Rows[1], Rows[2], Rows[3] - Rows[2]};
            for (glm::vec4& Plane : Planes)
            {
                const float Length = glm::length(glm::vec3(Plane));
                if (std::isfinite(Length) && Length > 0.0F)
                    Plane /= Length;
                else
                    Plane = glm::vec4(0.0F); // Invalid input must not hide geometry.
            }
        }

        [[nodiscard]] bool ContainsSphere(const glm::vec3& Center, float Radius) const noexcept
        {
            if (!std::isfinite(Center.x) || !std::isfinite(Center.y) || !std::isfinite(Center.z) ||
                !std::isfinite(Radius) || Radius < 0.0F)
                return true;
            for (const glm::vec4& Plane : Planes)
                if (glm::dot(glm::vec3(Plane), Center) + Plane.w < -Radius)
                    return false;
            return true;
        }
    };

    [[nodiscard]] inline bool IsMeshSphereVisible(const TViewFrustum& Frustum,
                                                  const glm::vec3&    LocalCenter,
                                                  float               LocalRadius,
                                                  const glm::mat4&    Model) noexcept
    {
        const glm::vec3 Center = glm::vec3(Model * glm::vec4(LocalCenter, 1.0F));
        // TTransform composes translation, rotation and scale; its columns are orthogonal.
        const float Scale = std::max(
            {glm::length(glm::vec3(Model[0])), glm::length(glm::vec3(Model[1])), glm::length(glm::vec3(Model[2]))});
        return Frustum.ContainsSphere(Center, LocalRadius * Scale);
    }
}
