/**
 * @file SurfaceDataManager.h
 * @brief Scene에서 사용하는 Surface 전처리 데이터와 Registry를 소유한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    class TScene;
}
namespace MDSS::Asset
{
    class TAssetManager;
}

namespace MDSS::SurfaceState
{
    class TSurfaceDataManager final
    {
    public:
        // Runtime surface data loading
        explicit TSurfaceDataManager(Asset::TAssetManager& Assets);

        [[nodiscard]] TSurfaceRuntimeDataHandle LoadSurfaceData(Asset::TMeshAssetHandle      Mesh,
                                                                const std::filesystem::path& DistributionPath,
                                                                std::uint32_t                Resolution = 0);
        [[nodiscard]] TSurfaceRuntimeDataHandle LoadSurfaceDataAtResolution(TSurfaceRuntimeDataHandle Handle,
                                                                            std::uint32_t             Resolution);

        // Registry and runtime data access
        [[nodiscard]] std::uint32_t             GetSimulationResolution() const noexcept;
        void                                    SetSimulationResolution(std::uint32_t Resolution);
        void ReleaseUnusedSurfaceData(const std::vector<TSurfaceRuntimeDataHandle>& RetainedHandles);

        [[nodiscard]] bool                       HasSurfaceData(TSurfaceRuntimeDataHandle Handle) const noexcept;
        [[nodiscard]] const TSurfaceRuntimeData& GetSurfaceData(TSurfaceRuntimeDataHandle Handle) const;
        [[nodiscard]] const std::vector<Asset::TSRProfileAssetHandle>&
        GetSurfaceProfileTable(TSurfaceRuntimeDataHandle Handle) const;
        [[nodiscard]] std::vector<Asset::TSRProfileAssetHandle> GetSceneSurfaceProfiles(const TScene& Scene) const;
        [[nodiscard]] TSurfaceStateRegistry                     BuildSurfaceStateRegistry(const TScene& Scene) const;
        TSurfaceStateRegistry ExchangeSurfaceStateRegistry(TSurfaceStateRegistry Registry) noexcept;
        [[nodiscard]] const TSurfaceStateRegistry& GetSurfaceStateRegistry() const noexcept;

    private:
        struct TRuntimeSurfaceAsset
        {
            std::shared_ptr<const TSurfaceRuntimeData> Data;
            std::vector<Asset::TSRProfileAssetHandle>  ProfileTable;
            Asset::TMeshAssetHandle                    Mesh = Asset::InvalidAssetHandle;
            std::filesystem::path                      DistributionPath;
        };

        Asset::TAssetManager&                                      Assets;
        std::vector<TRuntimeSurfaceAsset>                          RuntimeSurfaceAssets;
        std::unordered_map<std::string, TSurfaceRuntimeDataHandle> RuntimeSurfaceAssetsByInputs;
        TSurfaceStateRegistry StateRegistry{std::vector<TSurfaceResponseProfileData>{}};
        std::uint32_t         SimulationResolution = SurfaceSimulationResolution;
    };
} // namespace MDSS::SurfaceState
