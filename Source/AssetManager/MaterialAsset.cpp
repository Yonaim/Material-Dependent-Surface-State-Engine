/**
 * @file MaterialAsset.cpp
 * @brief 렌더링에 필요한 재질 값과 텍스처 참조.
 */

#include "AssetManager/MaterialAsset.h"

#include <utility>

namespace MDSS
{
    MaterialAsset::MaterialAsset(AssetID               ID,
                                 std::string           Name,
                                 std::filesystem::path SourcePath,
                                 glm::vec4             BaseColor,
                                 TextureAssetHandle    BaseColorTexture,
                                 TextureAssetHandle    NormalTexture)
        : Asset(ID, std::move(Name), std::move(SourcePath)), BaseColor(BaseColor), BaseColorTexture(BaseColorTexture),
          NormalTexture(NormalTexture)
    {
    }

    const glm::vec4& MaterialAsset::GetBaseColor() const noexcept
    {
        return BaseColor;
    }

    TextureAssetHandle MaterialAsset::GetBaseColorTexture() const noexcept
    {
        return BaseColorTexture;
    }

    TextureAssetHandle MaterialAsset::GetNormalTexture() const noexcept
    {
        return NormalTexture;
    }
} // namespace MDSS
