/**
 * @file LiquidMaterial.h
 * @brief 액체층의 수송 물성과 셀 내부 갱신 규칙을 지정한다.
 */

#pragma once

#include <cstdint>

namespace MDSS::Liquid
{
    inline constexpr float IceDensity = 917.F;
    enum class ELocalMaterialRule : std::uint32_t
    {
        None,
        Water
    };

    /** @brief 계산 격자 전체에서 공유하는 단일 액체층의 물성이다. 밀도와 점도는 양수다. */
    struct FLiquidMaterial
    {
        bool operator==(const FLiquidMaterial &) const = default;

        float              DensityKgM3;
        float              DynamicViscosityPaS;
        ELocalMaterialRule LocalRule = ELocalMaterialRule::None;
        float              IceDensityKgM3 = IceDensity;

        // 외관 값은 그래픽 버퍼로 전달하며 수송·상변화 연산에 쓰이지 않는다.
        float RefractiveIndex = 1.333F;
        float Roughness = 0.24F;
        float IceRefractiveIndex = 1.31F;
        float IceRoughness = 0.13F;
        float FrostRoughness = 0.44F;
        bool  bIncandescent = false;

        float YieldStressPa = 0.F;
        float Friction = 0.F; // 안식각의 탄젠트, 무차원.
    };
} // namespace MDSS::Liquid
