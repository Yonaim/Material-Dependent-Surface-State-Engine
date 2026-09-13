#pragma once

#include "AssetManager/Asset.h"
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
        static void TransitionImageLayout(const VulkanContext& Context,
                                          VkImage              Image,
                                          VkImageLayout        OldLayout,
                                          VkImageLayout        NewLayout);
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
