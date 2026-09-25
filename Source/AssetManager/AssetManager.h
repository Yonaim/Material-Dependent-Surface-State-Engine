/**
 * @file AssetManager.h
 * @brief 에셋 로딩, 소유권 관리와 캐시 조회.
 */

#pragma once

#include "AssetManager/Asset.h"
#include "AssetManager/MaterialAsset.h"
#include "AssetManager/MeshAsset.h"
#include "AssetManager/TextureAsset.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    class VulkanContext;

    class AssetManager
    {
    public:
        explicit AssetManager(const VulkanContext& Context);

        [[nodiscard]] MeshAssetHandle LoadOBJ(const std::filesystem::path& Path);
        /** @brief OBJ와 참조된 material·texture를 로드하고 Mesh handle을 반환한다. */

        [[nodiscard]] const MeshAsset&     GetMesh(MeshAssetHandle Handle) const;
        [[nodiscard]] const MaterialAsset& GetMaterial(MaterialAssetHandle Handle) const;
        [[nodiscard]] const TextureAsset&  GetTexture(TextureAssetHandle Handle) const;

        [[nodiscard]] std::size_t         GetMaterialCount() const noexcept;
        [[nodiscard]] MaterialAssetHandle GetDefaultMaterialHandle() const noexcept;

    private:
        TextureAssetHandle  LoadTexture(const std::filesystem::path& Path, bool bSRGB);
        TextureAssetHandle  CreateSolidTexture(std::string Name, const std::vector<std::uint8_t>& RGBA, bool bSRGB);
        MaterialAssetHandle CreateMaterial(std::string                  Name,
                                           const std::filesystem::path& SourcePath,
                                           glm::vec4                    BaseColor,
                                           TextureAssetHandle           BaseColorTexture,
                                           TextureAssetHandle           NormalTexture);

        const VulkanContext& Context;

        std::vector<std::unique_ptr<MeshAsset>>             Meshes;
        std::vector<std::unique_ptr<MaterialAsset>>         Materials;
        std::vector<std::unique_ptr<TextureAsset>>          Textures;
        std::unordered_map<std::string, TextureAssetHandle> TextureCache;

        TextureAssetHandle  DefaultBaseColorTexture = InvalidAssetHandle;
        TextureAssetHandle  DefaultNormalTexture = InvalidAssetHandle;
        MaterialAssetHandle DefaultMaterial = InvalidAssetHandle;
    };
} // namespace MDSS
