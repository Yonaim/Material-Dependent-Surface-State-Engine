#include "VulkanContext/Vulkan/VulkanInstance.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace MDSS
{
#if MDSSP_ENABLE_VALIDATION
    namespace
    {
        constexpr const char* ValidationLayer = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                                     VkDebugUtilsMessageTypeFlagsEXT,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* CallbackData,
                                                     void*)
        {
            std::cerr << "[Vulkan Validation] " << CallbackData->pMessage << '\n';
            return VK_FALSE;
        }

        VkDebugUtilsMessengerCreateInfoEXT DebugMessengerCreateInfo()
        {
            VkDebugUtilsMessengerCreateInfoEXT CreateInfo{};
            CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            CreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            CreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            CreateInfo.pfnUserCallback = DebugCallback;
            return CreateInfo;
        }
    } // namespace
#endif

    VulkanInstance::VulkanInstance(std::string ApplicationName, const std::vector<const char*>& RequiredExtensions)
    {
        const std::vector<const char*> Extensions = BuildExtensionList(RequiredExtensions);
        ValidateExtensions(Extensions);
#if MDSSP_ENABLE_VALIDATION
        ValidateLayers();
#endif

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

#if MDSSP_ENABLE_VALIDATION
        const VkDebugUtilsMessengerCreateInfoEXT MessengerInfo = DebugMessengerCreateInfo();
        constexpr std::array                     EnabledFeatures = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
                                                                    VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};
        VkValidationFeaturesEXT                  ValidationFeatures{};
        ValidationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        ValidationFeatures.pNext = &MessengerInfo;
        ValidationFeatures.enabledValidationFeatureCount = static_cast<std::uint32_t>(EnabledFeatures.size());
        ValidationFeatures.pEnabledValidationFeatures = EnabledFeatures.data();

        CreateInfo.enabledLayerCount = 1;
        CreateInfo.ppEnabledLayerNames = &ValidationLayer;
        CreateInfo.pNext = &ValidationFeatures;
#endif

#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
        CreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        const VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &Instance);
        if (Result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance (VkResult: " +
                                     std::to_string(static_cast<std::int32_t>(Result)) + ").");
        }

#if MDSSP_ENABLE_VALIDATION
        const auto CreateDebugMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
        if (CreateDebugMessenger == nullptr ||
            CreateDebugMessenger(Instance, &MessengerInfo, nullptr, &DebugMessenger) != VK_SUCCESS)
        {
            vkDestroyInstance(Instance, nullptr);
            Instance = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create Vulkan debug messenger.");
        }
#endif

        std::cout << "[Vulkan] Instance created (API 1.2).\n";
#if MDSSP_ENABLE_VALIDATION
        std::cout << "[Vulkan] Validation enabled (core, synchronization, best practices).\n";
#endif
    }

    VulkanInstance::~VulkanInstance()
    {
#if MDSSP_ENABLE_VALIDATION
        if (DebugMessenger != VK_NULL_HANDLE)
        {
            const auto DestroyDebugMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(Instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (DestroyDebugMessenger != nullptr)
            {
                DestroyDebugMessenger(Instance, DebugMessenger, nullptr);
            }
            DebugMessenger = VK_NULL_HANDLE;
        }
#endif

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

#if MDSSP_ENABLE_VALIDATION
        Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

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

#if MDSSP_ENABLE_VALIDATION
    void VulkanInstance::ValidateLayers()
    {
        std::uint32_t LayerCount = 0;
        vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

        std::vector<VkLayerProperties> AvailableLayers(LayerCount);
        vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

        const bool bFound = std::any_of(AvailableLayers.begin(),
                                        AvailableLayers.end(),
                                        [](const VkLayerProperties& Layer)
                                        { return std::strcmp(Layer.layerName, ValidationLayer) == 0; });
        if (!bFound)
        {
            throw std::runtime_error("Required Vulkan validation layer is unavailable: " +
                                     std::string(ValidationLayer));
        }
    }

#endif
} // namespace MDSS
