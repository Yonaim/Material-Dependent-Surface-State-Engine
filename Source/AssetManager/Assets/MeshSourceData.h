/**
 * @file MeshSourceData.h
 * @brief Rendering과 Surface mapping이 공유하는 CPU mesh 입력 자료형.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace MDSS
{
    struct Vertex
    {
        glm::vec3 Position{0.0F};
        glm::vec3 Normal{0.0F, 1.0F, 0.0F};
        glm::vec2 UV{0.0F};
        glm::vec4 Tangent{1.0F, 0.0F, 0.0F, 1.0F};
    };

    /** @brief Render vertex와 OBJ 원본 position topology를 함께 보존한 triangle. */
    struct MeshTriangleSource
    {
        std::array<std::uint32_t, 3> RenderVertexIndices{};
        std::array<std::int32_t, 3>  OriginalPositionIndices{};
        std::array<std::int32_t, 3>  OriginalUVIndices{};
        SurfaceLocalID               Surface = InvalidSurfaceID;
    };
} // namespace MDSS
