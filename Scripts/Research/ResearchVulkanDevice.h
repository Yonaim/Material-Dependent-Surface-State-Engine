#pragma once
#include <vulkan/vulkan.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
class TVulkanUnavailable : public std::runtime_error { public: using std::runtime_error::runtime_error; };
    class TVulkanTestDevice final
    {
    public:
        TVulkanTestDevice()
        {
            VkApplicationInfo ApplicationInfo{};
            ApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            ApplicationInfo.pApplicationName = "MDSS Surface GPU resource tests";
            ApplicationInfo.apiVersion = VK_API_VERSION_1_2;

            VkInstanceCreateInfo InstanceInfo{};
            InstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            InstanceInfo.pApplicationInfo = &ApplicationInfo;
#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
            const char* PortabilityExtension = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            InstanceInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            InstanceInfo.enabledExtensionCount = 1;
            InstanceInfo.ppEnabledExtensionNames = &PortabilityExtension;
#endif
            if (vkCreateInstance(&InstanceInfo, nullptr, &Instance) != VK_SUCCESS)
            {
                throw TVulkanUnavailable("Could not create Vulkan instance for GPU resource tests.");
            }

            std::uint32_t PhysicalDeviceCount = 0;
            if (vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, nullptr) != VK_SUCCESS ||
                PhysicalDeviceCount == 0)
            {
                throw TVulkanUnavailable("No Vulkan physical device is available for GPU resource tests.");
            }
            std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDeviceCount);
            if (vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, PhysicalDevices.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not enumerate Vulkan physical devices.");
            }

            for (VkPhysicalDevice Candidate : PhysicalDevices)
            {
                std::uint32_t QueueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueFamilyCount, nullptr);
                std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueFamilyCount, QueueFamilies.data());
                for (std::uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
                {
                    if ((QueueFamilies[Index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
                    {
                        PhysicalDevice = Candidate;
                        QueueFamily = Index;
                        break;
                    }
                }
                if (PhysicalDevice != VK_NULL_HANDLE)
                {
                    break;
                }
            }
            if (PhysicalDevice == VK_NULL_HANDLE)
            {
                throw TVulkanUnavailable("No compute-capable Vulkan queue is available for GPU resource tests.");
            }

            constexpr float         QueuePriority = 1.0F;
            VkDeviceQueueCreateInfo QueueInfo{};
            QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueInfo.queueFamilyIndex = QueueFamily;
            QueueInfo.queueCount = 1;
            QueueInfo.pQueuePriorities = &QueuePriority;

            std::vector<const char*> DeviceExtensions;
            std::uint32_t            ExtensionCount = 0;
            vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
            std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
            vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());
            for (const VkExtensionProperties& Extension : AvailableExtensions)
            {
                if (std::string(Extension.extensionName) == "VK_KHR_portability_subset")
                {
                    DeviceExtensions.push_back("VK_KHR_portability_subset");
                    break;
                }
            }

            VkDeviceCreateInfo DeviceInfo{};
            DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            DeviceInfo.queueCreateInfoCount = 1;
            DeviceInfo.pQueueCreateInfos = &QueueInfo;
            DeviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
            DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.data();
            if (vkCreateDevice(PhysicalDevice, &DeviceInfo, nullptr, &Device) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create Vulkan device for GPU resource tests.");
            }

            vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);
            VkCommandPoolCreateInfo CommandPoolInfo{};
            CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            CommandPoolInfo.flags =
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            CommandPoolInfo.queueFamilyIndex = QueueFamily;
            if (vkCreateCommandPool(Device, &CommandPoolInfo, nullptr, &CommandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create Vulkan command pool for GPU resource tests.");
            }
        }

        ~TVulkanTestDevice()
        {
            if (Device != VK_NULL_HANDLE)
            {
                vkDeviceWaitIdle(Device);
                if (CommandPool != VK_NULL_HANDLE)
                {
                    vkDestroyCommandPool(Device, CommandPool, nullptr);
                }
                vkDestroyDevice(Device, nullptr);
            }
            if (Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(Instance, nullptr);
            }
        }

        TVulkanTestDevice(const TVulkanTestDevice&) = delete;
        TVulkanTestDevice& operator=(const TVulkanTestDevice&) = delete;

        [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept
        {
            return PhysicalDevice;
        }
        [[nodiscard]] VkDevice GetDevice() const noexcept
        {
            return Device;
        }

        void Execute(const std::function<void(VkCommandBuffer)>& Record)
        {
            VkCommandBufferAllocateInfo AllocateInfo{};
            AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            AllocateInfo.commandPool = CommandPool;
            AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            AllocateInfo.commandBufferCount = 1;
            VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
            if (vkAllocateCommandBuffers(Device, &AllocateInfo, &CommandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not allocate Vulkan command buffer for GPU resource tests.");
            }

            VkCommandBufferBeginInfo BeginInfo{};
            BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not begin Vulkan command buffer for GPU resource tests.");
            }
            Record(CommandBuffer);
            if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not finish Vulkan command buffer for GPU resource tests.");
            }

            VkSubmitInfo SubmitInfo{};
            SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            SubmitInfo.commandBufferCount = 1;
            SubmitInfo.pCommandBuffers = &CommandBuffer;
            if (vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS ||
                vkQueueWaitIdle(Queue) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not submit Vulkan command buffer for GPU resource tests.");
            }
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
        }

    private:
        VkInstance       Instance = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        std::uint32_t    QueueFamily = 0;
        VkDevice         Device = VK_NULL_HANDLE;
        VkQueue          Queue = VK_NULL_HANDLE;
        VkCommandPool    CommandPool = VK_NULL_HANDLE;
    };
