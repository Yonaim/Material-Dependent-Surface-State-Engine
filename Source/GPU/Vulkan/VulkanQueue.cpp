/**
 * @file VulkanQueue.cpp
 * @brief graphics·present queue family 검색과 queue 접근.
 */

#include "GPU/Vulkan/VulkanQueue.h"

#include "Logger/Logger.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace MDSS::GPU
{
// ============================================================
// 00. Queue Lifecycle and Access
// ============================================================
#pragma region 00_Queue_Lifecycle_and_Access
// 선택한 family의 queue를 보관해 command 제출에 제공한다.

    FVulkanQueue::FVulkanQueue(VkPhysicalDevice PhysicalDevice, VkDevice Device, VkSurfaceKHR Surface)
        : FamilyIndices(FindFamilies(PhysicalDevice, Surface))
    {
        if (!FamilyIndices.Complete())
        {
            throw std::runtime_error("Required Vulkan queue families are unavailable.");
        }

        vkGetDeviceQueue(Device, FamilyIndices.GraphicsFamily.value(), 0, &GraphicsQueue);
        vkGetDeviceQueue(Device, FamilyIndices.PresentFamily.value(), 0, &PresentQueue);
        FLogger::Debug("Vulkan", "Queue handles acquired from logical device.");
    }

    VkQueue FVulkanQueue::GetGraphics() const noexcept
    {
        return GraphicsQueue;
    }

    VkQueue FVulkanQueue::GetPresent() const noexcept
    {
        return PresentQueue;
    }

    const FQueueFamilyIndices& FVulkanQueue::GetFamilyIndices() const noexcept
    {
        return FamilyIndices;
    }

#pragma endregion

// ============================================================
// 01. Queue Family Selection
// ============================================================
#pragma region 01_Queue_Family_Selection
// graphics·present 등 필요한 작업을 지원하는 queue family를 찾는다.

    FQueueFamilyIndices FVulkanQueue::FindFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
    {
        FQueueFamilyIndices Indices;

        std::uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies.data());

        for (std::uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
        {
            constexpr VkQueueFlags RequiredGraphicsComputeFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

            if ((QueueFamilies[Index].queueFlags & RequiredGraphicsComputeFlags) == RequiredGraphicsComputeFlags)
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
#pragma endregion
} // namespace MDSS::GPU
