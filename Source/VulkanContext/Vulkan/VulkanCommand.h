#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace MDSS
{
    class VulkanCommand
    {
    public:
        VulkanCommand(VkDevice Device, std::uint32_t GraphicsQueueFamily);
        ~VulkanCommand();

        VulkanCommand(const VulkanCommand&) = delete;
        VulkanCommand& operator=(const VulkanCommand&) = delete;
        VulkanCommand(VulkanCommand&&) = delete;
        VulkanCommand& operator=(VulkanCommand&&) = delete;

        [[nodiscard]] VkCommandPool                GetPool() const noexcept;
        [[nodiscard]] std::vector<VkCommandBuffer> AllocatePrimary(std::uint32_t Count) const;

    private:
        VkDevice      Device = VK_NULL_HANDLE;
        VkCommandPool CommandPool = VK_NULL_HANDLE;
    };
} // namespace MDSS
