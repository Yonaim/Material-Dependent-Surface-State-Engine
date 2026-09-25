/**
 * @file VulkanQueue.cpp
 * @brief graphics·present queue family 검색과 queue 접근.
 */

#include "VulkanContext/Vulkan/VulkanQueue.h"

#include "Logger/Logger.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace MDSS
{
    VulkanQueue::VulkanQueue(VkPhysicalDevice PhysicalDevice, VkDevice Device, VkSurfaceKHR Surface)
        : FamilyIndices(FindFamilies(PhysicalDevice, Surface))
    {
        if (!FamilyIndices.Complete())
        {
            throw std::runtime_error("Required Vulkan queue families are unavailable.");
        }

        vkGetDeviceQueue(Device, FamilyIndices.GraphicsFamily.value(), 0, &GraphicsQueue);
        vkGetDeviceQueue(Device, FamilyIndices.PresentFamily.value(), 0, &PresentQueue);
        Logger::Debug("Vulkan", "Queue handles acquired from logical device.");
    }

    VkQueue VulkanQueue::GetGraphics() const noexcept
    {
        return GraphicsQueue;
    }

    VkQueue VulkanQueue::GetPresent() const noexcept
    {
        return PresentQueue;
    }

    const QueueFamilyIndices& VulkanQueue::GetFamilyIndices() const noexcept
    {
        return FamilyIndices;
    }

    QueueFamilyIndices VulkanQueue::FindFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
    {
        QueueFamilyIndices Indices;

        std::uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies.data());

        for (std::uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
        {
            if ((QueueFamilies[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                Indices.GraphicsFamily = Index;
            }

            VkBool32 bPresentSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Index, Surface, &bPresentSupported);
            if (bPresentSupported == VK_TRUE)
            {
                Indices.PresentFamily = Index;
            }

            if (Indices.Complete())
            {
                break;
            }
        }

        return Indices;
    }
} // namespace MDSS
