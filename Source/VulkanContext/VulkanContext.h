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
    class TWindow;

    class TVulkanContext
    {
    public:
        /** @brief TWindow surface와 Vulkan 실행에 필요한 핵심 자원을 순서대로 초기화한다. */
        explicit TVulkanContext(const TWindow& TWindow);
        ~TVulkanContext();

        TVulkanContext(const TVulkanContext&) = delete;
        TVulkanContext& operator=(const TVulkanContext&) = delete;
        TVulkanContext(TVulkanContext&&) = delete;
        TVulkanContext& operator=(TVulkanContext&&) = delete;

        [[nodiscard]] VkInstance           GetInstance() const noexcept;
        [[nodiscard]] VkSurfaceKHR         GetSurface() const noexcept;
        [[nodiscard]] VkPhysicalDevice     GetPhysicalDevice() const noexcept;
        [[nodiscard]] VkDevice             GetDevice() const noexcept;
        [[nodiscard]] bool                 SupportsGeometryShader() const noexcept;
        [[nodiscard]] const TVulkanQueue&   GetQueues() const noexcept;
        [[nodiscard]] const TVulkanCommand& GetCommands() const noexcept;

    private:
        /** @brief GLFW가 현재 platform에서 요구하는 Vulkan instance extension을 반환한다. */
        static std::vector<const char*> RequiredInstanceExtensions();
        /** @brief GLFW native window에 대응하는 Vulkan presentation surface를 생성한다. */
        static VkSurfaceKHR CreateSurface(VkInstance Instance, const TWindow& TWindow);

        TVulkanInstance Instance;
        VkSurfaceKHR   Surface = VK_NULL_HANDLE;
        TVulkanDevice   Device;
        TVulkanQueue    Queues;
        TVulkanCommand  Commands;
    };
} // namespace MDSS
