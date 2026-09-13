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
