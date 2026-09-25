/**
 * @file AssetManager.cpp
 * @brief 에셋 로딩, 소유권 관리와 캐시 조회.
 */

#include "AssetManager/Core/AssetManager.h"

#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "Logger/Logger.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

#ifndef MDSS_PROJECT_ROOT
#define MDSS_PROJECT_ROOT "."
#endif

#ifndef MDSS_CACHE_DIR
#define MDSS_CACHE_DIR "Cache/Surface"
#endif

namespace MDSS
{
    AssetManager::AssetManager(const VulkanContext& Context) : Context(Context)
    {
        Logger::Info("AssetManager", "Initializing default material resources.");
        DefaultBaseColorTexture = CreateSolidTexture("DefaultWhite", {255, 255, 255, 255}, true);
        DefaultNormalTexture = CreateSolidTexture("DefaultFlatNormal", {128, 128, 255, 255}, false);
        DefaultMaterial =
            CreateMaterial("DefaultMaterial", {}, glm::vec4(1.0F), DefaultBaseColorTexture, DefaultNormalTexture);
        Logger::Debug("AssetManager", "Default white texture, flat normal texture, and material created.");
    }

    MeshAssetHandle AssetManager::LoadOBJ(const std::filesystem::path& Path)
    {
        Logger::Info("AssetManager", "Loading OBJ asset: " + Path.string());
        OBJLoadResult Loaded = OBJLoader::Load(Path);

        std::vector<MaterialAssetHandle> MaterialRemap;
        MaterialRemap.reserve(Loaded.Materials.size());

        for (const MaterialSourceData& SourceMaterial : Loaded.Materials)
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

        std::vector<MeshSection> Sections;
        Sections.reserve(Loaded.Sections.size());
        for (const OBJMeshSectionData& SourceSection : Loaded.Sections)
        {
            MaterialAssetHandle Material = DefaultMaterial;
            if (SourceSection.MaterialIndex >= 0 &&
                static_cast<std::size_t>(SourceSection.MaterialIndex) < MaterialRemap.size())
            {
                Material = MaterialRemap[static_cast<std::size_t>(SourceSection.MaterialIndex)];
            }

            Sections.push_back({SourceSection.FirstIndex, SourceSection.IndexCount, Material, SourceSection.Surface});
        }

        const MeshAssetHandle Handle = static_cast<MeshAssetHandle>(Meshes.size());
        const std::size_t     VertexCount = Loaded.Vertices.size();
        const std::size_t     IndexCount = Loaded.Indices.size();
        const std::size_t     SectionCount = Sections.size();
        const std::size_t     MaterialCount = MaterialRemap.size();

        Meshes.push_back(std::make_unique<MeshAsset>(Handle,
                                                     Path.stem().string(),
                                                     Path,
                                                     Context,
                                                     std::move(Loaded.Vertices),
                                                     std::move(Loaded.Indices),
                                                     std::move(Sections),
                                                     std::move(Loaded.Triangles)));
        SurfaceAssets.emplace_back();
        SurfaceProfileTables.emplace_back();
        Logger::Info("AssetManager",
                     "OBJ registered as MeshAsset handle=" + std::to_string(Handle) +
                         " (vertices=" + std::to_string(VertexCount) + ", indices=" + std::to_string(IndexCount) +
                         ", sections=" + std::to_string(SectionCount) +
                         ", imported materials=" + std::to_string(MaterialCount) + ").");

        std::filesystem::path DistributionPath = Path;
        DistributionPath.replace_extension(".SurfaceProfileMap");
        if (std::filesystem::exists(DistributionPath))
        {
            LoadSurfaceAsset(Handle, DistributionPath);
        }
        return Handle;
    }

    SRProfileAssetHandle AssetManager::LoadSRProfile(const std::filesystem::path& Path)
    {
        const std::string CacheKey = std::filesystem::absolute(Path).lexically_normal().generic_string();
        if (const auto Found = SRProfileCache.find(CacheKey); Found != SRProfileCache.end())
        {
            return Found->second;
        }
        if (SRProfiles.size() >= InvalidAssetHandle)
        {
            throw std::overflow_error("SRProfileAsset handle range is exhausted.");
        }

        const SRProfileAssetHandle      Handle = static_cast<SRProfileAssetHandle>(SRProfiles.size());
        std::unique_ptr<SRProfileAsset> Profile = SRProfileLoader::Load(Handle, Path);
        Logger::Info("AssetManager",
                     "SRProfile registered: '" + Profile->GetName() + "' (handle=" + std::to_string(Handle) + ").");
        SRProfiles.push_back(std::move(Profile));
        SRProfileCache.emplace(CacheKey, Handle);
        StateRegistry.reset();
        return Handle;
    }

    void AssetManager::LoadSurfaceAsset(MeshAssetHandle MeshHandle, const std::filesystem::path& RequestedDistributionPath)
    {
        if (MeshHandle >= Meshes.size())
        {
            throw std::out_of_range("Invalid MeshAssetHandle for Surface preprocessing.");
        }
        const MeshAsset& Mesh = *Meshes[MeshHandle];
        std::filesystem::path DistributionPath = RequestedDistributionPath;
        if (DistributionPath.empty())
        {
            DistributionPath = Mesh.GetSourcePath();
            DistributionPath.replace_extension(".SurfaceProfileMap");
        }
        const SurfaceProfileDistribution Distribution = SurfaceProfileDistributionLoader::Load(DistributionPath);

        std::vector<SRProfileAssetHandle> ProfileTable;
        ProfileTable.reserve(Distribution.ProfilePaths.size());
        for (const std::filesystem::path& ProfilePath : Distribution.ProfilePaths)
        {
            ProfileTable.push_back(LoadSRProfile(ProfilePath));
        }

        SurfaceLocalID SurfaceCount = 0;
        for (const MeshSection& Section : Mesh.GetSections())
        {
            if (Section.Surface == InvalidSurfaceID)
            {
                throw std::runtime_error("Mesh section has an invalid Surface ID.");
            }
            if (Section.Surface == std::numeric_limits<SurfaceLocalID>::max() - 1U)
            {
                throw std::overflow_error("Mesh Surface count exceeds the supported range.");
            }
            SurfaceCount = std::max(SurfaceCount, static_cast<SurfaceLocalID>(Section.Surface + 1U));
        }
        if (SurfaceCount == 0)
        {
            throw std::runtime_error("Mesh contains no Surface sections to preprocess.");
        }

        std::vector<SurfaceDefinition> SurfaceDefinitions;
        SurfaceDefinitions.reserve(SurfaceCount);
        std::vector<SurfaceResolution> Resolutions(SurfaceCount, DefaultSurfaceResolution);
        for (SurfaceLocalID Surface = 0; Surface < SurfaceCount; ++Surface)
        {
            SurfaceDefinitions.push_back({Surface, DefaultSurfaceResolution});
        }

        std::vector<std::filesystem::path> NormalMapPaths(SurfaceCount);
        std::vector<bool>                  HasMaterial(SurfaceCount, false);
        for (const MeshSection& Section : Mesh.GetSections())
        {
            const MaterialAsset& Material = GetMaterial(Section.Material);
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

        SurfaceCacheMetadata Metadata = SurfacePreprocessor::CreateSourceMetadata(
            Mesh.GetSourcePath(),
            NormalMapPaths,
            Resolutions,
            0,
            1,
            static_cast<std::uint32_t>(ProfileTable.size()));

        const std::filesystem::path CachePath = SurfaceCache::GetPath(Mesh.GetSourcePath(),
                                                                       MDSS_PROJECT_ROOT,
                                                                       MDSS_CACHE_DIR);
        std::unique_ptr<SurfacePreprocessedAsset> LoadedAsset;
        if (std::filesystem::exists(CachePath))
        {
            try
            {
                LoadedAsset = std::make_unique<SurfacePreprocessedAsset>(SurfaceCache::Load(CachePath, Metadata));
                const SharedSurfaceGeometryData& CachedGeometry = LoadedAsset->Geometry;
                for (std::size_t Index = 0; Index < CachedGeometry.GetTexelCount(); ++Index)
                {
                    const SurfaceTexelGeometry& Texel = CachedGeometry.GetTexels()[Index];
                    const SurfaceProfileIndex ExpectedProfile =
                        Texel.IsValid() && Texel.Surface < Distribution.ProfileIndicesBySurface.size()
                            ? Distribution.ProfileIndicesBySurface[Texel.Surface]
                            : InvalidSurfaceProfileIndex;
                    if (CachedGeometry.GetProfileIndex(static_cast<LocalTexelIndex>(Index)) != ExpectedProfile)
                    {
                        throw std::runtime_error("cached Surface Profile map does not match the current distribution");
                    }
                }
                Logger::Info("AssetManager", "Surface cache hit: " + CachePath.string());
            }
            catch (const std::exception& Exception)
            {
                LoadedAsset.reset();
                Logger::Warning("AssetManager",
                                "Surface cache miss/stale; rebuilding '" + CachePath.string() + "': " + Exception.what());
            }
        }

        if (!LoadedAsset)
        {
            const SurfaceMappingData Mapping =
                SurfaceMappingBuilder::Build(Mesh.GetVertices(), Mesh.GetTriangles(), SurfaceDefinitions);
            std::vector<SurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
            Metadata.ProfileMapHash = SurfacePreprocessor::HashProfileMap(ProfileMap);
            SurfacePreprocessedAsset Built =
                SurfacePreprocessor::Build(Mapping, std::move(ProfileMap), static_cast<std::uint32_t>(ProfileTable.size()),
                                            std::move(Metadata));
            std::filesystem::create_directories(CachePath.parent_path());
            SurfaceCache::Save(CachePath, Built);
            LoadedAsset = std::make_unique<SurfacePreprocessedAsset>(std::move(Built));
            Logger::Info("AssetManager", "Built and saved Surface cache: " + CachePath.string());
        }

        SurfaceAssets[MeshHandle] = std::move(LoadedAsset);
        SurfaceProfileTables[MeshHandle] = std::move(ProfileTable);
    }

    const MeshAsset& AssetManager::GetMesh(MeshAssetHandle Handle) const
    {
        if (Handle >= Meshes.size())
        {
            throw std::out_of_range("Invalid MeshAssetHandle.");
        }
        return *Meshes[Handle];
    }

    const MaterialAsset& AssetManager::GetMaterial(MaterialAssetHandle Handle) const
    {
        if (Handle >= Materials.size())
        {
            throw std::out_of_range("Invalid MaterialAssetHandle.");
        }
        return *Materials[Handle];
    }

    const TextureAsset& AssetManager::GetTexture(TextureAssetHandle Handle) const
    {
        if (Handle >= Textures.size())
        {
            throw std::out_of_range("Invalid TextureAssetHandle.");
        }
        return *Textures[Handle];
    }

    const SRProfileAsset& AssetManager::GetSRProfile(SRProfileAssetHandle Handle) const
    {
        if (Handle >= SRProfiles.size())
        {
            throw std::out_of_range("Invalid SRProfileAssetHandle.");
        }
        return *SRProfiles[Handle];
    }

    bool AssetManager::HasSurfaceAsset(MeshAssetHandle Handle) const noexcept
    {
        return Handle < SurfaceAssets.size() && static_cast<bool>(SurfaceAssets[Handle]);
    }

    const SurfacePreprocessedAsset& AssetManager::GetSurfaceAsset(MeshAssetHandle Handle) const
    {
        if (!HasSurfaceAsset(Handle))
        {
            throw std::out_of_range("Mesh has no loaded Surface preprocessed asset.");
        }
        return *SurfaceAssets[Handle];
    }

    const std::vector<SRProfileAssetHandle>& AssetManager::GetSurfaceProfileTable(MeshAssetHandle Handle) const
    {
        if (!HasSurfaceAsset(Handle))
        {
            throw std::out_of_range("Mesh has no loaded Surface Profile table.");
        }
        return SurfaceProfileTables[Handle];
    }

    const SurfaceStateRegistry& AssetManager::GetSurfaceStateRegistry() const
    {
        if (!StateRegistry)
        {
            std::vector<SurfaceResponseProfileData> Profiles;
            Profiles.reserve(SRProfiles.size());
            for (const std::unique_ptr<SRProfileAsset>& Profile : SRProfiles)
            {
                Profiles.push_back(Profile->GetData());
            }
            StateRegistry = std::make_unique<SurfaceStateRegistry>(Profiles);
        }
        return *StateRegistry;
    }

    std::size_t AssetManager::GetMaterialCount() const noexcept
    {
        return Materials.size();
    }

    std::size_t AssetManager::GetSRProfileCount() const noexcept
    {
        return SRProfiles.size();
    }

    MaterialAssetHandle AssetManager::GetDefaultMaterialHandle() const noexcept
    {
        return DefaultMaterial;
    }

    TextureAssetHandle AssetManager::LoadTexture(const std::filesystem::path& Path, bool bSRGB)
    {
        const std::string CacheKey = Path.lexically_normal().string() + (bSRGB ? "#srgb" : "#linear");
        if (const auto Found = TextureCache.find(CacheKey); Found != TextureCache.end())
        {
            Logger::Verbose("AssetManager", "Texture cache hit: " + Path.string());
            return Found->second;
        }

        Logger::Debug("AssetManager",
                      "Loading " + std::string(bSRGB ? "sRGB" : "linear") + " texture: " + Path.string());

        const TextureData        Data = TextureLoader::LoadRGBA8(Path);
        const TextureAssetHandle Handle = static_cast<TextureAssetHandle>(Textures.size());
        const VkFormat           Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        Textures.push_back(std::make_unique<TextureAsset>(
            Handle, Path.stem().string(), Path, Context, Data.Width, Data.Height, Data.Pixels, Format));
        TextureCache.emplace(CacheKey, Handle);
        Logger::Info("AssetManager",
                     "Texture registered: " + Path.filename().string() + " (handle=" + std::to_string(Handle) + ", " +
                         std::to_string(Data.Width) + "x" + std::to_string(Data.Height) + ").");
        return Handle;
    }

    TextureAssetHandle
    AssetManager::CreateSolidTexture(std::string Name, const std::vector<std::uint8_t>& RGBA, bool bSRGB)
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
        Logger::Verbose("AssetManager",
                        "Created solid fallback texture '" + LogName + "' (handle=" + std::to_string(Handle) + ").");
        return Handle;
    }

    MaterialAssetHandle AssetManager::CreateMaterial(std::string                  Name,
                                                     const std::filesystem::path& SourcePath,
                                                     glm::vec4                    BaseColor,
                                                     TextureAssetHandle           BaseColorTexture,
                                                     TextureAssetHandle           NormalTexture)
    {
        const MaterialAssetHandle Handle = static_cast<MaterialAssetHandle>(Materials.size());
        const std::string         LogName = Name;
        Materials.push_back(std::make_unique<MaterialAsset>(
            Handle, std::move(Name), SourcePath, BaseColor, BaseColorTexture, NormalTexture));
        Logger::Debug("AssetManager",
                      "Material registered: '" + LogName + "' (handle=" + std::to_string(Handle) +
                          ", base texture=" + std::to_string(BaseColorTexture) +
                          ", normal texture=" + std::to_string(NormalTexture) + ").");
        return Handle;
    }
} // namespace MDSS
