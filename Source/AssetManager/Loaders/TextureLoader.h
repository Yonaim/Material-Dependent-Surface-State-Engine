/**
 * @file TextureLoader.h
 * @brief 이미지 파일을 Vulkan 업로드용 텍스처 데이터로 디코딩.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS::Asset
{
    struct FTextureData
    {
        std::uint32_t             Width = 0;
        std::uint32_t             Height = 0;
        std::vector<std::uint8_t> Pixels;
    };

    struct FHeightImageData
    {
        std::uint32_t              Width = 0;
        std::uint32_t              Height = 0;
        std::vector<std::uint16_t> Samples;
    };

    class FTextureLoader
    {
      public:
        /**
         * @brief 이미지 파일을 8-bit RGBA pixel 배열로 디코딩한다.
         * @throws std::runtime_error 파일을 읽을 수 없거나 이미지 디코딩이 실패한 경우.
         */
        [[nodiscard]] static FTextureData LoadRGBA8(const std::filesystem::path &Path);
        /** @brief PNG의 선형 R 채널을 읽는다. 8비트 입력은 16비트 범위로 확장한다. */
        [[nodiscard]] static FHeightImageData LoadHeightMap(const std::filesystem::path &Path);
        /** @brief Radiance HDR를 톤 매핑 없이 선형 RGBA16F 바이트 배열로 디코딩한다. */
        [[nodiscard]] static FTextureData LoadHdrRGBA16F(const std::filesystem::path &Path);
    };
} // namespace MDSS::Asset
