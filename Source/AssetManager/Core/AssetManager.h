/**
 * @file AssetManager.h
 * @brief 에셋 로딩, Runtime 데이터 소유권 관리와 조회.
 */

#pragma once

#include "AssetManager/Assets/MaterialAsset.h"
#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Core/Asset.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"

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

        /** @brief OBJ와 참조된 material·texture를 로드하고 Mesh handle을 반환한다. */
        [[nodiscard]] MeshAssetHandle LoadOBJ(const std::filesystem::path& Path);
        /** @brief `.SRProfile` 파일을 로드하고 Profile handle을 반환한다. */
        [[nodiscard]] SRProfileAssetHandle LoadSRProfile(const std::filesystem::path& Path);
        /**
         * @brief Profile Distribution을 읽고 Mesh의 Surface mapping/geometry를 Runtime 메모리에 생성한다.
         * @param Mesh Mesh asset handle.
         * @param DistributionPath Sidecar 경로. 비어 있으면 Mesh와 같은 stem의 `.SurfaceProfileMap`을 사용한다.
         */
        void LoadSurfaceData(MeshAssetHandle Mesh, const std::filesystem::path& DistributionPath = {});

        /** @throws std::out_of_range Handle이 현재 등록된 Mesh 범위를 벗어난 경우. */
        [[nodiscard]] const MeshAsset& GetMesh(MeshAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Material 범위를 벗어난 경우. */
        [[nodiscard]] const MaterialAsset& GetMaterial(MaterialAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Texture 범위를 벗어난 경우. */
        [[nodiscard]] const TextureAsset& GetTexture(TextureAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Profile 범위를 벗어난 경우. */
        [[nodiscard]] const SRProfileAsset&     GetSRProfile(SRProfileAssetHandle Handle) const;
        [[nodiscard]] bool                      HasSurfaceData(MeshAssetHandle Handle) const noexcept;
        [[nodiscard]] const SurfaceRuntimeData& GetSurfaceData(MeshAssetHandle Handle) const;
        /** @brief Profile table order used by the Mesh's per-texel SurfaceProfileMap indices. */
        [[nodiscard]] const std::vector<SRProfileAssetHandle>& GetSurfaceProfileTable(MeshAssetHandle Handle) const;
        /** @brief Build/cache the deterministic State registry from all currently loaded Profiles. */
        [[nodiscard]] const SurfaceStateRegistry& GetSurfaceStateRegistry() const;

        [[nodiscard]] std::size_t         GetMaterialCount() const noexcept;
        [[nodiscard]] std::size_t         GetSRProfileCount() const noexcept;
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

        std::vector<std::unique_ptr<MeshAsset>>                Meshes;
        std::vector<std::unique_ptr<MaterialAsset>>            Materials;
        std::vector<std::unique_ptr<SRProfileAsset>>           SRProfiles;
        std::vector<std::unique_ptr<TextureAsset>>             Textures;
        std::unordered_map<std::string, MeshAssetHandle>       MeshAssetsByPath;
        std::unordered_map<std::string, TextureAssetHandle>    TextureCache;
        std::unordered_map<std::string, SRProfileAssetHandle>  SRProfileCache;
        std::vector<std::shared_ptr<const SurfaceRuntimeData>> SurfaceData;
        std::vector<std::vector<SRProfileAssetHandle>>         SurfaceProfileTables;
        mutable std::unique_ptr<SurfaceStateRegistry>          StateRegistry;

        TextureAssetHandle  DefaultBaseColorTexture = InvalidAssetHandle;
        TextureAssetHandle  DefaultNormalTexture = InvalidAssetHandle;
        MaterialAssetHandle DefaultMaterial = InvalidAssetHandle;
    };
} // namespace MDSS
