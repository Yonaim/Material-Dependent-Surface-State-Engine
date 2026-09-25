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

namespace MDSS
{
    struct MaterialSourceData
    {
        std::string           Name;
        glm::vec4             BaseColor{1.0F};
        std::filesystem::path BaseColorTexturePath;
        std::filesystem::path NormalTexturePath;
    };

    class MTLLoader
    {
    public:
        /** @brief tinyobj material을 엔진 값과 OBJ 기준 texture 경로로 변환한다. */
        [[nodiscard]] static MaterialSourceData Convert(const tinyobj::material_t&   Material,
                                                        const std::filesystem::path& TextureBaseDirectory);
    };
} // namespace MDSS
