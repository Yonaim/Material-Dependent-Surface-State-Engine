#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS
{
    struct TextureData
    {
        std::uint32_t             Width = 0;
        std::uint32_t             Height = 0;
        std::vector<std::uint8_t> Pixels;
    };

    class TextureLoader
    {
    public:
        [[nodiscard]] static TextureData LoadRGBA8(const std::filesystem::path& Path);
    };
} // namespace MDSS
