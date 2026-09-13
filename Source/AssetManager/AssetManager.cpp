#include "AssetManager/AssetManager.h"

#include "AssetManager/Loader/OBJLoader.h"
#include "AssetManager/Loader/TextureLoader.h"
#include "VulkanContext/VulkanContext.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    AssetManager::AssetManager(const VulkanContext& Context) : Context(Context)
    {
        DefaultBaseColorTexture = CreateSolidTexture("DefaultWhite", {255, 255, 255, 255}, true);
        DefaultNormalTexture = CreateSolidTexture("DefaultFlatNormal", {128, 128, 255, 255}, false);
        DefaultMaterial =
            CreateMaterial("DefaultMaterial", {}, glm::vec4(1.0F), DefaultBaseColorTexture, DefaultNormalTexture);
    }

    MeshAssetHandle AssetManager::LoadOBJ(const std::filesystem::path& Path)
    {
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

            Sections.push_back({SourceSection.FirstIndex, SourceSection.IndexCount, Material});
        }

        const MeshAssetHandle Handle = static_cast<MeshAssetHandle>(Meshes.size());
        Meshes.push_back(std::make_unique<MeshAsset>(Handle,
                                                     Path.stem().string(),
                                                     Path,
                                                     Context,
                                                     std::move(Loaded.Vertices),
                                                     std::move(Loaded.Indices),
                                                     std::move(Sections)));
        return Handle;
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

    std::size_t AssetManager::GetMaterialCount() const noexcept
    {
        return Materials.size();
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
            return Found->second;
        }

        const TextureData        Data = TextureLoader::LoadRGBA8(Path);
        const TextureAssetHandle Handle = static_cast<TextureAssetHandle>(Textures.size());
        const VkFormat           Format = bSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        Textures.push_back(std::make_unique<TextureAsset>(
            Handle, Path.stem().string(), Path, Context, Data.Width, Data.Height, Data.Pixels, Format));
        TextureCache.emplace(CacheKey, Handle);
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
        Textures.push_back(std::make_unique<TextureAsset>(
            Handle, std::move(Name), std::filesystem::path{}, Context, 1, 1, RGBA, Format));
        return Handle;
    }

    MaterialAssetHandle AssetManager::CreateMaterial(std::string                  Name,
                                                     const std::filesystem::path& SourcePath,
                                                     glm::vec4                    BaseColor,
                                                     TextureAssetHandle           BaseColorTexture,
                                                     TextureAssetHandle           NormalTexture)
    {
        const MaterialAssetHandle Handle = static_cast<MaterialAssetHandle>(Materials.size());
        Materials.push_back(std::make_unique<MaterialAsset>(
            Handle, std::move(Name), SourcePath, BaseColor, BaseColorTexture, NormalTexture));
        return Handle;
    }
} // namespace MDSS
