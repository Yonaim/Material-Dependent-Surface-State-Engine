/**
 * @file TextureAsset.h
 * @brief 텍스처 이미지, image view와 sampler 자원.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "VulkanContext/GPU/GPUImage.h"
#include "VulkanContext/GPU/GPUImageView.h"
#include "VulkanContext/GPU/GPUSampler.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace MDSS
{
    class VulkanContext;

    class TextureAsset final : public Asset
    {
    public:
        /**
         * @brief RGBA8 픽셀을 Vulkan image에 업로드하고 view와 sampler를 생성한다.
         * @throws std::runtime_error 이미지 자원 생성, layout 전환 또는 upload가 실패한 경우.
         */
        TextureAsset(AssetID                          ID,
                     std::string                      Name,
                     std::filesystem::path            SourcePath,
                     const VulkanContext&             Context,
                     std::uint32_t                    Width,
                     std::uint32_t                    Height,
                     const std::vector<std::uint8_t>& RGBA8Pixels,
                     VkFormat                         Format);

        [[nodiscard]] std::uint32_t GetWidth() const noexcept;
        [[nodiscard]] std::uint32_t GetHeight() const noexcept;
        [[nodiscard]] VkFormat      GetFormat() const noexcept;
        [[nodiscard]] VkImageView   GetImageView() const noexcept;
        [[nodiscard]] VkSampler     GetSampler() const noexcept;

    private:
        /** @brief one-time command buffer로 지정된 image layout 전환을 실행한다. */
        static void TransitionImageLayout(const VulkanContext& Context,
                                          VkImage              Image,
                                          VkImageLayout        OldLayout,
                                          VkImageLayout        NewLayout);
        /** @brief staging buffer의 pixel data를 image의 color subresource로 복사한다. */
        static void CopyBufferToImage(
            const VulkanContext& Context, VkBuffer Buffer, VkImage Image, std::uint32_t Width, std::uint32_t Height);

        std::uint32_t                 Width = 0;
        std::uint32_t                 Height = 0;
        VkFormat                      Format = VK_FORMAT_UNDEFINED;
        std::unique_ptr<GPUImage>     Image;
        std::unique_ptr<GPUImageView> ImageView;
        std::unique_ptr<GPUSampler>   Sampler;
    };
} // namespace MDSS
