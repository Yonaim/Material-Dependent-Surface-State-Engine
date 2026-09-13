#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace MDSS
{
    class VulkanInstance
    {
    public:
        VulkanInstance(std::string ApplicationName, const std::vector<const char*>& RequiredExtensions);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;
        VulkanInstance(VulkanInstance&&) = delete;
        VulkanInstance& operator=(VulkanInstance&&) = delete;

        [[nodiscard]] VkInstance GetHandle() const noexcept;

    private:
        static std::vector<const char*> BuildExtensionList(const std::vector<const char*>& RequiredExtensions);
        static void                     ValidateExtensions(const std::vector<const char*>& Extensions);
#if MDSSP_ENABLE_VALIDATION
        static void ValidateLayers();
#endif

        VkInstance               Instance = VK_NULL_HANDLE;
#if MDSSP_ENABLE_VALIDATION
        VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;
#endif
    };
} // namespace MDSS
