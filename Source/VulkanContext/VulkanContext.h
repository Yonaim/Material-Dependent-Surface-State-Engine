/**
 * @file VulkanContext.h
 * @brief Vulkan instance·device·queue·command 자원 통합 수명 주기.
 */

#pragma once

#include "VulkanContext/Vulkan/VulkanCommand.h"
#include "VulkanContext/Vulkan/VulkanDevice.h"
#include "VulkanContext/Vulkan/VulkanInstance.h"
#include "VulkanContext/Vulkan/VulkanQueue.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class Window;

    class VulkanContext
    {
    public:
        /** @brief Window surface와 Vulkan 실행에 필요한 핵심 자원을 순서대로 초기화한다. */
        explicit VulkanContext(const Window& Window);
        ~VulkanContext();

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        [[nodiscard]] VkInstance           GetInstance() const noexcept;
        [[nodiscard]] VkSurfaceKHR         GetSurface() const noexcept;
        [[nodiscard]] VkPhysicalDevice     GetPhysicalDevice() const noexcept;
        [[nodiscard]] VkDevice             GetDevice() const noexcept;
        [[nodiscard]] bool                 SupportsGeometryShader() const noexcept;
        [[nodiscard]] const VulkanQueue&   GetQueues() const noexcept;
        [[nodiscard]] const VulkanCommand& GetCommands() const noexcept;

    private:
        /** @brief GLFW가 현재 platform에서 요구하는 Vulkan instance extension을 반환한다. */
        static std::vector<const char*> RequiredInstanceExtensions();
        /** @brief GLFW native window에 대응하는 Vulkan presentation surface를 생성한다. */
        static VkSurfaceKHR CreateSurface(VkInstance Instance, const Window& Window);

        VulkanInstance Instance;
        VkSurfaceKHR   Surface = VK_NULL_HANDLE;
        VulkanDevice   Device;
        VulkanQueue    Queues;
        VulkanCommand  Commands;
    };
} // namespace MDSS
