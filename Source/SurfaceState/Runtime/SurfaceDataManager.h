/**
 * @file SurfaceDataManager.h
 * @brief Scene에서 사용하는 Surface 전처리 데이터와 Registry를 소유한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    class FScene;
}

namespace MDSS::Asset
{
    class FAssetManager;
}

namespace MDSS::SurfaceState
{
    class FSurfaceDataManager final
    {
    public:
        // Runtime surface data loading
        explicit FSurfaceDataManager(Asset::FAssetManager& Assets);

        [[nodiscard]] FSurfaceRuntimeDataHandle LoadSurfaceData(Asset::FMeshAssetHandle      Mesh,
                                                                const std::filesystem::path& DistributionPath,
                                                                std::uint32_t                Resolution = 0);
        [[nodiscard]] FSurfaceRuntimeDataHandle LoadSurfaceDataAtResolution(FSurfaceRuntimeDataHandle Handle,
                                                                            std::uint32_t             Resolution);

        // Registry and runtime data access
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        void                        SetSimulationResolution(std::uint32_t Resolution);
        void ReleaseUnusedSurfaceData(const std::vector<FSurfaceRuntimeDataHandle>& RetainedHandles);

        [[nodiscard]] bool                       HasSurfaceData(FSurfaceRuntimeDataHandle Handle) const noexcept;
        [[nodiscard]] const FSurfaceRuntimeData& GetSurfaceData(FSurfaceRuntimeDataHandle Handle) const;
        [[nodiscard]] const std::vector<Asset::FSRProfileAssetHandle>&
        GetSurfaceProfileTable(FSurfaceRuntimeDataHandle Handle) const;
        [[nodiscard]] std::vector<Asset::FSRProfileAssetHandle> GetSceneSurfaceProfiles(const FScene& Scene) const;
        [[nodiscard]] FSurfaceStateRegistry                     BuildSurfaceStateRegistry(const FScene& Scene) const;
        FSurfaceStateRegistry ExchangeSurfaceStateRegistry(FSurfaceStateRegistry Registry) noexcept;
        [[nodiscard]] const FSurfaceStateRegistry& GetSurfaceStateRegistry() const noexcept;

    private:
        struct FRuntimeSurfaceAsset
        {
            std::shared_ptr<const FSurfaceRuntimeData> Data;
            std::vector<Asset::FSRProfileAssetHandle>  ProfileTable;
            Asset::FMeshAssetHandle                    Mesh = Asset::InvalidAssetHandle;
            std::filesystem::path                      DistributionPath;
        };

        Asset::FAssetManager&                                      Assets;
        std::vector<FRuntimeSurfaceAsset>                          RuntimeSurfaceAssets;
        std::unordered_map<std::string, FSurfaceRuntimeDataHandle> RuntimeSurfaceAssetsByInputs;
        FSurfaceStateRegistry StateRegistry{std::vector<FSurfaceResponseProfileData>{}};
        std::uint32_t         SimulationResolution = SurfaceSimulationResolution;
    };
} // namespace MDSS::SurfaceState
