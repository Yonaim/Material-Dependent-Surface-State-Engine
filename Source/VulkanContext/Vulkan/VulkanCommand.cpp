/**
 * @file VulkanCommand.cpp
 * @brief command pool과 primary·one-time command buffer 관리.
 */

#include "VulkanContext/Vulkan/VulkanCommand.h"

#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS
{
    TVulkanCommand::TVulkanCommand(VkDevice Device, std::uint32_t GraphicsQueueFamily) : Device(Device)
    {
        VkCommandPoolCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        CreateInfo.queueFamilyIndex = GraphicsQueueFamily;

        if (vkCreateCommandPool(Device, &CreateInfo, nullptr, &CommandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan command pool.");
        }
        TLogger::Debug("Vulkan",
                      "Command pool created for graphics queue family " + std::to_string(GraphicsQueueFamily) + ".");
    }

    TVulkanCommand::~TVulkanCommand()
    {
        if (CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(Device, CommandPool, nullptr);
            CommandPool = VK_NULL_HANDLE;
        }
    }

    VkCommandPool TVulkanCommand::GetPool() const noexcept
    {
        return CommandPool;
    }

    std::vector<VkCommandBuffer> TVulkanCommand::AllocatePrimary(std::uint32_t Count) const
    {
        std::vector<VkCommandBuffer> CommandBuffers(Count, VK_NULL_HANDLE);

        VkCommandBufferAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        AllocateInfo.commandPool = CommandPool;
        AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        AllocateInfo.commandBufferCount = Count;

        if (vkAllocateCommandBuffers(Device, &AllocateInfo, CommandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate Vulkan command buffers.");
        }

        TLogger::Verbose("Vulkan", "Allocated " + std::to_string(Count) + " primary command buffer(s).");
        return CommandBuffers;
    }

    VkCommandBuffer TVulkanCommand::BeginSingleTime() const
    {
        VkCommandBuffer             CommandBuffer = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        AllocateInfo.commandPool = CommandPool;
        AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        AllocateInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(Device, &AllocateInfo, &CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate one-time Vulkan command buffer.");
        }

        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
            throw std::runtime_error("Failed to begin one-time Vulkan command buffer.");
        }

        return CommandBuffer;
    }

    void TVulkanCommand::EndSingleTime(VkCommandBuffer CommandBuffer, VkQueue Queue) const
    {
        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
            throw std::runtime_error("Failed to end one-time Vulkan command buffer.");
        }

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;

        if (vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
            throw std::runtime_error("Failed to submit one-time Vulkan command buffer.");
        }

        if (vkQueueWaitIdle(Queue) != VK_SUCCESS)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
            throw std::runtime_error("Failed while waiting for one-time Vulkan command buffer.");
        }

        vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
    }
} // namespace MDSS
