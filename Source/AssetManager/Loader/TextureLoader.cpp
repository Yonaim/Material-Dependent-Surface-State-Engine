#include "AssetManager/Loader/TextureLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stdexcept>
#include <string>

namespace MDSS
{
    TextureData TextureLoader::LoadRGBA8(const std::filesystem::path& Path)
    {
        int Width = 0;
        int Height = 0;
        int Channels = 0;

        stbi_uc* Pixels = stbi_load(Path.string().c_str(), &Width, &Height, &Channels, STBI_rgb_alpha);
        if (Pixels == nullptr)
        {
            const char* Reason = stbi_failure_reason();
            throw std::runtime_error("Failed to load texture '" + Path.string() +
                                     "': " + (Reason != nullptr ? Reason : "unknown stb_image error"));
        }

        TextureData Data{};
        Data.Width = static_cast<std::uint32_t>(Width);
        Data.Height = static_cast<std::uint32_t>(Height);
        const std::size_t ByteCount = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4U;
        Data.Pixels.assign(Pixels, Pixels + ByteCount);

        stbi_image_free(Pixels);
        return Data;
    }
} // namespace MDSS
