/**
 * @file GPUImageView.h
 * @brief Vulkan image에 대한 image view 자원.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace MDSS::GPU
{
    class TGPUImageView
    {
    public:
        /** @brief image와 format·aspect 설정을 참조하는 Vulkan image view를 생성한다. */
        TGPUImageView(VkDevice Device, VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                      std::uint32_t ArrayLayers = 1, VkImageViewType ViewType = VK_IMAGE_VIEW_TYPE_2D);
        ~TGPUImageView();

        TGPUImageView(const TGPUImageView&) = delete;
        TGPUImageView& operator=(const TGPUImageView&) = delete;
        TGPUImageView(TGPUImageView&&) = delete;
        TGPUImageView& operator=(TGPUImageView&&) = delete;

        void Recreate(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                      std::uint32_t ArrayLayers = 1, VkImageViewType ViewType = VK_IMAGE_VIEW_TYPE_2D);
        void Reset();

        [[nodiscard]] VkImageView GetHandle() const noexcept;

    private:
        void Create(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask,
                    std::uint32_t ArrayLayers, VkImageViewType ViewType);

        VkDevice    Device = VK_NULL_HANDLE;
        VkImageView Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS::GPU
