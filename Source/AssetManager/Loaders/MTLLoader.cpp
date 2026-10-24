/**
 * @file MTLLoader.cpp
 * @brief OBJ 재질 데이터를 엔진 Material 데이터로 변환.
 */

#include "AssetManager/Loaders/MTLLoader.h"

#include "Logger/Logger.h"

#include <cmath>
#include <stdexcept>
#include <tiny_obj_loader.h>

namespace MDSS::Asset
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

#pragma region TMTLLoader_Implementation

    TMaterialSourceData TMTLLoader::Convert(const tinyobj::material_t&   Material,
                                            const std::filesystem::path& TextureBaseDirectory)
    {
        TMaterialSourceData Result{};
        Result.Name = Material.name.empty() ? "Material" : Material.name;
        Result.BaseColor = {Material.diffuse[0], Material.diffuse[1], Material.diffuse[2], Material.dissolve};
        Result.BaseColorTexturePath = ResolveTexturePath(TextureBaseDirectory, Material.diffuse_texname);

        // Extended MTL uses `norm`; many exporters place tangent-space normal maps in `map_Bump`/`bump`.
        // Prefer `norm`, then accept bump_texname as a pragmatic normal-map fallback.
        const std::string& NormalTextureName =
            !Material.normal_texname.empty() ? Material.normal_texname : Material.bump_texname;
        Result.NormalTexturePath = ResolveTexturePath(TextureBaseDirectory, NormalTextureName);
        if (const auto Style = Material.unknown_parameter.find("mdss_water_style");
            Style != Material.unknown_parameter.end())
        {
            if (Style->second == "droplets")
                Result.WaterAppearance.x = 1.0F;
            else if (Style->second != "film")
                throw std::runtime_error("MTL mdss_water_style must be film or droplets.");
        }
        const auto ReadWaterParameter = [&](const char* Name, float& Target, float Minimum, float Maximum)
        {
            const auto Found = Material.unknown_parameter.find(Name);
            if (Found == Material.unknown_parameter.end())
                return;
            try
            {
                std::size_t Consumed = 0;
                const float Value = std::stof(Found->second, &Consumed);
                if (Consumed != Found->second.size() || !std::isfinite(Value) || Value < Minimum || Value > Maximum)
                    throw std::out_of_range("value");
                Target = Value;
            }
            catch (const std::exception&)
            {
                throw std::runtime_error(std::string("MTL ") + Name + " is out of range.");
            }
        };
        ReadWaterParameter("mdss_water_opacity", Result.WaterAppearance.y, 0.0F, 4.0F);
        ReadWaterParameter("mdss_water_darkening", Result.WaterAppearance.z, 0.0F, 1.0F);
        ReadWaterParameter("mdss_droplet_density", Result.WaterAppearance.w, 1.0F, 256.0F);

        TLogger::Debug("TMTLLoader",
                       "Material '" + Result.Name + "': base texture=" +
                           (Result.BaseColorTexturePath.empty() ? std::string("<default>")
                                                                : Result.BaseColorTexturePath.filename().string()) +
                           ", normal texture=" +
                           (Result.NormalTexturePath.empty() ? std::string("<flat default>")
                                                             : Result.NormalTexturePath.filename().string()) +
                           ".");
        return Result;
    }
#pragma endregion
} // namespace MDSS::Asset
