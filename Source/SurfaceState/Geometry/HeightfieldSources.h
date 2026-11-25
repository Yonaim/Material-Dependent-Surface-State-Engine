/**
 * @file HeightfieldSources.h
 * @brief 메시와 맵을 높이 필드로 변환하고 공간 대역을 분리하는 API를 정의한다.
 */

#pragma once

#include "SurfaceState/Geometry/Heightfield.h"
#include <filesystem>
#include <glm/vec2.hpp>

namespace MDSS::SurfaceState
{
    inline constexpr int MaxNormalIntegrationAxis = 2048;
    enum class EHeightfieldSource
    {
        Procedural,
        HeightMap,
        DisplacementMap,
        NormalMap,
        Mesh
    };

    struct FHeightfieldBands
    {
        Liquid::FHeightfield Base;
        Liquid::FHeightfield Meso;
        Liquid::FHeightfield Micro;
    };

    /**
     * @brief 메시 또는 맵 입력을 동일한 로컬 XY 차트의 샘플 높이로 변환한다.
     * @param Width 차트의 X 방향 길이(m).
     * @param Length 차트의 Y 방향 길이(m).
     * @param Scale 메시와 높이 맵의 높이 배율 또는 노멀 적분의 기울기 배율.
     * @param IntegrationSamples 노멀 적분의 가로·세로 샘플 수. 0, 0이면 입력 텍스처 크기를 사용한다.
     * @throws std::invalid_argument 차트 크기 또는 배율이 유효하지 않거나 Source가 Procedural인 경우.
     */
    [[nodiscard]] Liquid::FHeightfield LoadHeightfield(EHeightfieldSource           Source,
                                                       const std::filesystem::path &Path,
                                                       int                          Columns,
                                                       int                          Rows,
                                                       float                        Width,
                                                       float                        Length,
                                                       float                        Scale,
                                                       glm::ivec2                   IntegrationSamples = {0, 0});
    /**
     * @brief Base, Meso, Micro를 샘플별로 합하면 입력 높이를 복원한다.
     * @pre 필터 반복 횟수는 0 ≤ MesoIterations ≤ BaseIterations ≤ 1024다.
     */
    [[nodiscard]] FHeightfieldBands
    SplitHeightfield(const Liquid::FHeightfield &Shape, int BaseIterations, int MesoIterations);
} // namespace MDSS::SurfaceState
