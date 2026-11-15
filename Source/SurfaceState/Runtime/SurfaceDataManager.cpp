/**
 * @file SurfaceDataManager.cpp
 * @brief Surface 전처리 데이터 캐시, Profile table과 State Registry 소유권.
 */

#include "SurfaceState/Runtime/SurfaceDataManager.h"

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Cache/SurfaceCache.h"
#include "SurfaceState/Geometry/MesoGeometryBuilder.h"
#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceState/Mapping/NormalMapTransferNormalBuilder.h"
#include "SurfaceState/Mapping/SurfaceMappingBuilder.h"

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
// ============================================================
// 00. Runtime Surface Key Helper
// ============================================================
#pragma region 00_Runtime_Surface_Key_Helper
// Mesh·Profile 분포·설정으로 공유 전처리 자료의 식별 key를 만든다.

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
#pragma endregion

// ============================================================
// 01. Surface Data Loading
// ============================================================
#pragma region 01_Surface_Data_Loading
// 필요한 mapping·Meso·Profile 전처리 결과를 로드하거나 생성한다.

    namespace
    {
        std::uint32_t ResolveSurfaceResolution(std::uint32_t Resolution, std::uint32_t DefaultResolution)
        {
            const auto Selected = Resolution == 0 ? DefaultResolution : Resolution;
            if (!SurfaceState::IsSurfaceSimulationResolution(Selected))
            {
                throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
            }
            return Selected;
        }

        void ValidateSurfaceRequest(Asset::FMeshAssetHandle MeshHandle,
                                    const std::filesystem::path& DistributionPath)
        {
            if (MeshHandle == Asset::InvalidAssetHandle)
            {
                throw std::out_of_range("Invalid TMeshAssetHandle for Surface preprocessing.");
            }
            if (DistributionPath.empty())
            {
                throw std::invalid_argument("A Scene-selected Surface Profile Map path is required.");
            }
        }

        std::vector<Asset::FSRProfileAssetHandle>
        LoadSurfaceProfileTable(Asset::FAssetManager& Assets, const std::vector<std::filesystem::path>& Paths)
        {
            std::vector<Asset::FSRProfileAssetHandle> Table;
            Table.reserve(Paths.size());
            for (const auto& Path : Paths)
            {
                Table.push_back(Assets.LoadSRProfile(Path));
            }
            return Table;
        }

        struct FSurfaceGeometryInputs
        {
            std::vector<SurfaceState::FSurfaceDefinition> Definitions;
            std::vector<std::filesystem::path> NormalMapPaths;
        };

        FSurfaceGeometryInputs BuildSurfaceGeometryInputs(Asset::FAssetManager& Assets,
                                                          const Asset::FMeshAsset& Mesh,
                                                          std::uint32_t Resolution)
        {
            SurfaceState::FSurfaceLocalID SurfaceCount = 0;

            for (const Asset::FMeshSection& Section : Mesh.GetSections())
            {
                if (Section.Surface == SurfaceState::InvalidSurfaceID)
                {
                    throw std::runtime_error("Mesh section has an invalid Surface ID.");
                }

                if (Section.Surface == std::numeric_limits<SurfaceState::FSurfaceLocalID>::max() - 1U)
                {
                    throw std::overflow_error("Mesh Surface count exceeds the supported range.");
                }

                SurfaceCount = std::max(SurfaceCount, static_cast<SurfaceState::FSurfaceLocalID>(Section.Surface + 1U));
            }

            if (SurfaceCount == 0)
            {
                throw std::runtime_error("Mesh contains no Surface sections to preprocess.");
            }

            FSurfaceGeometryInputs Inputs;
            Inputs.Definitions.reserve(SurfaceCount);

            for (SurfaceState::FSurfaceLocalID Surface = 0; Surface < SurfaceCount; ++Surface)
            {
                Inputs.Definitions.push_back({Surface, {Resolution, Resolution}});
            }

            Inputs.NormalMapPaths.resize(SurfaceCount);
            std::vector<bool>                  HasMaterial(SurfaceCount, false);

            for (const Asset::FMeshSection& Section : Mesh.GetSections())
            {
                const Asset::FMaterialAsset& Material = Assets.GetMaterial(Section.Material);
                const std::filesystem::path  NormalPath = Assets.GetTexture(Material.GetNormalTexture()).GetSourcePath();

                if (HasMaterial[Section.Surface] && Inputs.NormalMapPaths[Section.Surface] != NormalPath)
                {
                    throw std::runtime_error("One Mesh Surface resolves to multiple Normal Maps.");
                }

                Inputs.NormalMapPaths[Section.Surface] = NormalPath;
                HasMaterial[Section.Surface] = true;
            }

            if (std::ranges::find(HasMaterial, false) != HasMaterial.end())
            {
                throw std::runtime_error("Mesh Surface IDs must be dense and have a material section.");
            }

            return Inputs;
        }
    } // namespace

    namespace
    {
        void PrecomputeTransferNormals(SurfaceState::FSharedSurfaceGeometryData& Geometry,
                                        const Asset::FMeshAsset& Mesh,
                                        const std::vector<std::filesystem::path>& NormalMapPaths)
        {
            // 텍셀의 UV 접선 공간 노멀을 공유 Surface Geometry에 미리 기록한다.
            std::unordered_map<std::string, Asset::FTextureData> NormalMapPixels;

            for (const std::filesystem::path& NormalMapPath : NormalMapPaths)
            {
                if (NormalMapPath.empty())
                {
                    continue;
                }

                const std::string Key = std::filesystem::absolute(NormalMapPath).lexically_normal().generic_string();

                if (!NormalMapPixels.contains(Key))
                {
                    NormalMapPixels.emplace(Key, Asset::FTextureLoader::LoadRGBA8(NormalMapPath));
                }
            }

            std::size_t                                       MappedNormalCount = 0;
            std::vector<SurfaceState::FSurfaceTexelGeometry>& GeometryTexels = Geometry.GetTexels();

            for (SurfaceState::FSurfaceTexelGeometry& Texel : GeometryTexels)
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
                    Texel.bHasTransferNormal = true;
                    ++MappedNormalCount;
                }
            }

            FLogger::Info("TSurfaceDataManager",
                          "Precomputed Normal Map transfer normals for " + std::to_string(MappedNormalCount) + "/" +
                              std::to_string(Geometry.GetTexelCount()) + " Simulation texels.");
        }

        SurfaceState::FSharedSurfaceGeometryData LoadOrBuildSurfaceGeometry(
            const Asset::FMeshAsset& Mesh,
            const Asset::FSurfaceProfileDistribution& Distribution,
            const FSurfaceGeometryInputs& Inputs,
            const std::filesystem::path& DistributionPath,
            std::size_t ProfileCount,
            std::uint32_t Resolution)
        {
            // 캐시 Fingerprint로 입력을 검증하고, miss일 때 geometry를 전처리·저장한다.
            const auto                                  CacheStart = std::chrono::steady_clock::now();
            const SurfaceState::FSurfaceCacheDescriptor CacheDescriptor =
                SurfaceState::FSurfaceCache::Describe(Mesh.GetVertices(),
                                                      Mesh.GetTriangles(),
                                                      Inputs.Definitions,
                                                      Inputs.NormalMapPaths,
                                                      DistributionPath,
                                                      Distribution.ProfilePaths,
                                                      Distribution.ProfileIndicesBySurface);
            const std::filesystem::path CachePath = SurfaceState::FSurfaceCache::GetPath(
                MDSS_SURFACE_CACHE_DIR, Mesh.GetSourcePath(), DistributionPath, Resolution);
            std::string CacheDiagnostic;
            auto        CachedGeometry = SurfaceState::FSurfaceCache::Load(CachePath, CacheDescriptor, CacheDiagnostic);

            if (CachedGeometry)
            {
                const double LoadMilliseconds =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
                FLogger::Info("TSurfaceDataManager",
                              "Loaded .Surface cache: " + CachePath.string() + " (" + std::to_string(LoadMilliseconds) +
                                  " ms, including input fingerprint).");
            }
            else
            {
                FLogger::Info("TSurfaceDataManager",
                              "Rebuilding .Surface cache (" + CacheDiagnostic + "): " + CachePath.string());
                const SurfaceState::FSurfaceMappingData Mapping = SurfaceState::FSurfaceMappingBuilder::Build(
                    Mesh.GetVertices(), Mesh.GetTriangles(), Inputs.Definitions);
                for (const std::string& MappingWarning : Mapping.Warnings)
                {
                    if (MappingWarning.starts_with("UV seam texel links were dropped:"))
                    {
                        FLogger::Warning("TSurfaceDataManager", MappingWarning);
                    }
                }

                std::vector<SurfaceState::FSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
                CachedGeometry.emplace(SurfaceState::FSurfaceGeometryBuilder::Build(
                    Mapping, std::move(ProfileMap), static_cast<std::uint32_t>(ProfileCount)));
                SurfaceState::FSharedSurfaceGeometryData& Geometry = *CachedGeometry;

                PrecomputeTransferNormals(Geometry, Mesh, Inputs.NormalMapPaths);

                // 노멀 맵의 텍셀 노멀을 준비한 뒤 공유 형상의 높이와 파생 형상을 전처리한다.
                const SurfaceState::FMesoGeometryBuildReport MesoReport = SurfaceState::BuildMesoGeometry(Geometry);
                FLogger::Info("TSurfaceDataManager",
                              "Integrated Normal Map meso geometry for " + std::to_string(MesoReport.ActiveTexelCount) +
                                  " texels across " + std::to_string(MesoReport.ComponentCount) + " connected charts (" +
                                  std::to_string(MesoReport.IterationCount) + " PCG iterations, relative edge residual " +
                                  std::to_string(MesoReport.RelativeEdgeResidual) + ").");
                try
                {
                    SurfaceState::FSurfaceCache::Save(CachePath, CacheDescriptor, Geometry);
                    const double BuildMilliseconds =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
                    FLogger::Info("TSurfaceDataManager",
                                  "Saved .Surface cache: " + CachePath.string() + " (" + std::to_string(BuildMilliseconds) +
                                      " ms, including preprocessing and save).");
                }
                catch (const std::exception& Error)
                {
                    FLogger::Warning("TSurfaceDataManager",
                                     "Unable to save .Surface cache; using Runtime data: " + std::string(Error.what()));
                }
            }


            return std::move(*CachedGeometry);
        }
    } // namespace

    FSurfaceDataManager::FSurfaceDataManager(Asset::FAssetManager& Assets) : Assets(Assets)
    {
    }

    FSurfaceRuntimeDataHandle
    FSurfaceDataManager::LoadSurfaceData(Asset::FMeshAssetHandle      MeshHandle,
                                         const std::filesystem::path& RequestedDistributionPath,
                                         std::uint32_t                Resolution)
    {
        Resolution = ResolveSurfaceResolution(Resolution, SimulationResolution);
        ValidateSurfaceRequest(MeshHandle, RequestedDistributionPath);

        const Asset::FMeshAsset& Mesh = Assets.GetMesh(MeshHandle);
        const std::filesystem::path DistributionPath = RequestedDistributionPath;
        const std::string RuntimeKey = MakeRuntimeSurfaceKey(Mesh.GetSourcePath(), DistributionPath, Resolution);

        if (const auto Found = RuntimeSurfaceAssetsByInputs.find(RuntimeKey);
            Found != RuntimeSurfaceAssetsByInputs.end())
        {
            return Found->second;
        }

        const Asset::FSurfaceProfileDistribution Distribution =
            Asset::FSurfaceProfileDistributionLoader::Load(DistributionPath);
        auto ProfileTable = LoadSurfaceProfileTable(Assets, Distribution.ProfilePaths);
        const auto Inputs = BuildSurfaceGeometryInputs(Assets, Mesh, Resolution);

        auto Geometry = LoadOrBuildSurfaceGeometry(
            Mesh, Distribution, Inputs, DistributionPath, ProfileTable.size(), Resolution);
        FSurfaceRuntimeData Built(std::move(Geometry));

        if (RuntimeSurfaceAssets.size() >= InvalidSurfaceRuntimeDataHandle)
        {
            throw std::overflow_error("Runtime Surface Data handle range is exhausted.");
        }

        const FSurfaceRuntimeDataHandle RuntimeHandle =
            static_cast<FSurfaceRuntimeDataHandle>(RuntimeSurfaceAssets.size());
        FRuntimeSurfaceAsset RuntimeAsset;
        RuntimeAsset.Data = std::make_shared<const FSurfaceRuntimeData>(std::move(Built));
        RuntimeAsset.ProfileTable = std::move(ProfileTable);
        RuntimeAsset.Mesh = MeshHandle;
        RuntimeAsset.DistributionPath = DistributionPath;
        RuntimeSurfaceAssets.push_back(std::move(RuntimeAsset));
        RuntimeSurfaceAssetsByInputs.emplace(RuntimeKey, RuntimeHandle);
        FLogger::Info("TSurfaceDataManager",
                      "Registered Runtime Surface data (" + std::to_string(Resolution) + " x " +
                          std::to_string(Resolution) + ") for Mesh: " + Mesh.GetSourcePath().string());
        return RuntimeHandle;
    }

    FSurfaceRuntimeDataHandle FSurfaceDataManager::LoadSurfaceDataAtResolution(FSurfaceRuntimeDataHandle Handle,
                                                                               std::uint32_t             Resolution)
    {
        if (!HasSurfaceData(Handle))
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        // Copy before LoadSurfaceData can grow RuntimeSurfaceAssets and invalidate references.
        const Asset::FMeshAssetHandle Mesh = RuntimeSurfaceAssets[Handle].Mesh;
        const auto                    DistributionPath = RuntimeSurfaceAssets[Handle].DistributionPath;

        return LoadSurfaceData(Mesh, DistributionPath, Resolution);
    }
#pragma endregion

// ============================================================
// 02. Registry and Surface Data Accessors
// ============================================================
#pragma region 02_Registry_and_Surface_Data_Accessors
// Scene의 State Registry와 runtime Surface 자료를 관련 소비자에 제공한다.

    std::uint32_t FSurfaceDataManager::GetSimulationResolution() const noexcept
    {
        return SimulationResolution;
    }

    void FSurfaceDataManager::SetSimulationResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        SimulationResolution = Resolution;
    }

    void FSurfaceDataManager::ReleaseUnusedSurfaceData(const std::vector<FSurfaceRuntimeDataHandle>& RetainedHandles)
    {
        const auto IsRetained = [&](FSurfaceRuntimeDataHandle Handle)
        { return std::find(RetainedHandles.begin(), RetainedHandles.end(), Handle) != RetainedHandles.end(); };
        std::erase_if(RuntimeSurfaceAssetsByInputs, [&](const auto& Entry) { return !IsRetained(Entry.second); });

        for (std::size_t Index = 0; Index < RuntimeSurfaceAssets.size(); ++Index)
        {
            if (!IsRetained(static_cast<FSurfaceRuntimeDataHandle>(Index)))
            {
                RuntimeSurfaceAssets[Index].Data.reset();
                RuntimeSurfaceAssets[Index].ProfileTable.clear();
            }
        }
    }

    bool FSurfaceDataManager::HasSurfaceData(FSurfaceRuntimeDataHandle Handle) const noexcept
    {
        return Handle < RuntimeSurfaceAssets.size() && static_cast<bool>(RuntimeSurfaceAssets[Handle].Data);
    }

    const FSurfaceRuntimeData& FSurfaceDataManager::GetSurfaceData(FSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        }

        return *RuntimeSurfaceAssets[Handle].Data;
    }

    const std::vector<Asset::FSRProfileAssetHandle>&
    FSurfaceDataManager::GetSurfaceProfileTable(FSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Profile table handle.");
        }

        return RuntimeSurfaceAssets[Handle].ProfileTable;
    }

    std::vector<Asset::FSRProfileAssetHandle> FSurfaceDataManager::GetSceneSurfaceProfiles(const FScene& Scene) const
    {
        std::vector<Asset::FSRProfileAssetHandle> Handles;

        for (const FStaticMeshInstance& Instance : Scene.GetStaticMeshInstances())
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

    SurfaceState::FSurfaceStateRegistry FSurfaceDataManager::BuildSurfaceStateRegistry(const FScene& Scene) const
    {
        std::vector<SurfaceState::FSurfaceResponseProfileData> Profiles;

        for (Asset::FSRProfileAssetHandle Handle : GetSceneSurfaceProfiles(Scene))
        {
            Profiles.push_back(Assets.GetSRProfile(Handle).GetData());
        }

        return SurfaceState::FSurfaceStateRegistry(Profiles);
    }

    SurfaceState::FSurfaceStateRegistry
    FSurfaceDataManager::ExchangeSurfaceStateRegistry(SurfaceState::FSurfaceStateRegistry Registry) noexcept
    {
        std::swap(StateRegistry, Registry);
        return Registry;
    }

    const SurfaceState::FSurfaceStateRegistry& FSurfaceDataManager::GetSurfaceStateRegistry() const noexcept
    {
        return StateRegistry;
    }

#pragma endregion
} // namespace MDSS::SurfaceState
