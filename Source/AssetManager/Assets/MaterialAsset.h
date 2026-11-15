/**
 * @file MaterialAsset.h
 * @brief 렌더링에 필요한 재질 값과 텍스처 참조.
 */

#pragma once

#include "AssetManager/Core/Asset.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace MDSS::Asset
{
    class FMaterialAsset final : public FAsset
    {
    public:
        // Material construction and data access
        FMaterialAsset(FAssetID              ID,
                       std::string           Name,
                       std::filesystem::path SourcePath,
                       glm::vec4             BaseColor,
                       FTextureAssetHandle   BaseColorTexture,
                       FTextureAssetHandle   NormalTexture,
                       glm::vec4             WaterAppearance = {0.0F, 1.0F, 0.22F, 100.0F});

        [[nodiscard]] const glm::vec4&    GetBaseColor() const noexcept;
        [[nodiscard]] FTextureAssetHandle GetBaseColorTexture() const noexcept;
        [[nodiscard]] FTextureAssetHandle GetNormalTexture() const noexcept;
        [[nodiscard]] const glm::vec4&    GetWaterAppearance() const noexcept;

    private:
        glm::vec4           BaseColor{1.0F};
        FTextureAssetHandle BaseColorTexture = InvalidAssetHandle;
        FTextureAssetHandle NormalTexture = InvalidAssetHandle;
        glm::vec4           WaterAppearance{0.0F, 1.0F, 0.22F, 100.0F};
    };
} // namespace MDSS::Asset
