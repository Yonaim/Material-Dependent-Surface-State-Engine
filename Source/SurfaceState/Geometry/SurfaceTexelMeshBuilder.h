/**
 * @file SurfaceTexelMeshBuilder.h
 * @brief texel 기반 표시 mesh 생성에 필요한 자료형과 함수를 선언한다.
 */
#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"

#include <span>

namespace MDSS::SurfaceState
{
    // Static render vertex. Seam copies retain their own UV/normal but share displacement data.
    struct TSurfaceTexelMeshVertex
    {
        glm::vec4  Position{0};
        glm::vec4  Normal{0, 0, 1, 0};
        glm::vec4  UVSurface{0}; // xy: UV, w: Surface ID
        glm::vec4  DisplacementNormal{0, 0, 1, 0};
        glm::uvec4 Samples{InvalidTexelIndex};
        glm::vec4  Weights{0};
    };
    static_assert(sizeof(TSurfaceTexelMeshVertex) == 96);

    struct TSurfaceTexelMeshRange
    {
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
    };

    struct TSurfaceTexelMesh
    {
        std::vector<TSurfaceTexelMeshVertex> Vertices;
        std::vector<std::uint32_t>           Indices;
        // x/y: render edge endpoints, z: incident triangle's third vertex.
        std::vector<glm::uvec4>             BoundaryEdges;
        std::vector<TSurfaceTexelMeshRange> Surfaces;
    };

    /** @brief Refine source triangles with texel centers; preserve and weld source seam boundaries.
     *  Without source topology, connect the supplied same-chart grid (synthetic geometry fixtures).
     */
    [[nodiscard]] TSurfaceTexelMesh BuildSurfaceTexelMesh(const TSharedSurfaceGeometryData&           Geometry,
                                                          std::span<const Asset::TVertex>             Vertices = {},
                                                          std::span<const Asset::TMeshTriangleSource> Triangles = {});
}
