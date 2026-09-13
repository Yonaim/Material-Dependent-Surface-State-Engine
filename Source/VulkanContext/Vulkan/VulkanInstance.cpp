#include "VulkanContext/Vulkan/VulkanInstance.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace MDSS
{
    VulkanInstance::VulkanInstance(std::string ApplicationName, const std::vector<const char*>& RequiredExtensions)
    {
        const std::vector<const char*> Extensions = BuildExtensionList(RequiredExtensions);
        ValidateExtensions(Extensions);

        VkApplicationInfo ApplicationInfo{};
        ApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        ApplicationInfo.pApplicationName = ApplicationName.c_str();
        ApplicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        ApplicationInfo.pEngineName = "MDSSP Engine";
        ApplicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        ApplicationInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        CreateInfo.pApplicationInfo = &ApplicationInfo;
        CreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(Extensions.size());
        CreateInfo.ppEnabledExtensionNames = Extensions.data();

#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
        CreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        const VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &Instance);
        if (Result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance (VkResult: " +
                                     std::to_string(static_cast<std::int32_t>(Result)) + ").");
        }

        std::cout << "[Vulkan] Instance created (API 1.2).\n";
    }

    VulkanInstance::~VulkanInstance()
    {
        if (Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(Instance, nullptr);
            Instance = VK_NULL_HANDLE;
        }
    }

    VkInstance VulkanInstance::GetHandle() const noexcept
    {
        return Instance;
    }

    std::vector<const char*> VulkanInstance::BuildExtensionList(const std::vector<const char*>& RequiredExtensions)
    {
        std::vector<const char*> Extensions = RequiredExtensions;

#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
        const auto AlreadyPresent =
            std::find_if(Extensions.begin(),
                         Extensions.end(),
                         [](const char* Extension)
                         { return std::strcmp(Extension, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0; });

        if (AlreadyPresent == Extensions.end())
        {
            Extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }
#endif

        return Extensions;
    }

    void VulkanInstance::ValidateExtensions(const std::vector<const char*>& Extensions)
    {
        std::uint32_t AvailableCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &AvailableCount, nullptr);

        std::vector<VkExtensionProperties> AvailableExtensions(AvailableCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &AvailableCount, AvailableExtensions.data());

        for (const char* RequiredExtension : Extensions)
        {
            const bool bFound = std::any_of(AvailableExtensions.begin(),
                                            AvailableExtensions.end(),
                                            [RequiredExtension](const VkExtensionProperties& Available)
                                            { return std::strcmp(RequiredExtension, Available.extensionName) == 0; });

            if (!bFound)
            {
                throw std::runtime_error(std::string("Required Vulkan instance extension is unavailable: ") +
                                         RequiredExtension);
            }
        }
    }
} // namespace MDSS
