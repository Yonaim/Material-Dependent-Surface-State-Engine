/**
 * @file TextureLoader.h
 * @brief 이미지 파일을 RGBA8 텍스처 데이터로 디코딩.
 */

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
        /**
         * @brief 이미지 파일을 8-bit RGBA pixel 배열로 디코딩한다.
         * @throws std::runtime_error 파일을 읽을 수 없거나 이미지 디코딩이 실패한 경우.
         */
        [[nodiscard]] static TextureData LoadRGBA8(const std::filesystem::path& Path);
    };
} // namespace MDSS
