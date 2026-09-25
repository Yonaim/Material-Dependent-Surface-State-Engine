/**
 * @file MaterialAsset.cpp
 * @brief 렌더링에 필요한 재질 값과 텍스처 참조.
 */

#include "AssetManager/Assets/MaterialAsset.h"

#include <utility>

namespace MDSS
{
    TMaterialAsset::TMaterialAsset(TAssetID               ID,
                                 std::string           Name,
                                 std::filesystem::path SourcePath,
                                 glm::vec4             BaseColor,
                                 TextureAssetHandle    BaseColorTexture,
                                 TextureAssetHandle    NormalTexture)
        : TAsset(ID, std::move(Name), std::move(SourcePath)), BaseColor(BaseColor), BaseColorTexture(BaseColorTexture),
          NormalTexture(NormalTexture)
    {
    }

    const glm::vec4& TMaterialAsset::GetBaseColor() const noexcept
    {
        return BaseColor;
    }

    TextureAssetHandle TMaterialAsset::GetBaseColorTexture() const noexcept
    {
        return BaseColorTexture;
    }

    TextureAssetHandle TMaterialAsset::GetNormalTexture() const noexcept
    {
        return NormalTexture;
    }
} // namespace MDSS
