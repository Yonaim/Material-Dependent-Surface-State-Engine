/**
 * @file LiquidMaterialLoader.cpp
 * @brief 액체 재질 파일을 CPU와 GPU가 공유하는 물성으로 변환한다.
 */

#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "AssetManager/Loaders/YamlAssetLoader.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace MDSS::Asset
{
    // ============================================================
    // 00. Physical Material Paths
    // ============================================================
    #pragma region 00_Physical_Material_Paths
        namespace
        {
            float OptionalNumber(const nlohmann::json &Phase, const char *Key, float Default, float Min, float Max)
            {
                const auto Found = Phase.find(Key);
                if (Found == Phase.end())
                    return Default;
                if (!Found->is_number())
                    throw std::runtime_error(std::string(Key) + " must be a number.");
                const float Value = Found->get<float>();
                if (!std::isfinite(Value) || Value < Min || Value > Max)
                    throw std::runtime_error(std::string(Key) + " is outside its supported range.");
                return Value;
            }

            std::filesystem::path MaterialPath(std::string_view Id)
            {
                if (Id.empty() || !std::ranges::all_of(Id,
                                                       [](char C)
                                                       {
                                                           return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
                                                                  (C >= '0' && C <= '9') || C == '_';
                                                       }))
                    throw std::invalid_argument("Invalid physical material ID.");
                return std::filesystem::path(MDSS_ASSET_DIR) / "Simulation" / "Materials" /
                       (std::string(Id) + ".PhysicalMaterial");
            }
        } // namespace
    #pragma endregion

    Liquid::FLiquidMaterial LoadLiquidMaterialById(std::string_view Id)
    {
        return LoadLiquidMaterial(MaterialPath(Id));
    }

    Liquid::FLiquidMaterial LoadLiquidMaterial(const std::filesystem::path &Path)
    {
        const auto Source = Path.empty() ? MaterialPath("Water") : Path;
        const bool bTyped = Source.extension() == ".PhysicalMaterial";

        try
        {
            std::ifstream LegacyStream;
            if (!bTyped)
            {
                LegacyStream.open(Source);
                if (!LegacyStream)
                    throw std::runtime_error("Unable to open asset.");
            }
            const auto  Data = bTyped ? LoadVersionedYamlAsset(Source) : nlohmann::json::parse(LegacyStream);
            const auto &LiquidProperties = bTyped ? Data.at("phases").at("liquid") : Data;
            const auto &Density = LiquidProperties.at("densityKgM3");
            const auto &Viscosity = LiquidProperties.at("dynamicViscosityPaS");
            if (!Density.is_number() || !Viscosity.is_number())
                throw std::runtime_error("densityKgM3 and dynamicViscosityPaS must be numbers.");
            Liquid::FLiquidMaterial Material{Density.get<float>(), Viscosity.get<float>()};
            const float             ReposeAngle =
                OptionalNumber(LiquidProperties, "angleOfReposeDeg", 0.F, 0.F, std::nextafter(90.F, 0.F));
            Material.Friction = std::tan(ReposeAngle * 0.01745329252F);
            Material.YieldStressPa =
                OptionalNumber(LiquidProperties, "yieldStressPa", 0.F, 0.F, std::numeric_limits<float>::max());
            Material.RefractiveIndex =
                OptionalNumber(LiquidProperties, "refractiveIndex", Material.RefractiveIndex, 1.F, 4.F);
            Material.Roughness = OptionalNumber(LiquidProperties, "roughness", Material.Roughness, 0.F, 1.F);
            if (bTyped)
                Material.bIncandescent = LiquidProperties.value("incandescent", false);
            // 구형 CLI 파일은 규칙을 직접 지정한다. 새 씬은 프로파일로 이 선택을 덮어쓴다.
            const auto Rule = bTyped ? (Data.at("phases").contains("ice") ? "water" : "none")
                                     : Data.at("localRule").get<std::string>();
            if (bTyped && Data.at("phases").contains("ice"))
            {
                const auto &Ice = Data.at("phases").at("ice");
                const auto &IceDensity = Ice.at("densityKgM3");
                if (!IceDensity.is_number())
                    throw std::runtime_error("ice densityKgM3 must be a number.");
                Material.IceDensityKgM3 = IceDensity.get<float>();
                Material.IceRefractiveIndex =
                    OptionalNumber(Ice, "refractiveIndex", Material.IceRefractiveIndex, 1.F, 4.F);
                Material.IceRoughness = OptionalNumber(Ice, "roughness", Material.IceRoughness, 0.F, 1.F);
                Material.FrostRoughness = OptionalNumber(Ice, "frostRoughness", Material.FrostRoughness, 0.F, 1.F);
            }
            if (Rule == "water")
                Material.LocalRule = Liquid::ELocalMaterialRule::Water;
            else if (Rule != "none")
                throw std::runtime_error("localRule must be water or none.");
            if (!std::isfinite(Material.DensityKgM3) || Material.DensityKgM3 <= 0.F ||
                !std::isfinite(Material.DynamicViscosityPaS) || Material.DynamicViscosityPaS <= 0.F ||
                !std::isfinite(Material.IceDensityKgM3) || Material.IceDensityKgM3 <= 0.F)
                throw std::runtime_error("Density and dynamic viscosity must be finite and positive.");
            return Material;
        }
        catch (const std::exception &Error)
        {
            throw std::runtime_error("Invalid liquid material " + Source.string() + ": " + Error.what());
        }
    }
} // namespace MDSS::Asset
