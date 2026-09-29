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
    class TVulkanContext;
    class TScene;

    class TAssetManager
    {
    public:
        explicit TAssetManager(const TVulkanContext& Context);

        /** @brief OBJ와 참조된 material·texture만 로드한다. Profile Distribution은 Scene이 별도로 선택한다. */
        [[nodiscard]] TMeshAssetHandle LoadOBJ(const std::filesystem::path& Path);
        /** @brief `.SRProfile` 파일을 로드하고 Profile handle을 반환한다. */
        [[nodiscard]] TSRProfileAssetHandle LoadSRProfile(const std::filesystem::path& Path);
        /**
         * @brief 해상도별 .Surface 캐시를 로드하거나 전처리 후 저장하고 Runtime 메모리에서 공유한다.
         * @param Mesh Mesh asset handle.
         * @param DistributionPath Scene에서 지정한 `.SurfaceProfileMap` 경로.
         */
        [[nodiscard]] TSurfaceRuntimeDataHandle LoadSurfaceData(TMeshAssetHandle Mesh,
                                                                const std::filesystem::path& DistributionPath,
                                                                std::uint32_t Resolution = 0);
        [[nodiscard]] TSurfaceRuntimeDataHandle LoadSurfaceDataAtResolution(TSurfaceRuntimeDataHandle Handle,
                                                                           std::uint32_t Resolution);
        /** @brief 현재 GPU Scene에 적용된 해상도. Scene load 후보의 해상도는 명시적으로 전달한다. */
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        void SetSimulationResolution(std::uint32_t Resolution);
        /** @brief Call after replacing GPU resources; retained Scene handles remain valid. */
        void ReleaseUnusedSurfaceData(const std::vector<TSurfaceRuntimeDataHandle>& RetainedHandles);

        /** @throws std::out_of_range Handle이 현재 등록된 Mesh 범위를 벗어난 경우. */
        [[nodiscard]] const TMeshAsset& GetMesh(TMeshAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Material 범위를 벗어난 경우. */
        [[nodiscard]] const TMaterialAsset& GetMaterial(TMaterialAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Texture 범위를 벗어난 경우. */
        [[nodiscard]] const TextureAsset& GetTexture(TextureAssetHandle Handle) const;
        /** @throws std::out_of_range Handle이 현재 등록된 Profile 범위를 벗어난 경우. */
        [[nodiscard]] const TSRProfileAsset& GetSRProfile(TSRProfileAssetHandle Handle) const;
        [[nodiscard]] bool                  HasSurfaceData(TSurfaceRuntimeDataHandle Handle) const noexcept;
        [[nodiscard]] const TSurfaceRuntimeData& GetSurfaceData(TSurfaceRuntimeDataHandle Handle) const;
        /** @brief Profile table order used by this Runtime Surface Data's per-texel Profile indices. */
        [[nodiscard]] const std::vector<TSRProfileAssetHandle>&
        GetSurfaceProfileTable(TSurfaceRuntimeDataHandle Handle) const;
        /** @brief Unique, sorted Profile handles referenced by the Scene's Runtime Surface tables. */
        [[nodiscard]] std::vector<TSRProfileAssetHandle> GetSceneSurfaceProfiles(const TScene& Scene) const;
        /** @brief Build a Registry from this Scene only; loading cached assets does not change the active Registry. */
        [[nodiscard]] TSurfaceStateRegistry BuildSurfaceStateRegistry(const TScene& Scene) const;
        /** @brief Install a Scene Registry and return the previous one for resource-reload rollback. */
        TSurfaceStateRegistry ExchangeSurfaceStateRegistry(TSurfaceStateRegistry Registry) noexcept;
        [[nodiscard]] const TSurfaceStateRegistry& GetSurfaceStateRegistry() const noexcept;

        [[nodiscard]] std::size_t          GetMaterialCount() const noexcept;
        [[nodiscard]] std::size_t          GetSRProfileCount() const noexcept;
        [[nodiscard]] TMaterialAssetHandle GetDefaultMaterialHandle() const noexcept;

    private:
        TextureAssetHandle  LoadTexture(const std::filesystem::path& Path, bool bSRGB);
        TextureAssetHandle  CreateSolidTexture(std::string Name, const std::vector<std::uint8_t>& RGBA, bool bSRGB);
        TMaterialAssetHandle CreateMaterial(std::string                  Name,
                                           const std::filesystem::path& SourcePath,
                                           glm::vec4                    BaseColor,
                                           TextureAssetHandle           BaseColorTexture,
                                           TextureAssetHandle           NormalTexture);

        const TVulkanContext& Context;

        struct TRuntimeSurfaceAsset
        {
            std::shared_ptr<const TSurfaceRuntimeData> Data;
            std::vector<TSRProfileAssetHandle> ProfileTable;
            TMeshAssetHandle Mesh = InvalidAssetHandle;
            std::filesystem::path DistributionPath;
        };

        std::vector<std::unique_ptr<TMeshAsset>>                Meshes;
        std::vector<std::unique_ptr<TMaterialAsset>>            Materials;
        std::vector<std::unique_ptr<TSRProfileAsset>>           SRProfiles;
        std::vector<std::unique_ptr<TextureAsset>>             Textures;
        std::unordered_map<std::string, TMeshAssetHandle>       MeshAssetsByPath;
        std::unordered_map<std::string, TextureAssetHandle>    TextureCache;
        std::unordered_map<std::string, TSRProfileAssetHandle>  SRProfileCache;
        std::vector<TRuntimeSurfaceAsset>                       RuntimeSurfaceAssets;
        std::unordered_map<std::string, TSurfaceRuntimeDataHandle> RuntimeSurfaceAssetsByInputs;
        TSurfaceStateRegistry StateRegistry{std::vector<TSurfaceResponseProfileData>{}};
        std::uint32_t SimulationResolution = SurfaceSimulationResolution;

        TextureAssetHandle  DefaultBaseColorTexture = InvalidAssetHandle;
        TextureAssetHandle  DefaultNormalTexture = InvalidAssetHandle;
        TMaterialAssetHandle DefaultMaterial = InvalidAssetHandle;
    };
} // namespace MDSS
