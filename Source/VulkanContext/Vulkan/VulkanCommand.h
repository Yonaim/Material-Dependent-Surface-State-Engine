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
    class TVulkanCommand
    {
    public:
        TVulkanCommand(VkDevice Device, std::uint32_t GraphicsQueueFamily);
        ~TVulkanCommand();

        TVulkanCommand(const TVulkanCommand&) = delete;
        TVulkanCommand& operator=(const TVulkanCommand&) = delete;
        TVulkanCommand(TVulkanCommand&&) = delete;
        TVulkanCommand& operator=(TVulkanCommand&&) = delete;

        [[nodiscard]] VkCommandPool GetPool() const noexcept;
        /** @brief graphics command pool에서 primary command buffer를 할당한다. */
        [[nodiscard]] std::vector<VkCommandBuffer> AllocatePrimary(std::uint32_t Count) const;
        /** @brief one-time submit 용도로 기록을 시작한 command buffer를 할당한다. */
        [[nodiscard]] VkCommandBuffer BeginSingleTime() const;
        /** @brief 기록한 buffer를 제출하고 queue가 idle이 될 때까지 기다린 뒤 해제한다. */
        void EndSingleTime(VkCommandBuffer CommandBuffer, VkQueue Queue) const;

    private:
        VkDevice      Device = VK_NULL_HANDLE;
        VkCommandPool CommandPool = VK_NULL_HANDLE;
    };
} // namespace MDSS
