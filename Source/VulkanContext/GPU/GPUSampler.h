#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class GPUSampler
    {
    public:
        explicit GPUSampler(VkDevice Device);
        ~GPUSampler();

        GPUSampler(const GPUSampler&) = delete;
        GPUSampler& operator=(const GPUSampler&) = delete;
        GPUSampler(GPUSampler&&) = delete;
        GPUSampler& operator=(GPUSampler&&) = delete;

        [[nodiscard]] VkSampler GetHandle() const noexcept;

    private:
        VkDevice  Device = VK_NULL_HANDLE;
        VkSampler Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
