/**
 * @file VulkanQueue.h
 * @brief graphics·present queue family 검색과 queue 접근.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace MDSS
{
    struct TQueueFamilyIndices
    {
        std::optional<std::uint32_t> GraphicsFamily;
        std::optional<std::uint32_t> PresentFamily;

        [[nodiscard]] bool Complete() const noexcept
        {
            return GraphicsFamily.has_value() && PresentFamily.has_value();
        }
    };

    class TVulkanQueue
    {
    public:
        TVulkanQueue(VkPhysicalDevice PhysicalDevice, VkDevice Device, VkSurfaceKHR Surface);

        [[nodiscard]] VkQueue                   GetGraphics() const noexcept;
        [[nodiscard]] VkQueue                   GetPresent() const noexcept;
        [[nodiscard]] const TQueueFamilyIndices& GetFamilyIndices() const noexcept;

        /** @brief graphics/compute와 surface presentation에 필요한 queue family를 찾는다. */
        [[nodiscard]] static TQueueFamilyIndices FindFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);

    private:
        TQueueFamilyIndices FamilyIndices;
        VkQueue            GraphicsQueue = VK_NULL_HANDLE;
        VkQueue            PresentQueue = VK_NULL_HANDLE;
    };
} // namespace MDSS
