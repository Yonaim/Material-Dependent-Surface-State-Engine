/**
 * @file VulkanInstance.h
 * @brief Vulkan instance, validation layer와 debug messenger.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace MDSS::GPU
{
    class FVulkanInstance
    {
    public:
        // Vulkan instance lifecycle
        /**
         * @brief Vulkan instance를 만들고 요청 extension 및 빌드 설정에 따른 validation을 적용한다.
         * @throws std::runtime_error 필수 extension/layer가 없거나 instance 생성이 실패한 경우.
         */
        FVulkanInstance(std::string ApplicationName, const std::vector<const char*>& RequiredExtensions);
        ~FVulkanInstance();

        FVulkanInstance(const FVulkanInstance&) = delete;
        FVulkanInstance& operator=(const FVulkanInstance&) = delete;
        FVulkanInstance(FVulkanInstance&&) = delete;
        FVulkanInstance& operator=(FVulkanInstance&&) = delete;

        [[nodiscard]] VkInstance GetHandle() const noexcept;

    private:
        // Extension and validation layer checks
        static std::vector<const char*> BuildExtensionList(const std::vector<const char*>& RequiredExtensions);
        static void                     ValidateExtensions(const std::vector<const char*>& Extensions);
#if MDSS_ENABLE_VALIDATION
        static void ValidateLayers();
#endif

        VkInstance Instance = VK_NULL_HANDLE;
#if MDSS_ENABLE_VALIDATION
        VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;
#endif
    };
} // namespace MDSS::GPU
