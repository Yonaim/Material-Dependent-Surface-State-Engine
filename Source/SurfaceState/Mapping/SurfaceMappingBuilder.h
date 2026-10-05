/**
 * @file SurfaceMappingBuilder.h
 * @brief Mesh UV와 원본 topology를 texel graph로 변환.
 */

#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "SurfaceState/Mapping/SurfaceMappingData.h"

#include <vector>

namespace MDSS::SurfaceState
{
    class TSurfaceMappingBuilder final
    {
    public:
        /**
         * @brief 준비된 OBJ UV를 rasterize하고 chart-local 이웃과 seam 이웃을 생성한다.
         * @throws std::invalid_argument 입력 index, UV, Surface 또는 topology가 유효하지 않은 경우.
         * @throws std::runtime_error UV overlap 또는 texel당 이웃 수 제한을 위반한 경우.
         */
        [[nodiscard]] static TSurfaceMappingData Build(const std::vector<Asset::TVertex>&             Vertices,
                                                       const std::vector<Asset::TMeshTriangleSource>& Triangles,
                                                       const std::vector<TSurfaceDefinition>&         Surfaces);
    };
} // namespace MDSS::SurfaceState
