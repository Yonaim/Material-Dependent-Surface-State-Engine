/**
 * @file NormalMapTransferNormalBuilder.h
 * @brief Simulation texel용 Normal Map sample을 mesh-local transfer normal로 변환.
 */

#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"

#include <vector>

namespace MDSS
{
    /**
     * @brief Texel mapping의 triangle/barycentric 정보로 Normal Map normal을 샘플링하고 mesh-local로 변환한다.
     * @return sample과 tangent frame이 유효하면 true. false이면 호출자가 geometric normal fallback을 사용한다.
     */
    [[nodiscard]] bool BuildNormalMapTransferNormal(const TSurfaceTexelGeometry&            Texel,
                                                    const std::vector<TVertex>&             Vertices,
                                                    const std::vector<TMeshTriangleSource>& Triangles,
                                                    const TextureData&                      NormalMap,
                                                    glm::vec3&                              OutTransferNormal);
} // namespace MDSS
