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

        void Recreate(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask);
        void Reset();

        [[nodiscard]] VkImageView GetHandle() const noexcept;

    private:
        void Create(VkImage Image, VkFormat Format, VkImageAspectFlags AspectMask);

        VkDevice    Device = VK_NULL_HANDLE;
        VkImageView Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
