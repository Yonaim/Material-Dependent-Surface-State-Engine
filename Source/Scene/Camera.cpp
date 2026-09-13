#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>

namespace MDSS
{
    Camera::Camera(
        glm::vec3 Position, glm::vec3 Target, float VerticalFieldOfViewDegrees, float NearPlane, float FarPlane)
        : Position(Position), Target(Target), VerticalFieldOfViewDegrees(VerticalFieldOfViewDegrees),
          NearPlane(NearPlane), FarPlane(FarPlane)
    {
        if (VerticalFieldOfViewDegrees <= 0.0F || VerticalFieldOfViewDegrees >= 180.0F)
        {
            throw std::invalid_argument("Camera field of view must be between 0 and 180 degrees.");
        }

        if (NearPlane <= 0.0F || FarPlane <= NearPlane)
        {
            throw std::invalid_argument("Camera clipping planes are invalid.");
        }
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        return glm::lookAtRH(Position, Target, Up);
    }

    glm::mat4 Camera::GetProjectionMatrix(float AspectRatio) const
    {
        if (AspectRatio <= 0.0F)
        {
            throw std::invalid_argument("Camera aspect ratio must be positive.");
        }

        glm::mat4 Projection =
            glm::perspectiveRH_ZO(glm::radians(VerticalFieldOfViewDegrees), AspectRatio, NearPlane, FarPlane);

        // GLM's clip-space convention is Y-up. Vulkan framebuffers use the opposite Y direction.
        Projection[1][1] *= -1.0F;
        return Projection;
    }

    glm::mat4 Camera::GetViewProjectionMatrix(float AspectRatio) const
    {
        return GetProjectionMatrix(AspectRatio) * GetViewMatrix();
    }

    void Camera::SetPosition(glm::vec3 NewPosition) noexcept
    {
        Position = NewPosition;
    }

    void Camera::SetTarget(glm::vec3 NewTarget) noexcept
    {
        Target = NewTarget;
    }

    void Camera::SetRotationDegrees(glm::vec2 RotationDegrees) noexcept
    {
        const float PitchDegrees = std::clamp(RotationDegrees.x, -89.0F, 89.0F);
        const float YawDegrees = RotationDegrees.y;

        const float Pitch = glm::radians(PitchDegrees);
        const float Yaw = glm::radians(YawDegrees);

        const glm::vec3 Forward{std::cos(Pitch) * std::cos(Yaw), std::sin(Pitch), std::cos(Pitch) * std::sin(Yaw)};
        Target = Position + glm::normalize(Forward);
    }

    void Camera::SetVerticalFieldOfViewDegrees(float FieldOfViewDegrees) noexcept
    {
        VerticalFieldOfViewDegrees = std::clamp(FieldOfViewDegrees, 1.0F, 179.0F);
    }

    const glm::vec3& Camera::GetPosition() const noexcept
    {
        return Position;
    }

    const glm::vec3& Camera::GetTarget() const noexcept
    {
        return Target;
    }

    glm::vec2 Camera::GetRotationDegrees() const noexcept
    {
        const glm::vec3 Direction = glm::normalize(Target - Position);
        const float     Pitch = std::asin(std::clamp(Direction.y, -1.0F, 1.0F));
        const float     Yaw = std::atan2(Direction.z, Direction.x);
        return {glm::degrees(Pitch), glm::degrees(Yaw)};
    }

    float Camera::GetVerticalFieldOfViewDegrees() const noexcept
    {
        return VerticalFieldOfViewDegrees;
    }
} // namespace MDSS
