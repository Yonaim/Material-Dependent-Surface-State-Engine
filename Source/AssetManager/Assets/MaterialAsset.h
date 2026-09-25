/**
 * @file MaterialAsset.h
 * @brief 렌더링에 필요한 재질 값과 텍스처 참조.
 */

#pragma once

#include "AssetManager/Core/Asset.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace MDSS
{
    class MaterialAsset final : public Asset
    {
    public:
        MaterialAsset(AssetID               ID,
                      std::string           Name,
                      std::filesystem::path SourcePath,
                      glm::vec4             BaseColor,
                      TextureAssetHandle    BaseColorTexture,
                      TextureAssetHandle    NormalTexture);

        [[nodiscard]] const glm::vec4&   GetBaseColor() const noexcept;
        [[nodiscard]] TextureAssetHandle GetBaseColorTexture() const noexcept;
        [[nodiscard]] TextureAssetHandle GetNormalTexture() const noexcept;

    private:
        glm::vec4          BaseColor{1.0F};
        TextureAssetHandle BaseColorTexture = InvalidAssetHandle;
        TextureAssetHandle NormalTexture = InvalidAssetHandle;
    };
} // namespace MDSS
