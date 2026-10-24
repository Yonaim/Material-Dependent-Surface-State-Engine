/**
 * @file MTLLoader.h
 * @brief OBJ 재질 데이터를 엔진 Material 데이터로 변환.
 */

#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace tinyobj
{
    struct material_t;
}

namespace MDSS::Asset
{
    struct TMaterialSourceData
    {
        std::string           Name;
        glm::vec4             BaseColor{1.0F};
        std::filesystem::path BaseColorTexturePath;
        std::filesystem::path NormalTexturePath;
        // x: 0 film / 1 droplets, y: opacity multiplier, z: film darkening, w: droplets per UV unit.
        glm::vec4 WaterAppearance{0.0F, 1.0F, 0.22F, 100.0F};
    };

    class TMTLLoader
    {
    public:
        /** @brief tinyobj material을 엔진 값과 OBJ 기준 texture 경로로 변환한다. */
        [[nodiscard]] static TMaterialSourceData Convert(const tinyobj::material_t&   Material,
                                                         const std::filesystem::path& TextureBaseDirectory);
    };
} // namespace MDSS::Asset
