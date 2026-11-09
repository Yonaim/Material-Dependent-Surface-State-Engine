/**
 * @file AssetManager.h
 * @brief 일반 에셋 로딩과 조회.
 */

#pragma once

#include "AssetManager/Assets/MaterialAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Core/Asset.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS::Asset
{

    class FAssetManager
    {
    public:
        // Asset loading
        explicit FAssetManager(const GPU::FVulkanContext& Context);

        /** @brief OBJ와 참조된 material·texture만 로드한다. Profile Distribution은 Scene이 별도로 선택한다. */
        [[nodiscard]] FMeshAssetHandle LoadOBJ(const std::filesystem::path& Path);
        /** @brief `.SRProfile` 파일을 로드하고 Profile handle을 반환한다. */
        [[nodiscard]] FSRProfileAssetHandle LoadSRProfile(const std::filesystem::path& Path);
        // Loaded asset access
        /** @throws std::out_of_range Handle이 현재 등록된 Mesh 범위를 벗어난 경우. */
        [[nodiscard]] const FMeshAsset& GetMesh(FMeshAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Material 범위를 벗어난 경우. */
        [[nodiscard]] const FMaterialAsset& GetMaterial(FMaterialAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Texture 범위를 벗어난 경우. */
        [[nodiscard]] const FTextureAsset& GetTexture(FTextureAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Profile 범위를 벗어난 경우. */
        [[nodiscard]] const FSRProfileAsset& GetSRProfile(FSRProfileAssetHandle Handle) const;

        [[nodiscard]] std::size_t          GetMaterialCount() const noexcept;
        [[nodiscard]] std::size_t          GetSRProfileCount() const noexcept;
        [[nodiscard]] FMaterialAssetHandle GetDefaultMaterialHandle() const noexcept;

    private:
        // Generated textures and materials
        FTextureAssetHandle  LoadTexture(const std::filesystem::path& Path, bool bSRGB);
        FTextureAssetHandle  CreateSolidTexture(std::string Name, const std::vector<std::uint8_t>& RGBA, bool bSRGB);
        FMaterialAssetHandle CreateMaterial(std::string                  Name,
                                            const std::filesystem::path& SourcePath,
                                            glm::vec4                    BaseColor,
                                            FTextureAssetHandle          BaseColorTexture,
                                            FTextureAssetHandle          NormalTexture,
                                            glm::vec4                    WaterAppearance = {0.0F, 1.0F, 0.22F, 100.0F});

        const GPU::FVulkanContext& Context;

        std::vector<std::unique_ptr<FMeshAsset>>               Meshes;
        std::vector<std::unique_ptr<FMaterialAsset>>           Materials;
        std::vector<std::unique_ptr<FSRProfileAsset>>          SRProfiles;
        std::vector<std::unique_ptr<FTextureAsset>>            Textures;
        std::unordered_map<std::string, FMeshAssetHandle>      MeshAssetsByPath;
        std::unordered_map<std::string, FTextureAssetHandle>   TextureCache;
        std::unordered_map<std::string, FSRProfileAssetHandle> SRProfileCache;

        FTextureAssetHandle  DefaultBaseColorTexture = InvalidAssetHandle;
        FTextureAssetHandle  DefaultNormalTexture = InvalidAssetHandle;
        FMaterialAssetHandle DefaultMaterial = InvalidAssetHandle;
    };
} // namespace MDSS::Asset
