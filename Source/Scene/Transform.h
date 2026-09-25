/**
 * @file Transform.h
 * @brief 위치·회전·크기와 model 행렬 계산.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MDSS
{
    struct Transform
    {
        glm::vec3 Position{0.0F, 0.0F, 0.0F};
        glm::vec3 RotationDegrees{0.0F, 0.0F, 0.0F};
        glm::vec3 Scale{1.0F, 1.0F, 1.0F};

        [[nodiscard]] glm::mat4 GetMatrix() const
        {
            glm::mat4 Matrix(1.0F);
            Matrix = glm::translate(Matrix, Position);
            Matrix = glm::rotate(Matrix, glm::radians(RotationDegrees.x), glm::vec3(1.0F, 0.0F, 0.0F));
            Matrix = glm::rotate(Matrix, glm::radians(RotationDegrees.y), glm::vec3(0.0F, 1.0F, 0.0F));
            Matrix = glm::rotate(Matrix, glm::radians(RotationDegrees.z), glm::vec3(0.0F, 0.0F, 1.0F));
            Matrix = glm::scale(Matrix, Scale);
            return Matrix;
        }
    };
} // namespace MDSS
