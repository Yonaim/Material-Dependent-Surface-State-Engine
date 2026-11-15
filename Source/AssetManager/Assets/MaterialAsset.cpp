/**
 * @file MaterialAsset.cpp
 * @brief 렌더링에 필요한 재질 값과 텍스처 참조.
 */

#include "AssetManager/Assets/MaterialAsset.h"

#include <utility>

namespace MDSS::Asset
{
// ============================================================
// 00. FMaterialAsset Implementation
// ============================================================
#pragma region 00_TMaterialAsset_Implementation
// 렌더링에 필요한 재질 값과 텍스처 참조.

    FMaterialAsset::FMaterialAsset(FAssetID              ID,
                                   std::string           Name,
                                   std::filesystem::path SourcePath,
                                   glm::vec4             BaseColor,
                                   FTextureAssetHandle   BaseColorTexture,
                                   FTextureAssetHandle   NormalTexture,
                                   glm::vec4             WaterAppearance)
        : FAsset(ID, std::move(Name), std::move(SourcePath)), BaseColor(BaseColor), BaseColorTexture(BaseColorTexture),
          NormalTexture(NormalTexture), WaterAppearance(WaterAppearance)
    {
    }

    const glm::vec4& FMaterialAsset::GetBaseColor() const noexcept
    {
        return BaseColor;
    }

    FTextureAssetHandle FMaterialAsset::GetBaseColorTexture() const noexcept
    {
        return BaseColorTexture;
    }

    FTextureAssetHandle FMaterialAsset::GetNormalTexture() const noexcept
    {
        return NormalTexture;
    }

    const glm::vec4& FMaterialAsset::GetWaterAppearance() const noexcept
    {
        return WaterAppearance;
    }
#pragma endregion
} // namespace MDSS::Asset
