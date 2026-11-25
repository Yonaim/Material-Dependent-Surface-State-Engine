/**
 * @file SurfaceAppearance.h
 * @brief 표면 렌더링에 사용하는 바탕색과 텍스처 매핑 입력을 정의한다.
 */

#pragma once

#include <filesystem>
#include <glm/vec3.hpp>

namespace MDSS::Rendering
{
    inline constexpr float MaxSubstrateSpecularScale = 4.F;

    /** @brief 유전체 바닥의 기준 거칠기, 굴절률과 렌더링용 정면 반사 배율이다. */
    struct FSubstrateOptics
    {
        bool operator==(const FSubstrateOptics &) const = default;

        float DryRoughness = 0.8F;
        float WetRoughness = 0.33F;
        float RefractiveIndex = 1.46F;
        float SpecularScale = 1.F;
        float NormalStrength = 1.F;
        /** @brief 노멀 텍스처 알파 최고점과 최저점 사이의 표시 깊이 [m]. */
        float ReliefDepthMeters = 0.F;
    };

    /** @brief 표면 외관의 기본색과 실제 길이에 따른 albedo/roughness 텍스처 매핑을 지정한다. */
    struct FSurfaceAppearance
    {
        glm::vec3             BaseColor;
        std::filesystem::path TexturePath;
        float                 TextureRepeatsPerMeter = 0.F;
        FSubstrateOptics      Optics;
        std::filesystem::path NormalTexturePath;
    };
} // namespace MDSS::Rendering
