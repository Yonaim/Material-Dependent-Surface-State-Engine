/**
 * @file AssetManager.cpp
 * @brief 에셋 로딩, Runtime 데이터 소유권 관리와 조회.
 */

#include "AssetManager/Core/AssetManager.h"

#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "SurfaceStateSystem/Geometry/MesoGeometryBuilder.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/Mapping/NormalMapTransferNormalBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceCache.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <stdexcept>
#include <utility>

namespace MDSS
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

    } // 내부 네임스페이스

    TAssetManager::TAssetManager(const TVulkanContext& Context) : Context(Context)
    {
        TLogger::Info("TAssetManager", "Initializing default material resources.");
        DefaultBaseColorTexture = CreateSolidTexture("DefaultWhite", {255, 255, 255, 255}, true);
        DefaultNormalTexture = CreateSolidTexture("DefaultFlatNormal", {128, 128, 255, 255}, false);
        DefaultMaterial =
            CreateMaterial("DefaultMaterial", {}, glm::vec4(1.0F), DefaultBaseColorTexture, DefaultNormalTexture);
        TLogger::Debug("TAssetManager", "Default white texture, flat normal texture, and material created.");
    }

    TMeshAssetHandle TAssetManager::LoadOBJ(const std::filesystem::path& Path)
    {
        const std::string MeshPathKey = std::filesystem::absolute(Path).lexically_normal().generic_string();
        if (const auto Found = MeshAssetsByPath.find(MeshPathKey); Found != MeshAssetsByPath.end())
        {
            return Found->second;
        }

        TLogger::Info("TAssetManager", "Loading OBJ asset: " + Path.string());
        TOBJLoadResult Loaded = TOBJLoader::Load(Path);

        std::vector<TMaterialAssetHandle> MaterialRemap;
        MaterialRemap.reserve(Loaded.Materials.size());

        for (const TMaterialSourceData& SourceMaterial : Loaded.Materials)
        {
            const TextureAssetHandle BaseColorTexture = SourceMaterial.BaseColorTexturePath.empty()
                                                            ? DefaultBaseColorTexture
                                                            : LoadTexture(SourceMaterial.BaseColorTexturePath, true);
            const TextureAssetHandle NormalTexture = SourceMaterial.NormalTexturePath.empty()
                                                         ? DefaultNormalTexture
                                                         : LoadTexture(SourceMaterial.NormalTexturePath, false);

            MaterialRemap.push_back(
                CreateMaterial(SourceMaterial.Name, Path, SourceMaterial.BaseColor, BaseColorTexture, NormalTexture));
        }

        std::vector<TMeshSection> Sections;
        Sections.reserve(Loaded.Sections.size());
        for (const TOBJMeshSectionData& SourceSection : Loaded.Sections)
        {
            TMaterialAssetHandle Material = DefaultMaterial;
            if (SourceSection.MaterialIndex >= 0 &&
                static_cast<std::size_t>(SourceSection.MaterialIndex) < MaterialRemap.size())
            {
                Material = MaterialRemap[static_cast<std::size_t>(SourceSection.MaterialIndex)];
            }

            Sections.push_back({SourceSection.FirstIndex, SourceSection.IndexCount, Material, SourceSection.Surface});
        }

        const TMeshAssetHandle Handle = static_cast<TMeshAssetHandle>(Meshes.size());
        const std::size_t      VertexCount = Loaded.Vertices.size();
        const std::size_t      IndexCount = Loaded.Indices.size();
        const std::size_t      SectionCount = Sections.size();
        const std::size_t      MaterialCount = MaterialRemap.size();

        Meshes.push_back(std::make_unique<TMeshAsset>(Handle,
                                                      Path.stem().string(),
                                                      Path,
                                                      Context,
                                                      std::move(Loaded.Vertices),
                                                      std::move(Loaded.Indices),
                                                      std::move(Sections),
                                                      std::move(Loaded.Triangles)));
        MeshAssetsByPath.emplace(MeshPathKey, Handle);
        TLogger::Info("TAssetManager",
                      "OBJ registered as TMeshAsset handle=" + std::to_string(Handle) +
                          " (vertices=" + std::to_string(VertexCount) + ", indices=" + std::to_string(IndexCount) +
                          ", sections=" + std::to_string(SectionCount) +
                          ", imported materials=" + std::to_string(MaterialCount) + ").");

        return Handle;
    }

    TSRProfileAssetHandle TAssetManager::LoadSRProfile(const std::filesystem::path& Path)
    {
        const std::string CacheKey = std::filesystem::absolute(Path).lexically_normal().generic_string();
        if (const auto Found = SRProfileCache.find(CacheKey); Found != SRProfileCache.end())
        {
            return Found->second;
        }
        if (SRProfiles.size() >= InvalidAssetHandle)
        {
            throw std::overflow_error("TSRProfileAsset handle range is exhausted.");
        }

        const TSRProfileAssetHandle      Handle = static_cast<TSRProfileAssetHandle>(SRProfiles.size());
        std::unique_ptr<TSRProfileAsset> Profile = TSRProfileLoader::Load(Handle, Path);
        TLogger::Info("TAssetManager",
                      "SRProfile registered: '" + Profile->GetName() + "' (handle=" + std::to_string(Handle) + ").");
        SRProfiles.push_back(std::move(Profile));
        SRProfileCache.emplace(CacheKey, Handle);
        return Handle;
    }

    TSurfaceRuntimeDataHandle TAssetManager::LoadSurfaceData(TMeshAssetHandle             MeshHandle,
                                                             const std::filesystem::path& RequestedDistributionPath,
                                                             std::uint32_t                Resolution)
    {
        if (Resolution == 0)
            Resolution = SimulationResolution;
        if (!IsSurfaceSimulationResolution(Resolution))
        {
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        }
        if (MeshHandle >= Meshes.size())
        {
            throw std::out_of_range("Invalid TMeshAssetHandle for Surface preprocessing.");
        }
        if (RequestedDistributionPath.empty())
        {
            throw std::invalid_argument("A Scene-selected Surface Profile Map path is required.");
        }
        const TMeshAsset&           Mesh = *Meshes[MeshHandle];
        const std::filesystem::path DistributionPath = RequestedDistributionPath;
        const std::string RuntimeKey = MakeRuntimeSurfaceKey(Mesh.GetSourcePath(), DistributionPath, Resolution);
        if (const auto Found = RuntimeSurfaceAssetsByInputs.find(RuntimeKey);
            Found != RuntimeSurfaceAssetsByInputs.end())
        {
            return Found->second;
        }
        const TSurfaceProfileDistribution Distribution = TSurfaceProfileDistributionLoader::Load(DistributionPath);

        std::vector<TSRProfileAssetHandle> ProfileTable;
        ProfileTable.reserve(Distribution.ProfilePaths.size());
        for (const std::filesystem::path& ProfilePath : Distribution.ProfilePaths)
        {
            ProfileTable.push_back(LoadSRProfile(ProfilePath));
        }

        TSurfaceLocalID SurfaceCount = 0;
        for (const TMeshSection& Section : Mesh.GetSections())
        {
            if (Section.Surface == InvalidSurfaceID)
            {
                throw std::runtime_error("Mesh section has an invalid Surface ID.");
            }
            if (Section.Surface == std::numeric_limits<TSurfaceLocalID>::max() - 1U)
            {
                throw std::overflow_error("Mesh Surface count exceeds the supported range.");
            }
            SurfaceCount = std::max(SurfaceCount, static_cast<TSurfaceLocalID>(Section.Surface + 1U));
        }
        if (SurfaceCount == 0)
        {
            throw std::runtime_error("Mesh contains no Surface sections to preprocess.");
        }

        std::vector<TSurfaceDefinition> SurfaceDefinitions;
        SurfaceDefinitions.reserve(SurfaceCount);
        for (TSurfaceLocalID Surface = 0; Surface < SurfaceCount; ++Surface)
        {
            SurfaceDefinitions.push_back({Surface, {Resolution, Resolution}});
        }

        std::vector<std::filesystem::path> NormalMapPaths(SurfaceCount);
        std::vector<bool>                  HasMaterial(SurfaceCount, false);
        for (const TMeshSection& Section : Mesh.GetSections())
        {
            const TMaterialAsset&       Material = GetMaterial(Section.Material);
            const std::filesystem::path NormalPath = GetTexture(Material.GetNormalTexture()).GetSourcePath();
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

        const auto                    CacheStart = std::chrono::steady_clock::now();
        const TSurfaceCacheDescriptor CacheDescriptor = TSurfaceCache::Describe(Mesh.GetVertices(),
                                                                                Mesh.GetTriangles(),
                                                                                SurfaceDefinitions,
                                                                                NormalMapPaths,
                                                                                DistributionPath,
                                                                                Distribution.ProfilePaths,
                                                                                Distribution.ProfileIndicesBySurface);
        const std::filesystem::path   CachePath =
            TSurfaceCache::GetPath(MDSS_SURFACE_CACHE_DIR, Mesh.GetSourcePath(), DistributionPath, Resolution);
        std::string CacheDiagnostic;
        auto        CachedGeometry = TSurfaceCache::Load(CachePath, CacheDescriptor, CacheDiagnostic);
        if (CachedGeometry)
        {
            const double LoadMilliseconds =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
            TLogger::Info("TAssetManager",
                          "Loaded .Surface cache: " + CachePath.string() + " (" + std::to_string(LoadMilliseconds) +
                              " ms, including input fingerprint).");
        }
        else
        {
            TLogger::Info("TAssetManager",
                          "Rebuilding .Surface cache (" + CacheDiagnostic + "): " + CachePath.string());
            const TSurfaceMappingData Mapping =
                TSurfaceMappingBuilder::Build(Mesh.GetVertices(), Mesh.GetTriangles(), SurfaceDefinitions);
            for (const std::string& MappingWarning : Mapping.Warnings)
            {
                if (MappingWarning.starts_with("UV seam texel links were dropped:"))
                {
                    TLogger::Warning("TAssetManager", MappingWarning);
                }
            }
            std::vector<TSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
            CachedGeometry.emplace(TSurfaceGeometryBuilder::Build(
                Mapping, std::move(ProfileMap), static_cast<std::uint32_t>(ProfileTable.size())));
            TSharedSurfaceGeometryData& Geometry = *CachedGeometry;

            std::unordered_map<std::string, TextureData> NormalMapPixels;
            for (const std::filesystem::path& NormalMapPath : NormalMapPaths)
            {
                if (NormalMapPath.empty())
                {
                    continue;
                }
                const std::string Key = std::filesystem::absolute(NormalMapPath).lexically_normal().generic_string();
                if (!NormalMapPixels.contains(Key))
                {
                    NormalMapPixels.emplace(Key, TextureLoader::LoadRGBA8(NormalMapPath));
                }
            }

            std::size_t                         MappedNormalCount = 0;
            std::vector<TSurfaceTexelGeometry>& GeometryTexels = Geometry.GetTexels();
            for (TSurfaceTexelGeometry& Texel : GeometryTexels)
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

            TLogger::Info("TAssetManager",
                          "Precomputed Normal Map transfer normals for " + std::to_string(MappedNormalCount) + "/" +
                              std::to_string(Geometry.GetTexelCount()) + " Simulation texels.");
            // 노멀 맵의 텍셀 노멀을 준비한 뒤 공유 형상의 높이와 파생 형상을 전처리한다.
            const TMesoGeometryBuildReport MesoReport = BuildMesoGeometry(Geometry);
            TLogger::Info("TAssetManager",
                          "Integrated Normal Map meso geometry for " + std::to_string(MesoReport.ActiveTexelCount) +
                              " texels across " + std::to_string(MesoReport.ComponentCount) + " connected charts (" +
                              std::to_string(MesoReport.IterationCount) + " PCG iterations, relative edge residual " +
                              std::to_string(MesoReport.RelativeEdgeResidual) + ").");
            try
            {
                TSurfaceCache::Save(CachePath, CacheDescriptor, Geometry);
                const double BuildMilliseconds =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - CacheStart).count();
                TLogger::Info("TAssetManager",
                              "Saved .Surface cache: " + CachePath.string() + " (" + std::to_string(BuildMilliseconds) +
                                  " ms, including preprocessing and save).");
            }
            catch (const std::exception& Error)
            {
                TLogger::Warning("TAssetManager",
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
        TLogger::Info("TAssetManager",
                      "Registered Runtime Surface data (" + std::to_string(Resolution) + " x " +
                          std::to_string(Resolution) + ") for Mesh: " + Mesh.GetSourcePath().string());
        return RuntimeHandle;
    }

    TSurfaceRuntimeDataHandle TAssetManager::LoadSurfaceDataAtResolution(TSurfaceRuntimeDataHandle Handle,
                                                                         std::uint32_t             Resolution)
    {
        if (!HasSurfaceData(Handle))
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        // Copy before LoadSurfaceData can grow RuntimeSurfaceAssets and invalidate references.
        const TMeshAssetHandle Mesh = RuntimeSurfaceAssets[Handle].Mesh;
        const auto             DistributionPath = RuntimeSurfaceAssets[Handle].DistributionPath;
        return LoadSurfaceData(Mesh, DistributionPath, Resolution);
    }

    std::uint32_t TAssetManager::GetSimulationResolution() const noexcept
    {
        return SimulationResolution;
    }

    void TAssetManager::SetSimulationResolution(std::uint32_t Resolution)
    {
        if (!IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        SimulationResolution = Resolution;
    }

    void TAssetManager::ReleaseUnusedSurfaceData(const std::vector<TSurfaceRuntimeDataHandle>& RetainedHandles)
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

    const TMeshAsset& TAssetManager::GetMesh(TMeshAssetHandle Handle) const
    {
        if (Handle >= Meshes.size())
        {
            throw std::out_of_range("Invalid TMeshAssetHandle.");
        }
        return *Meshes[Handle];
    }

    const TMaterialAsset& TAssetManager::GetMaterial(TMaterialAssetHandle Handle) const
    {
        if (Handle >= Materials.size())
        {
            throw std::out_of_range("Invalid TMaterialAssetHandle.");
        }
        return *Materials[Handle];
    }

    const TextureAsset& TAssetManager::GetTexture(TextureAssetHandle Handle) const
    {
        if (Handle >= Textures.size())
        {
            throw std::out_of_range("Invalid TextureAssetHandle.");
        }
        return *Textures[Handle];
    }

    const TSRProfileAsset& TAssetManager::GetSRProfile(TSRProfileAssetHandle Handle) const
    {
        if (Handle >= SRProfiles.size())
        {
            throw std::out_of_range("Invalid TSRProfileAssetHandle.");
        }
        return *SRProfiles[Handle];
    }

    bool TAssetManager::HasSurfaceData(TSurfaceRuntimeDataHandle Handle) const noexcept
    {
        return Handle < RuntimeSurfaceAssets.size() && static_cast<bool>(RuntimeSurfaceAssets[Handle].Data);
    }

    const TSurfaceRuntimeData& TAssetManager::GetSurfaceData(TSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Data handle.");
        }
        return *RuntimeSurfaceAssets[Handle].Data;
    }

    const std::vector<TSRProfileAssetHandle>&
    TAssetManager::GetSurfaceProfileTable(TSurfaceRuntimeDataHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Invalid Runtime Surface Profile table handle.");
        }
        return RuntimeSurfaceAssets[Handle].ProfileTable;
    }

    std::vector<TSRProfileAssetHandle> TAssetManager::GetSceneSurfaceProfiles(const TScene& Scene) const
    {
        std::vector<TSRProfileAssetHandle> Handles;
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

    TSurfaceStateRegistry TAssetManager::BuildSurfaceStateRegistry(const TScene& Scene) const
    {
        std::vector<TSurfaceResponseProfileData> Profiles;
        for (TSRProfileAssetHandle Handle : GetSceneSurfaceProfiles(Scene))
        {
            Profiles.push_back(GetSRProfile(Handle).GetData());
        }
        return TSurfaceStateRegistry(Profiles);
    }

    TSurfaceStateRegistry TAssetManager::ExchangeSurfaceStateRegistry(TSurfaceStateRegistry Registry) noexcept
    {
        std::swap(StateRegistry, Registry);
        return Registry;
    }

    const TSurfaceStateRegistry& TAssetManager::GetSurfaceStateRegistry() const noexcept
    {
        return StateRegistry;
    }

    std::size_t TAssetManager::GetMaterialCount() const noexcept
    {
        return Materials.size();
    }

    std::size_t TAssetManager::GetSRProfileCount() const noexcept
    {
        return SRProfiles.size();
    }

    TMaterialAssetHandle TAssetManager::GetDefaultMaterialHandle() const noexcept
    {
        return DefaultMaterial;
    }

    TextureAssetHandle TAssetManager::LoadTexture(const std::filesystem::path& Path, bool bSRGB)
    {
        const std::string CacheKey = Path.lexically_normal().string() + (bSRGB ? "#srgb" : "#linear");
        if (const auto Found = TextureCache.find(CacheKey); Found != TextureCache.end())
        {
            TLogger::Verbose("TAssetManager", "Texture cache hit: " + Path.string());
            return Found->second;
        }

        TLogger::Debug("TAssetManager",
                       "Loading " + std::string(bSRGB ? "sRGB" : "linear") + " texture: " + Path.string());

        const TextureData        Data = TextureLoader::LoadRGBA8(Path);
        const TextureAssetHandle Handle = static_cast<TextureAssetHandle>(Textures.size());
        const VkFormat           Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        Textures.push_back(std::make_unique<TextureAsset>(
            Handle, Path.stem().string(), Path, Context, Data.Width, Data.Height, Data.Pixels, Format));
        TextureCache.emplace(CacheKey, Handle);
        TLogger::Info("TAssetManager",
                      "Texture registered: " + Path.filename().string() + " (handle=" + std::to_string(Handle) + ", " +
                          std::to_string(Data.Width) + "x" + std::to_string(Data.Height) + ").");
        return Handle;
    }

    TextureAssetHandle
    TAssetManager::CreateSolidTexture(std::string Name, const std::vector<std::uint8_t>& RGBA, bool bSRGB)
    {
        if (RGBA.size() != 4U)
        {
            throw std::invalid_argument("Solid fallback texture requires exactly one RGBA8 pixel.");
        }

        const TextureAssetHandle Handle = static_cast<TextureAssetHandle>(Textures.size());
        const VkFormat           Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        const std::string        LogName = Name;
        Textures.push_back(std::make_unique<TextureAsset>(
            Handle, std::move(Name), std::filesystem::path{}, Context, 1, 1, RGBA, Format));
        TLogger::Verbose("TAssetManager",
                         "Created solid fallback texture '" + LogName + "' (handle=" + std::to_string(Handle) + ").");
        return Handle;
    }

    TMaterialAssetHandle TAssetManager::CreateMaterial(std::string                  Name,
                                                       const std::filesystem::path& SourcePath,
                                                       glm::vec4                    BaseColor,
                                                       TextureAssetHandle           BaseColorTexture,
                                                       TextureAssetHandle           NormalTexture)
    {
        const TMaterialAssetHandle Handle = static_cast<TMaterialAssetHandle>(Materials.size());
        const std::string          LogName = Name;
        Materials.push_back(std::make_unique<TMaterialAsset>(
            Handle, std::move(Name), SourcePath, BaseColor, BaseColorTexture, NormalTexture));
        TLogger::Debug("TAssetManager",
                       "Material registered: '" + LogName + "' (handle=" + std::to_string(Handle) +
                           ", base texture=" + std::to_string(BaseColorTexture) +
                           ", normal texture=" + std::to_string(NormalTexture) + ").");
        return Handle;
    }
} // MDSS 네임스페이스
