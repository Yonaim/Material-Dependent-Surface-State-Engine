/**
 * @file NormalMapTransferNormalBuilder.h
 * @brief Simulation texel용 Normal Map sample을 mesh-local transfer normal로 변환.
 */

#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "SurfaceState/Types/SurfaceMappingTypes.h"

#include <vector>

namespace MDSS::SurfaceState
{
    /**
     * @brief Texel mapping의 triangle/barycentric 정보로 Normal Map normal을 샘플링하고 mesh-local로 변환한다.
     * @param bFlipNormalY 렌더 재질의 기본 Normal Map Y convention과 동일하게 적용한다.
     * @return sample과 tangent frame이 유효하면 true. false이면 호출자가 geometric normal fallback을 사용한다.
     */
    [[nodiscard]] bool BuildNormalMapTransferNormal(const TSurfaceTexelGeometry&            Texel,
                                                    const std::vector<Asset::TVertex>&             Vertices,
                                                    const std::vector<Asset::TMeshTriangleSource>& Triangles,
                                                    const Asset::TextureData&                      NormalMap,
                                                    glm::vec3&                              OutTransferNormal,
                                                    bool                                    bFlipNormalY);
} // namespace MDSS::SurfaceState
