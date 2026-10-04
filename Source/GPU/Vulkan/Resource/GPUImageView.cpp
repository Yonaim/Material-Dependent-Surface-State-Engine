/**
 * @file GPUImageView.cpp
 * @brief Vulkan image에 대한 image view 자원.
 */

#include "GPU/Vulkan/Resource/GPUImageView.h"

#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS::GPU
{
    TGPUImageView::TGPUImageView(VkDevice Device, VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                                 std::uint32_t ArrayLayers, VkImageViewType ViewType)
        : Device(Device)
    {
        Create(Image, Format, AspectMask, ArrayLayers, ViewType);
    }

    TGPUImageView::~TGPUImageView()
    {
        Reset();
    }

    void TGPUImageView::Recreate(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                                 std::uint32_t ArrayLayers, VkImageViewType ViewType)
    {
        Reset();
        Create(Image, Format, AspectMask, ArrayLayers, ViewType);
    }

    void TGPUImageView::Reset()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }
    }

    void TGPUImageView::Create(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                               std::uint32_t ArrayLayers, VkImageViewType ViewType)
    {
        VkImageViewCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        CreateInfo.image = Image;
        CreateInfo.viewType = ViewType;
        CreateInfo.format = Format;
        CreateInfo.subresourceRange.aspectMask = AspectMask;
        CreateInfo.subresourceRange.baseMipLevel = 0;
        CreateInfo.subresourceRange.levelCount = 1;
        CreateInfo.subresourceRange.baseArrayLayer = 0;
        CreateInfo.subresourceRange.layerCount = ArrayLayers;

        if (vkCreateImageView(Device, &CreateInfo, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan image view.");
        }
        TLogger::Verbose("Vulkan", "TGPUImageView created (format=" + std::to_string(static_cast<int>(Format)) + ").");
    }

    VkImageView TGPUImageView::GetHandle() const noexcept
    {
        return Handle;
    }
} // namespace MDSS::GPU
