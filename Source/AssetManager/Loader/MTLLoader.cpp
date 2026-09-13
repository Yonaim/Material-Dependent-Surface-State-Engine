#include "AssetManager/Loader/MTLLoader.h"

#include "Logger/Logger.h"

#include <tiny_obj_loader.h>

namespace MDSS
{
    namespace
    {
        std::filesystem::path ResolveTexturePath(const std::filesystem::path& BaseDirectory,
                                                 const std::string&           TextureName)
        {
            if (TextureName.empty())
            {
                return {};
            }

            std::filesystem::path Path(TextureName);
            if (Path.is_relative())
            {
                Path = BaseDirectory / Path;
            }
            return Path.lexically_normal();
        }
    } // namespace

    MaterialSourceData MTLLoader::Convert(const tinyobj::material_t&   Material,
                                          const std::filesystem::path& TextureBaseDirectory)
    {
        MaterialSourceData Result{};
        Result.Name = Material.name.empty() ? "Material" : Material.name;
        Result.BaseColor = {Material.diffuse[0], Material.diffuse[1], Material.diffuse[2], Material.dissolve};
        Result.BaseColorTexturePath = ResolveTexturePath(TextureBaseDirectory, Material.diffuse_texname);

        // Extended MTL uses `norm`; many exporters place tangent-space normal maps in `map_Bump`/`bump`.
        // Prefer `norm`, then accept bump_texname as a pragmatic normal-map fallback.
        const std::string& NormalTextureName =
            !Material.normal_texname.empty() ? Material.normal_texname : Material.bump_texname;
        Result.NormalTexturePath = ResolveTexturePath(TextureBaseDirectory, NormalTextureName);

        Logger::Debug("MTLLoader",
                      "Material '" + Result.Name + "': base texture=" +
                          (Result.BaseColorTexturePath.empty() ? std::string("<default>")
                                                               : Result.BaseColorTexturePath.filename().string()) +
                          ", normal texture=" +
                          (Result.NormalTexturePath.empty() ? std::string("<flat default>")
                                                            : Result.NormalTexturePath.filename().string()) +
                          ".");
        return Result;
    }
} // namespace MDSS
