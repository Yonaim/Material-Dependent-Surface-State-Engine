/**
 * @file LiquidMaterialLoader.h
 * @brief Surface Simulation의 액체 재질 JSON을 읽는다.
 */

#pragma once

#include "SurfaceState/Liquid/LiquidMaterial.h"
#include <filesystem>
#include <string_view>

namespace MDSS::Asset
{
    /**
     * @brief 밀도, 점도와 국소 재질 규칙을 읽고 검증한다.
     * @param Path 비어 있으면 기본 Water.PhysicalMaterial을 읽는다. 기존 CLI JSON도 허용한다.
     * @throws std::runtime_error 파일에 접근할 수 없거나 물성 또는 규칙이 유효하지 않은 경우.
     */
    [[nodiscard]] Liquid::FLiquidMaterial LoadLiquidMaterial(const std::filesystem::path &Path);

    /** @brief 재질 ID의 공용 .PhysicalMaterial 정의를 읽고 액체 물성 및 선택적 얼음 밀도를 얻는다. */
    [[nodiscard]] Liquid::FLiquidMaterial LoadLiquidMaterialById(std::string_view Id);
} // namespace MDSS::Asset
