/**
 * @file GPUImageView.cpp
 * @brief Vulkan image에 대한 image view 자원.
 */

#include "GPU/Vulkan/Resource/GPUImageView.h"

#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS::GPU
{
// ============================================================
// 00. Image View Lifecycle
// ============================================================
#pragma region 00_Image_View_Lifecycle

    FGPUImageView::FGPUImageView(VkDevice           Device,
                                 VkImage            Image,
                                 VkFormat           Format,
                                 VkImageAspectFlags AspectMask,
                                 std::uint32_t      ArrayLayers,
                                 VkImageViewType    ViewType)
        : Device(Device)
    {
        Create(Image, Format, AspectMask, ArrayLayers, ViewType);
    }

    FGPUImageView::~FGPUImageView()
    {
        Reset();
    }

    void FGPUImageView::Recreate(VkImage            Image,
                                 VkFormat           Format,
                                 VkImageAspectFlags AspectMask,
                                 std::uint32_t      ArrayLayers,
                                 VkImageViewType    ViewType)
    {
        Reset();
        Create(Image, Format, AspectMask, ArrayLayers, ViewType);
    }

    void FGPUImageView::Reset()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }
    }

#pragma endregion

// ============================================================
// 01. Image View Creation
// ============================================================
#pragma region 01_Image_View_Creation

    void FGPUImageView::Create(VkImage            Image,
                               VkFormat           Format,
                               VkImageAspectFlags AspectMask,
                               std::uint32_t      ArrayLayers,
                               VkImageViewType    ViewType)
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

        FLogger::Verbose("Vulkan", "TGPUImageView created (format=" + std::to_string(static_cast<int>(Format)) + ").");
    }

#pragma endregion

// ============================================================
// 02. Image View Access
// ============================================================
#pragma region 02_Image_View_Access

    VkImageView FGPUImageView::GetHandle() const noexcept
    {
        return Handle;
    }
#pragma endregion
} // namespace MDSS::GPU
