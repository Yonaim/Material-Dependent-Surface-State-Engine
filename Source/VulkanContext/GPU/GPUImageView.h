#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class GPUImageView
    {
    public:
        GPUImageView(VkDevice Device, VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask);
        ~GPUImageView();

        GPUImageView(const GPUImageView&) = delete;
        GPUImageView& operator=(const GPUImageView&) = delete;
        GPUImageView(GPUImageView&&) = delete;
        GPUImageView& operator=(GPUImageView&&) = delete;

        [[nodiscard]] VkImageView GetHandle() const noexcept;

    private:
        VkDevice    Device = VK_NULL_HANDLE;
        VkImageView Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
