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
        [[nodiscard]] static MaterialSourceData Convert(const tinyobj::material_t&   Material,
                                                        const std::filesystem::path& TextureBaseDirectory);
    };
} // namespace MDSS
