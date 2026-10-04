/**
 * @file SurfaceDataManager.cpp
 * @brief Surface 전처리 데이터 캐시, Profile table과 State Registry 소유권.
 */

#include "SurfaceState/Preprocessing/SurfaceDataManager.h"

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Geometry/MesoGeometryBuilder.h"
#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceState/Mapping/NormalMapTransferNormalBuilder.h"
#include "SurfaceState/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceState/Preprocessing/SurfaceCache.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace MDSS::SurfaceState
{
    namespace
    {
        std::string MakeRuntimeSurfaceKey(const std::filesystem::path& MeshPath,
                                          const std::filesystem::path& DistributionPath,
                                          std::uint32_t                Resolution)
        {
            const std::string MeshKey = std::filesystem::absolute(MeshPath).lexically_normal().generic_string();
            const std::string DistributionKey =
                std::filesystem::absolute(DistributionPath).lexically_normal().generic_string();
            return MeshKey + '\n' + DistributionKey + '\n' + std::to_string(Resolution);
        }
    } // namespace

    TSurfaceDataManager::TSurfaceDataManager(Asset::TAssetManager& Assets) : Assets(Assets)
    {
    }

    TSurfaceRuntimeDataHandle
    TSurfaceDataManager::LoadSurfaceData(Asset::TMeshAssetHandle      MeshHandle,
                                         const std::filesystem::path& RequestedDistributionPath,
                                         std::uint32_t                Resolution)
    {
        if (Resolution == 0)
            Resolution = SimulationResolution;
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
        {
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        }
        if (MeshHandle == Asset::InvalidAssetHandle)
        {
            throw std::out_of_range("Invalid TMeshAssetHandle for Surface preprocessing.");
        }
        if (RequestedDistributionPath.empty())
        {
            throw std::invalid_argument("A Scene-selected Surface Profile Map path is required.");
        }
        const Asset::TMeshAsset&    Mesh = Assets.GetMesh(MeshHandle);
        const std::filesystem::path DistributionPath = RequestedDistributionPath;
        const std::string RuntimeKey = MakeRuntimeSurfaceKey(Mesh.GetSourcePath(), DistributionPath, Resolution);
        if (const auto Found = RuntimeSurfaceAssetsByInputs.find(RuntimeKey);
            Found != RuntimeSurfaceAssetsByInputs.end())
        {
            return Found->second;
        }
        const Asset::TSurfaceProfileDistribution Distribution =
            Asset::TSurfaceProfileDistributionLoader::Load(DistributionPath);

        std::vector<Asset::TSRProfileAssetHandle> ProfileTable;
        ProfileTable.reserve(Distribution.ProfilePaths.size());
        for (const std::filesystem::path& ProfilePath : Distribution.ProfilePaths)
        {
            ProfileTable.push_back(Assets.LoadSRProfile(ProfilePath));
        }

        SurfaceState::TSurfaceLocalID SurfaceCount = 0;
        for (const Asset::TMeshSection& Section : Mesh.GetSections())
        {
            if (Section.Surface == SurfaceState::InvalidSurfaceID)
            {
                throw std::runtime_error("Mesh section has an invalid Surface ID.");
            }
            if (Section.Surface == std::numeric_limits<SurfaceState::TSurfaceLocalID>::max() - 1U)
            {
                throw std::overflow_error("Mesh Surface count exceeds the supported range.");
            }
            SurfaceCount = std::max(SurfaceCount, static_cast<SurfaceState::TSurfaceLocalID>(Section.Surface + 1U));
        }
        if (SurfaceCount == 0)
        {
            throw std::runtime_error("Mesh contains no Surface sections to preprocess.");
        }

        std::vector<SurfaceState::TSurfaceDefinition> SurfaceDefinitions;
        SurfaceDefinitions.reserve(SurfaceCount);
        for (SurfaceState::TSurfaceLocalID Surface = 0; Surface < SurfaceCount; ++Surface)
        {
            SurfaceDefinitions.push_back({Surface, {Resolution, Resolution}});
        }

        std::vector<std::filesystem::path> NormalMapPaths(SurfaceCount);
        std::vector<bool>                  HasMaterial(SurfaceCount, false);
        for (const Asset::TMeshSection& Section : Mesh.GetSections())
        {
            const Asset::TMaterialAsset& Material = Assets.GetMaterial(Section.Material);
            const std::filesystem::path  NormalPath = Assets.GetTexture(Material.GetNormalTexture()).GetSourcePath();
            if (HasMaterial[Section.Surface] && NormalMapPaths[Section.Surface] != NormalPath)
            {
                throw std::runtime_error("One Mesh Surface resolves to multiple Normal Maps.");
            }
            NormalMapPaths[Section.Surface] = NormalPath;
            HasMaterial[Section.Surface] = true;
        }
        if (std::ranges::find(HasMaterial, false) != HasMaterial.end())
        {
            throw std::runtime_error("Mesh Surface IDs must be dense and have a material section.");
        }

        const auto                                  CacheStart = std::chrono::steady_clock::now();
        const SurfaceState::TSurfaceCacheDescriptor CacheDescriptor =
            SurfaceState::TSurfaceCache::Describe(Mesh.GetVertices(),
                                                  Mesh.GetTriangles(),
                                                  SurfaceDefinitions,
                                                  NormalMapPaths,
                                                  DistributionPath,
                                                  Distribution.ProfilePaths,
                                                  Distribution.ProfileIndicesBySurface);
        const std::filesystem::path CachePath = SurfaceState::TSurfaceCache::GetPath(
            MDSS_SURFACE_CACHE_DIR, Mesh.GetSourcePath(), DistributionPath, Resolution);
        std::string CacheDiagnostic;
        auto        CachedGeometry = SurfaceState::TSurfaceCache::Load(CachePath, CacheDescriptor, CacheDiagnostic);
        if (CachedGeometry)
        {
            const double LoadMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
            TLogger::Info("TSurfaceDataManager",
                          "Loaded .Surface cache: " + CachePath.string() + " (" + std::to_string(LoadMilliseconds) +
                              " ms, including input fingerprint).");
        }
        else
        {
            TLogger::Info("TSurfaceDataManager",
                          "Rebuilding .Surface cache (" + CacheDiagnostic + "): " + CachePath.string());
            const SurfaceState::TSurfaceMappingData Mapping = SurfaceState::TSurfaceMappingBuilder::Build(
                Mesh.GetVertices(), Mesh.GetTriangles(), SurfaceDefinitions);
            for (const std::string& MappingWarning : Mapping.Warnings)
            {
                if (MappingWarning.starts_with("UV seam texel links were dropped:"))
                {
                    TLogger::Warning("TSurfaceDataManager", MappingWarning);
                }
            }
            std::vector<SurfaceState::TSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
            CachedGeometry.emplace(SurfaceState::TSurfaceGeometryBuilder::Build(
                Mapping, std::move(ProfileMap), static_cast<std::uint32_t>(ProfileTable.size())));
            SurfaceState::TSharedSurfaceGeometryData& Geometry = *CachedGeometry;

            std::unordered_map<std::string, Asset::TextureData> NormalMapPixels;
            for (const std::filesystem::path& NormalMapPath : NormalMapPaths)
            {
                if (NormalMapPath.empty())
                {
                    continue;
                }
                const std::string Key = std::filesystem::absolute(NormalMapPath).lexically_normal().generic_string();
                if (!NormalMapPixels.contains(Key))
                {
                    NormalMapPixels.emplace(Key, Asset::TextureLoader::LoadRGBA8(NormalMapPath));
                }
            }

            std::size_t                                       MappedNormalCount = 0;
            std::vector<SurfaceState::TSurfaceTexelGeometry>& GeometryTexels = Geometry.GetTexels();
            for (SurfaceState::TSurfaceTexelGeometry& Texel : GeometryTexels)
            {
                if (!Texel.IsValid() || Texel.Surface >= NormalMapPaths.size() || NormalMapPaths[Texel.Surface].empty())
                {
                    continue;
                }

                const std::string Key =
                    std::filesystem::absolute(NormalMapPaths[Texel.Surface]).lexically_normal().generic_string();
                const auto TextureIt = NormalMapPixels.find(Key);
                glm::vec3  TransferNormal{};
                if (TextureIt != NormalMapPixels.end() &&
                    BuildNormalMapTransferNormal(
                        Texel, Mesh.GetVertices(), Mesh.GetTriangles(), TextureIt->second, TransferNormal, true))
                {
                    Texel.TransferNormal = TransferNormal;
                    Texel.HasTransferNormal = true;
                    ++MappedNormalCount;
                }
            }

            TLogger::Info("TSurfaceDataManager",
                          "Precomputed Normal Map transfer normals for " + std::to_string(MappedNormalCount) + "/" +
                              std::to_string(Geometry.GetTexelCount()) + " Simulation texels.");
            // 노멀 맵의 텍셀 노멀을 준비한 뒤 공유 형상의 높이와 파생 형상을 전처리한다.
            const SurfaceState::TMesoGeometryBuildReport MesoReport = SurfaceState::BuildMesoGeometry(Geometry);
            TLogger::Info("TSurfaceDataManager",
                          "Integrated Normal Map meso geometry for " + std::to_string(MesoReport.ActiveTexelCount) +
                              " texels across " + std::to_string(MesoReport.ComponentCount) + " connected charts (" +
                              std::to_string(MesoReport.IterationCount) + " PCG iterations, relative edge residual " +
                              std::to_string(MesoReport.RelativeEdgeResidual) + ").");
            try
            {
                SurfaceState::TSurfaceCache::Save(CachePath, CacheDescriptor, Geometry);
                const double BuildMilliseconds =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
                TLogger::Info("TSurfaceDataManager",
                              "Saved .Surface cache: " + CachePath.string() + " (" + std::to_string(BuildMilliseconds) +
                                  " ms, including preprocessing and save).");
            }
            catch (const std::exception& Error)
            {
                TLogger::Warning("TSurfaceDataManager",
                                 "Unable to save .Surface cache; using Runtime data: " + std::string(Error.what()));
            }
        }
        TSurfaceRuntimeData Built(std::move(*CachedGeometry));
        if (RuntimeSurfaceAssets.size() >= InvalidSurfaceRuntimeDataHandle)
        {
            throw std::overflow_error("Runtime Surface Data handle range is exhausted.");
        }
        const TSurfaceRuntimeDataHandle RuntimeHandle =
            static_cast<TSurfaceRuntimeDataHandle>(RuntimeSurfaceAssets.size());
        TRuntimeSurfaceAsset RuntimeAsset;
        RuntimeAsset.Data = std::make_shared<const TSurfaceRuntimeData>(std::move(Built));
        RuntimeAsset.ProfileTable = std::move(ProfileTable);
        RuntimeAsset.Mesh = MeshHandle;
        RuntimeAsset.DistributionPath = DistributionPath;
        RuntimeSurfaceAssets.push_back(std::move(RuntimeAsset));
        RuntimeSurfaceAssetsByInputs.emplace(RuntimeKey, RuntimeHandle);
        TLogger::Info("TSurfaceDataManager",
                      "Registered Runtime Surface data (" + std::to_string(Resolution) + " x " +
                          std::to_string(Resolution) + ") for Mesh: " + Mesh.GetSourcePath().string());
        return RuntimeHandle;
    }

    TSurfaceRuntimeDataHandle TSurfaceDataManager::LoadSurfaceDataAtResolution(TSurfaceRuntimeDataHandle Handle,
                                                                               std::uint32_t             Resolution)
    {
        if (!HasSurfaceData(Handle))
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        // Copy before LoadSurfaceData can grow RuntimeSurfaceAssets and invalidate references.
        const Asset::TMeshAssetHandle Mesh = RuntimeSurfaceAssets[Handle].Mesh;
        const auto                    DistributionPath = RuntimeSurfaceAssets[Handle].DistributionPath;
        return LoadSurfaceData(Mesh, DistributionPath, Resolution);
    }

    std::uint32_t TSurfaceDataManager::GetSimulationResolution() const noexcept
    {
        return SimulationResolution;
    }

    void TSurfaceDataManager::SetSimulationResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        SimulationResolution = Resolution;
    }

    void TSurfaceDataManager::ReleaseUnusedSurfaceData(const std::vector<TSurfaceRuntimeDataHandle>& RetainedHandles)
    {
        const auto IsRetained = [&](TSurfaceRuntimeDataHandle Handle)
        { return std::find(RetainedHandles.begin(), RetainedHandles.end(), Handle) != RetainedHandles.end(); };
        std::erase_if(RuntimeSurfaceAssetsByInputs, [&](const auto& Entry) { return !IsRetained(Entry.second); });
        for (std::size_t Index = 0; Index < RuntimeSurfaceAssets.size(); ++Index)
        {
            if (!IsRetained(static_cast<TSurfaceRuntimeDataHandle>(Index)))
            {
                RuntimeSurfaceAssets[Index].Data.reset();
                RuntimeSurfaceAssets[Index].ProfileTable.clear();
            }
        }
    }

    bool TSurfaceDataManager::HasSurfaceData(TSurfaceRuntimeDataHandle Handle) const noexcept
    {
        return Handle < RuntimeSurfaceAssets.size() && static_cast<bool>(RuntimeSurfaceAssets[Handle].Data);
    }

    const TSurfaceRuntimeData& TSurfaceDataManager::GetSurfaceData(TSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        }
        return *RuntimeSurfaceAssets[Handle].Data;
    }

    const std::vector<Asset::TSRProfileAssetHandle>&
    TSurfaceDataManager::GetSurfaceProfileTable(TSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Profile table handle.");
        }
        return RuntimeSurfaceAssets[Handle].ProfileTable;
    }

    std::vector<Asset::TSRProfileAssetHandle> TSurfaceDataManager::GetSceneSurfaceProfiles(const TScene& Scene) const
    {
        std::vector<Asset::TSRProfileAssetHandle> Handles;
        for (const TStaticMeshInstance& Instance : Scene.GetStaticMeshInstances())
        {
            if (HasSurfaceData(Instance.GetSurfaceData()))
            {
                const auto& Table = GetSurfaceProfileTable(Instance.GetSurfaceData());
                Handles.insert(Handles.end(), Table.begin(), Table.end());
            }
        }
        std::sort(Handles.begin(), Handles.end());
        Handles.erase(std::unique(Handles.begin(), Handles.end()), Handles.end());
        return Handles;
    }

    SurfaceState::TSurfaceStateRegistry TSurfaceDataManager::BuildSurfaceStateRegistry(const TScene& Scene) const
    {
        std::vector<SurfaceState::TSurfaceResponseProfileData> Profiles;
        for (Asset::TSRProfileAssetHandle Handle : GetSceneSurfaceProfiles(Scene))
        {
            Profiles.push_back(Assets.GetSRProfile(Handle).GetData());
        }
        return SurfaceState::TSurfaceStateRegistry(Profiles);
    }

    SurfaceState::TSurfaceStateRegistry
    TSurfaceDataManager::ExchangeSurfaceStateRegistry(SurfaceState::TSurfaceStateRegistry Registry) noexcept
    {
        std::swap(StateRegistry, Registry);
        return Registry;
    }

    const SurfaceState::TSurfaceStateRegistry& TSurfaceDataManager::GetSurfaceStateRegistry() const noexcept
    {
        return StateRegistry;
    }

} // namespace MDSS::SurfaceState
