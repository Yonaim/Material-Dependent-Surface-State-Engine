/**
 * @file VulkanCommand.h
 * @brief command pool과 primary·one-time command buffer 관리.
 */

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
        [[nodiscard]] VkCommandBuffer              BeginSingleTime() const;
        void                                       EndSingleTime(VkCommandBuffer CommandBuffer, VkQueue Queue) const;

    private:
        VkDevice      Device = VK_NULL_HANDLE;
        VkCommandPool CommandPool = VK_NULL_HANDLE;
    };
} // namespace MDSS
