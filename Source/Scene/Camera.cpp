/**
 * @file Camera.cpp
 * @brief 카메라 view·projection 행렬과 시야 설정.
 */

#include "Scene/Camera.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <stdexcept>

namespace MDSS
{
// ============================================================
// 00. Camera Matrices
// ============================================================
#pragma region 00_Camera_Matrices

    FCamera::FCamera(
        glm::vec3 Position, glm::vec3 Target, float VerticalFieldOfViewDegrees, float NearPlane, float FarPlane)
        : Position(Position), Target(Target), VerticalFieldOfViewDegrees(VerticalFieldOfViewDegrees),
          NearPlane(NearPlane), FarPlane(FarPlane)
    {
        if (VerticalFieldOfViewDegrees <= 0.0F || VerticalFieldOfViewDegrees >= 180.0F)
        {
            throw std::invalid_argument("TCamera field of view must be between 0 and 180 degrees.");
        }

        if (NearPlane <= 0.0F || FarPlane <= NearPlane)
        {
            throw std::invalid_argument("TCamera clipping planes are invalid.");
        }
    }

    glm::mat4 FCamera::GetViewMatrix() const
    {
        return glm::lookAtRH(Position, Target, Up);
    }

    glm::mat4 FCamera::GetProjectionMatrix(float AspectRatio) const
    {
        if (AspectRatio <= 0.0F)
        {
            throw std::invalid_argument("TCamera aspect ratio must be positive.");
        }

        glm::mat4 Projection =
            glm::perspectiveRH_ZO(glm::radians(VerticalFieldOfViewDegrees), AspectRatio, NearPlane, FarPlane);

        // GLM's clip-space convention is Y-up. Vulkan framebuffers use the opposite Y direction.
        Projection[1][1] *= -1.0F;
        return Projection;
    }

    glm::mat4 FCamera::GetViewProjectionMatrix(float AspectRatio) const
    {
        return GetProjectionMatrix(AspectRatio) * GetViewMatrix();
    }

#pragma endregion

// ============================================================
// 01. Camera Controls
// ============================================================
#pragma region 01_Camera_Controls

    void FCamera::SetPosition(glm::vec3 NewPosition) noexcept
    {
        Position = NewPosition;
    }

    void FCamera::SetTarget(glm::vec3 NewTarget) noexcept
    {
        Target = NewTarget;
    }

    void FCamera::SetRotationDegrees(glm::vec2 RotationDegrees) noexcept
    {
        const glm::vec3 ViewDirection = Target - Position;
        const float     Distance = std::max(glm::length(ViewDirection), 1.0F);

        const float PitchDegrees = std::clamp(RotationDegrees.x, -89.0F, 89.0F);
        const float YawDegrees = RotationDegrees.y;

        const float Pitch = glm::radians(PitchDegrees);
        const float Yaw = glm::radians(YawDegrees);

        const glm::vec3 Forward{std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch)};
        Target = Position + glm::normalize(Forward) * Distance;
    }

    void FCamera::SetVerticalFieldOfViewDegrees(float FieldOfViewDegrees) noexcept
    {
        VerticalFieldOfViewDegrees = std::clamp(FieldOfViewDegrees, 1.0F, 179.0F);
    }

#pragma endregion

// ============================================================
// 02. Camera State Access
// ============================================================
#pragma region 02_Camera_State_Access

    const glm::vec3& FCamera::GetPosition() const noexcept
    {
        return Position;
    }

    const glm::vec3& FCamera::GetTarget() const noexcept
    {
        return Target;
    }

    glm::vec2 FCamera::GetRotationDegrees() const noexcept
    {
        const glm::vec3 Direction = glm::normalize(Target - Position);
        const float     Pitch = std::asin(std::clamp(Direction.z, -1.0F, 1.0F));
        const float     Yaw = std::atan2(Direction.y, Direction.x);
        return {glm::degrees(Pitch), glm::degrees(Yaw)};
    }

    float FCamera::GetVerticalFieldOfViewDegrees() const noexcept
    {
        return VerticalFieldOfViewDegrees;
    }
#pragma endregion
} // namespace MDSS
