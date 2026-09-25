/**
 * @file VulkanDevice.cpp
 * @brief 물리 장치 선택과 논리 장치 생성.
 */

#include "VulkanContext/Vulkan/VulkanDevice.h"

#include "Logger/Logger.h"
#include "VulkanContext/Vulkan/VulkanQueue.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace MDSS
{
    namespace
    {
        constexpr const char* PortabilitySubsetExtension = "VK_KHR_portability_subset";
    }

    VulkanDevice::VulkanDevice(VkInstance Instance, VkSurfaceKHR Surface)
    {
        std::uint32_t PhysicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, nullptr);

        if (PhysicalDeviceCount == 0)
        {
            throw std::runtime_error("No Vulkan-capable GPU was found.");
        }

        std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDeviceCount);
        vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, PhysicalDevices.data());
        Logger::Debug("Vulkan", "Enumerated " + std::to_string(PhysicalDeviceCount) + " physical device(s).");

        const auto Selected =
            std::find_if(PhysicalDevices.begin(),
                         PhysicalDevices.end(),
                         [Surface](VkPhysicalDevice Candidate) { return IsDeviceSuitable(Candidate, Surface); });

        if (Selected == PhysicalDevices.end())
        {
            throw std::runtime_error("No GPU satisfies the Vulkan 1.2, graphics, present, and swapchain requirements.");
        }

        PhysicalDevice = *Selected;

        const QueueFamilyIndices QueueFamilies = VulkanQueue::FindFamilies(PhysicalDevice, Surface);
        std::set<std::uint32_t>  UniqueQueueFamilies = {QueueFamilies.GraphicsFamily.value(),
                                                        QueueFamilies.PresentFamily.value()};

        constexpr float                      QueuePriority = 1.0F;
        std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
        QueueCreateInfos.reserve(UniqueQueueFamilies.size());

        for (const std::uint32_t QueueFamily : UniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo{};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = QueueFamily;
            QueueCreateInfo.queueCount = 1;
            QueueCreateInfo.pQueuePriorities = &QueuePriority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }

        const std::vector<const char*> DeviceExtensions = BuildDeviceExtensions(PhysicalDevice);

        VkPhysicalDeviceFeatures SupportedFeatures{};
        vkGetPhysicalDeviceFeatures(PhysicalDevice, &SupportedFeatures);

        VkPhysicalDeviceFeatures Features{};
        Features.geometryShader = SupportedFeatures.geometryShader;
        bGeometryShaderSupported = SupportedFeatures.geometryShader == VK_TRUE;

        VkDeviceCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CreateInfo.queueCreateInfoCount = static_cast<std::uint32_t>(QueueCreateInfos.size());
        CreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
        CreateInfo.pEnabledFeatures = &Features;
        CreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
        CreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();

        if (vkCreateDevice(PhysicalDevice, &CreateInfo, nullptr, &Device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan logical device.");
        }

        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);

        Logger::Info("Vulkan", std::string("Physical device selected: ") + Properties.deviceName + ".");
        Logger::Debug("Vulkan",
                      "GPU Vulkan API version=" + std::to_string(VK_API_VERSION_MAJOR(Properties.apiVersion)) + "." +
                          std::to_string(VK_API_VERSION_MINOR(Properties.apiVersion)) + "." +
                          std::to_string(VK_API_VERSION_PATCH(Properties.apiVersion)) + ".");
        Logger::Info("Vulkan",
                     "Logical device created with " + std::to_string(DeviceExtensions.size()) +
                         " required device extension(s).");
        Logger::Info("Vulkan",
                     std::string("Geometry shader support: ") +
                         (bGeometryShaderSupported ? "yes." : "no (optional stage will require fallback)."));
    }

    VulkanDevice::~VulkanDevice()
    {
        if (Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(Device, nullptr);
            Device = VK_NULL_HANDLE;
        }
    }

    VkPhysicalDevice VulkanDevice::GetPhysicalHandle() const noexcept
    {
        return PhysicalDevice;
    }

    VkDevice VulkanDevice::GetHandle() const noexcept
    {
        return Device;
    }

    bool VulkanDevice::SupportsGeometryShader() const noexcept
    {
        return bGeometryShaderSupported;
    }

    bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
    {
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);

        if (VK_API_VERSION_MAJOR(Properties.apiVersion) < 1 ||
            (VK_API_VERSION_MAJOR(Properties.apiVersion) == 1 && VK_API_VERSION_MINOR(Properties.apiVersion) < 2))
        {
            return false;
        }

        const QueueFamilyIndices QueueFamilies = VulkanQueue::FindFamilies(PhysicalDevice, Surface);

        return QueueFamilies.Complete() && SupportsRequiredExtensions(PhysicalDevice) &&
               HasAdequateSwapchainSupport(PhysicalDevice, Surface);
    }

    bool VulkanDevice::SupportsRequiredExtensions(VkPhysicalDevice PhysicalDevice)
    {
        return HasDeviceExtension(PhysicalDevice, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    bool VulkanDevice::HasAdequateSwapchainSupport(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
    {
        std::uint32_t FormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);

        std::uint32_t PresentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr);

        return FormatCount > 0 && PresentModeCount > 0;
    }

    bool VulkanDevice::HasDeviceExtension(VkPhysicalDevice PhysicalDevice, const char* ExtensionName)
    {
        std::uint32_t ExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);

        std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
        vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

        return std::any_of(AvailableExtensions.begin(),
                           AvailableExtensions.end(),
                           [ExtensionName](const VkExtensionProperties& Extension)
                           { return std::strcmp(Extension.extensionName, ExtensionName) == 0; });
    }

    std::vector<const char*> VulkanDevice::BuildDeviceExtensions(VkPhysicalDevice PhysicalDevice)
    {
        std::vector<const char*> Extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // MoltenVK exposes this extension. The Vulkan portability specification
        // requires applications to enable it when the selected device advertises it.
        if (HasDeviceExtension(PhysicalDevice, PortabilitySubsetExtension))
        {
            Extensions.push_back(PortabilitySubsetExtension);
        }

        return Extensions;
    }
} // namespace MDSS
