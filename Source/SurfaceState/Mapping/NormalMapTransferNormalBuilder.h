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
     * @param NormalMap repeat 주소 방식과 선형 보간으로 샘플링할 RGBA8 Normal Map.
     * @param OutTransferNormal 성공 시 정규화한 mesh-local 법선. 실패 시 기존 값을 유지한다.
     * @param bFlipNormalY 렌더 재질의 Y 방향 규약에 맞춰 tangent-space Y를 뒤집을지 지정한다.
     * @return 대응 triangle, tangent frame, 샘플이 유효하면 true를 반환한다. false이면 호출자가 기하 법선을 사용한다.
     */
    [[nodiscard]] bool BuildNormalMapTransferNormal(const FSurfaceTexelGeometry                   &Texel,
                                                    const std::vector<Asset::FVertex>             &Vertices,
                                                    const std::vector<Asset::FMeshTriangleSource> &Triangles,
                                                    const Asset::FTextureData                     &NormalMap,
                                                    glm::vec3                                     &OutTransferNormal,
                                                    bool                                           bFlipNormalY);
} // namespace MDSS::SurfaceState
