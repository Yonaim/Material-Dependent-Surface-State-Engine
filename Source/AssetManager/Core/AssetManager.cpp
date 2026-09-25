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
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

namespace MDSS
{
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
            const TMeshAssetHandle Handle = Found->second;
            if (!HasSurfaceData(Handle))
            {
                std::filesystem::path DistributionPath = Path;
                DistributionPath.replace_extension(".SurfaceProfileMap");
                if (std::filesystem::exists(DistributionPath))
                {
                    LoadSurfaceData(Handle, DistributionPath);
                }
            }
            return Handle;
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
        const std::size_t     VertexCount = Loaded.Vertices.size();
        const std::size_t     IndexCount = Loaded.Indices.size();
        const std::size_t     SectionCount = Sections.size();
        const std::size_t     MaterialCount = MaterialRemap.size();

        Meshes.push_back(std::make_unique<TMeshAsset>(Handle,
                                                     Path.stem().string(),
                                                     Path,
                                                     Context,
                                                     std::move(Loaded.Vertices),
                                                     std::move(Loaded.Indices),
                                                     std::move(Sections),
                                                     std::move(Loaded.Triangles)));
        SurfaceData.emplace_back();
        SurfaceProfileTables.emplace_back();
        MeshAssetsByPath.emplace(MeshPathKey, Handle);
        TLogger::Info("TAssetManager",
                     "OBJ registered as TMeshAsset handle=" + std::to_string(Handle) +
                         " (vertices=" + std::to_string(VertexCount) + ", indices=" + std::to_string(IndexCount) +
                         ", sections=" + std::to_string(SectionCount) +
                         ", imported materials=" + std::to_string(MaterialCount) + ").");

        std::filesystem::path DistributionPath = Path;
        DistributionPath.replace_extension(".SurfaceProfileMap");
        if (std::filesystem::exists(DistributionPath))
        {
            LoadSurfaceData(Handle, DistributionPath);
        }
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
        StateRegistry.reset();
        return Handle;
    }

    void TAssetManager::LoadSurfaceData(TMeshAssetHandle              MeshHandle,
                                       const std::filesystem::path& RequestedDistributionPath)
    {
        if (MeshHandle >= Meshes.size())
        {
            throw std::out_of_range("Invalid TMeshAssetHandle for Surface preprocessing.");
        }
        const TMeshAsset&      Mesh = *Meshes[MeshHandle];
        std::filesystem::path DistributionPath = RequestedDistributionPath;
        if (DistributionPath.empty())
        {
            DistributionPath = Mesh.GetSourcePath();
            DistributionPath.replace_extension(".SurfaceProfileMap");
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
            SurfaceDefinitions.push_back({Surface, DefaultSurfaceResolution});
        }

        std::vector<std::filesystem::path> NormalMapPaths(SurfaceCount);
        std::vector<bool>                  HasMaterial(SurfaceCount, false);
        for (const TMeshSection& Section : Mesh.GetSections())
        {
            const TMaterialAsset&        Material = GetMaterial(Section.Material);
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

        const TSurfaceMappingData Mapping =
            TSurfaceMappingBuilder::Build(Mesh.GetVertices(), Mesh.GetTriangles(), SurfaceDefinitions);
        std::vector<TSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
        TSurfaceRuntimeData               Built =
            TSurfacePreprocessor::Build(Mapping, std::move(ProfileMap), static_cast<std::uint32_t>(ProfileTable.size()));
        SurfaceData[MeshHandle] = std::make_shared<const TSurfaceRuntimeData>(std::move(Built));
        SurfaceProfileTables[MeshHandle] = std::move(ProfileTable);
        TLogger::Info("TAssetManager", "Built Runtime Surface data for Mesh: " + Mesh.GetSourcePath().string());
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

    bool TAssetManager::HasSurfaceData(TMeshAssetHandle Handle) const noexcept
    {
        return Handle < SurfaceData.size() && static_cast<bool>(SurfaceData[Handle]);
    }

    const TSurfaceRuntimeData& TAssetManager::GetSurfaceData(TMeshAssetHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Mesh has no Runtime Surface data.");
        }
        return *SurfaceData[Handle];
    }

    const std::vector<TSRProfileAssetHandle>& TAssetManager::GetSurfaceProfileTable(TMeshAssetHandle Handle) const
    {
        if (!HasSurfaceData(Handle))
        {
            throw std::out_of_range("Mesh has no loaded Surface Profile table.");
        }
        return SurfaceProfileTables[Handle];
    }

    const TSurfaceStateRegistry& TAssetManager::GetSurfaceStateRegistry() const
    {
        if (!StateRegistry)
        {
            std::vector<TSurfaceResponseProfileData> Profiles;
            Profiles.reserve(SRProfiles.size());
            for (const std::unique_ptr<TSRProfileAsset>& Profile : SRProfiles)
            {
                Profiles.push_back(Profile->GetData());
            }
            StateRegistry = std::make_unique<TSurfaceStateRegistry>(Profiles);
        }
        return *StateRegistry;
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
        const std::string         LogName = Name;
        Materials.push_back(std::make_unique<TMaterialAsset>(
            Handle, std::move(Name), SourcePath, BaseColor, BaseColorTexture, NormalTexture));
        TLogger::Debug("TAssetManager",
                      "Material registered: '" + LogName + "' (handle=" + std::to_string(Handle) +
                          ", base texture=" + std::to_string(BaseColorTexture) +
                          ", normal texture=" + std::to_string(NormalTexture) + ").");
        return Handle;
    }
} // namespace MDSS
