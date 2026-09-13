#include "VulkanContext/Vulkan/VulkanCommand.h"

#include <stdexcept>

namespace MDSS
{
    VulkanCommand::VulkanCommand(VkDevice Device, std::uint32_t GraphicsQueueFamily) : Device(Device)
    {
        VkCommandPoolCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        CreateInfo.queueFamilyIndex = GraphicsQueueFamily;

        if (vkCreateCommandPool(Device, &CreateInfo, nullptr, &CommandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan command pool.");
        }
    }

    VulkanCommand::~VulkanCommand()
    {
        if (CommandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(Device, CommandPool, nullptr);
            CommandPool = VK_NULL_HANDLE;
        }
    }

    VkCommandPool VulkanCommand::GetPool() const noexcept
    {
        return CommandPool;
    }

    std::vector<VkCommandBuffer> VulkanCommand::AllocatePrimary(std::uint32_t Count) const
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

        return CommandBuffers;
    }
} // namespace MDSS
