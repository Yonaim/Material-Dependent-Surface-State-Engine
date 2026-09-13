#include "VulkanContext/GPU/GPUImageView.h"

#include <stdexcept>

namespace MDSS
{
    GPUImageView::GPUImageView(VkDevice Device, VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask)
        : Device(Device)
    {
        VkImageViewCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        CreateInfo.image = Image;
        CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        CreateInfo.format = Format;
        CreateInfo.subresourceRange.aspectMask = AspectMask;
        CreateInfo.subresourceRange.baseMipLevel = 0;
        CreateInfo.subresourceRange.levelCount = 1;
        CreateInfo.subresourceRange.baseArrayLayer = 0;
        CreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(Device, &CreateInfo, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan image view.");
        }
    }

    GPUImageView::~GPUImageView()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }
    }

    VkImageView GPUImageView::GetHandle() const noexcept
    {
        return Handle;
    }
} // namespace MDSS
