#pragma once

#include <glm/glm.hpp>

namespace MDSS
{
    class Camera
    {
    public:
        Camera() = default;
        Camera(glm::vec3 Position,
               glm::vec3 Target,
               float     VerticalFieldOfViewDegrees = 60.0F,
               float     NearPlane = 0.1F,
               float     FarPlane = 100.0F);

        [[nodiscard]] glm::mat4 GetViewMatrix() const;
        [[nodiscard]] glm::mat4 GetProjectionMatrix(float AspectRatio) const;
        [[nodiscard]] glm::mat4 GetViewProjectionMatrix(float AspectRatio) const;

        void SetPosition(glm::vec3 Position) noexcept;
        void SetTarget(glm::vec3 Target) noexcept;

        [[nodiscard]] const glm::vec3& GetPosition() const noexcept;
        [[nodiscard]] const glm::vec3& GetTarget() const noexcept;

    private:
        glm::vec3 Position{0.0F, 0.0F, 3.0F};
        glm::vec3 Target{0.0F, 0.0F, 0.0F};
        glm::vec3 Up{0.0F, 1.0F, 0.0F};

        float VerticalFieldOfViewDegrees = 60.0F;
        float NearPlane = 0.1F;
        float FarPlane = 100.0F;
    };
} // namespace MDSS
