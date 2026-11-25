/**
 * @file TextureLoader.cpp
 * @brief 이미지 파일을 RGBA8 텍스처 데이터로 디코딩.
 */

#include "AssetManager/Loaders/TextureLoader.h"
#include "Logger/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include <algorithm>
#include <cstring>
#include <glm/gtc/packing.hpp>
#include <memory>
#include <stb_image.h>
#include <stdexcept>
#include <string>

namespace MDSS::Asset
{
// ============================================================
// 00. FTextureLoader Implementation
// ============================================================
#pragma region 00_TextureLoader_Implementation
    // 일반 이미지와 선형 HDR의 GPU 업로드 형식을 제공한다.

    FTextureData FTextureLoader::LoadRGBA8(const std::filesystem::path &Path)
    {
        int Width = 0;
        int Height = 0;
        int Channels = 0;

        stbi_uc *Pixels = stbi_load(Path.string().c_str(), &Width, &Height, &Channels, STBI_rgb_alpha);

        if (Pixels == nullptr)
        {
            const char *Reason = stbi_failure_reason();
            throw std::runtime_error("Failed to load texture '" + Path.string() +
                                     "': " + (Reason != nullptr ? Reason : "unknown stb_image error"));
        }

        FTextureData Data{};
        Data.Width = static_cast<std::uint32_t>(Width);
        Data.Height = static_cast<std::uint32_t>(Height);
        const std::size_t ByteCount = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height) * 4U;
        Data.Pixels.assign(Pixels, Pixels + ByteCount);

        stbi_image_free(Pixels);
        FLogger::Debug("TextureLoader",
                       "Decoded '" + Path.filename().string() + "' as RGBA8 (" + std::to_string(Data.Width) + "x" +
                           std::to_string(Data.Height) + ", source channels=" + std::to_string(Channels) + ").");
        return Data;
    }

    FHeightImageData FTextureLoader::LoadHeightMap(const std::filesystem::path &Path)
    {
        const auto Filename = Path.string();
        if (Path.extension() != ".png")
            throw std::runtime_error("Height map must be a PNG: " + Filename);

        int Width = 0;
        int Height = 0;
        // RGB 맵에서는 기존 높이장과 같은 R 채널을 사용한다. 회색조 변환은 채널을 혼합한다.
        const std::unique_ptr<stbi_us, decltype(&stbi_image_free)> Pixels(
            stbi_load_16(Filename.c_str(), &Width, &Height, nullptr, STBI_rgb),
            stbi_image_free);
        if (!Pixels)
            throw std::runtime_error("Failed to decode height map: " + Filename);

        FHeightImageData Data{static_cast<std::uint32_t>(Width), static_cast<std::uint32_t>(Height), {}};
        Data.Samples.resize(static_cast<std::size_t>(Width) * Height);
        for (std::size_t Index = 0; Index < Data.Samples.size(); ++Index)
            Data.Samples[Index] = Pixels.get()[Index * 3U];
        return Data;
    }

    FTextureData FTextureLoader::LoadHdrRGBA16F(const std::filesystem::path &Path)
    {
        const auto Filename = Path.string();
        if (!stbi_is_hdr(Filename.c_str()))
            throw std::runtime_error("Expected a Radiance HDR file: " + Filename);

        int                                                      Width = 0;
        int                                                      Height = 0;
        const std::unique_ptr<float, decltype(&stbi_image_free)> Pixels(
            stbi_loadf(Filename.c_str(), &Width, &Height, nullptr, STBI_rgb_alpha),
            stbi_image_free);
        if (!Pixels)
            throw std::runtime_error("Failed to decode HDR texture: " + Filename);

        FTextureData Data{static_cast<std::uint32_t>(Width), static_cast<std::uint32_t>(Height), {}};
        const auto   Components = static_cast<std::size_t>(Width) * Height * 4U;
        Data.Pixels.resize(Components * sizeof(std::uint16_t));
        for (std::size_t Index = 0; Index < Components; ++Index)
        {
            // FP16의 유한 범위까지만 보존해 매우 밝은 광원이 무한대로 변하는 것을 막는다.
            const auto Encoded = glm::packHalf1x16(std::clamp(Pixels.get()[Index], 0.F, 65504.F));
            std::memcpy(Data.Pixels.data() + Index * sizeof(Encoded), &Encoded, sizeof(Encoded));
        }
        return Data;
    }
#pragma endregion
} // namespace MDSS::Asset
