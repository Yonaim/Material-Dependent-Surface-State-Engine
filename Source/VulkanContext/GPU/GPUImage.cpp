#include "VulkanContext/GPU/GPUImage.h"

#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS
{
    GPUImage::GPUImage(VkPhysicalDevice      PhysicalDevice,
                       VkDevice              Device,
                       VkExtent2D            Extent,
                       VkFormat              Format,
                       VkImageTiling         Tiling,
                       VkImageUsageFlags     Usage,
                       VkMemoryPropertyFlags MemoryProperties)
        : Device(Device)
    {
        Create(PhysicalDevice, Extent, Format, Tiling, Usage, MemoryProperties);
    }

    GPUImage::~GPUImage()
    {
        Reset();
    }

    void GPUImage::Recreate(VkPhysicalDevice      PhysicalDevice,
                            VkExtent2D            NewExtent,
                            VkFormat              NewFormat,
                            VkImageTiling         Tiling,
                            VkImageUsageFlags     Usage,
                            VkMemoryPropertyFlags MemoryProperties)
    {
        Reset();
        Create(PhysicalDevice, NewExtent, NewFormat, Tiling, Usage, MemoryProperties);
    }

    void GPUImage::Reset()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyImage(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }

        if (Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(Device, Memory, nullptr);
            Memory = VK_NULL_HANDLE;
        }

        Format = VK_FORMAT_UNDEFINED;
        Extent = {};
    }

    void GPUImage::Create(VkPhysicalDevice      PhysicalDevice,
                          VkExtent2D            NewExtent,
                          VkFormat              NewFormat,
                          VkImageTiling         Tiling,
                          VkImageUsageFlags     Usage,
                          VkMemoryPropertyFlags MemoryProperties)
    {
        if (NewExtent.width == 0 || NewExtent.height == 0)
        {
            throw std::invalid_argument("GPU image extent must be non-zero.");
        }

        Format = NewFormat;
        Extent = NewExtent;

        VkImageCreateInfo ImageInfo{};
        ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ImageInfo.imageType = VK_IMAGE_TYPE_2D;
        ImageInfo.extent.width = Extent.width;
        ImageInfo.extent.height = Extent.height;
        ImageInfo.extent.depth = 1;
        ImageInfo.mipLevels = 1;
        ImageInfo.arrayLayers = 1;
        ImageInfo.format = Format;
        ImageInfo.tiling = Tiling;
        ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageInfo.usage = Usage;
        ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(Device, &ImageInfo, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan image.");
        }

        VkMemoryRequirements MemoryRequirements{};
        vkGetImageMemoryRequirements(Device, Handle, &MemoryRequirements);

        VkMemoryAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        AllocateInfo.allocationSize = MemoryRequirements.size;
        AllocateInfo.memoryTypeIndex =
            FindMemoryType(PhysicalDevice, MemoryRequirements.memoryTypeBits, MemoryProperties);

        if (vkAllocateMemory(Device, &AllocateInfo, nullptr, &Memory) != VK_SUCCESS)
        {
            vkDestroyImage(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate Vulkan image memory.");
        }

        if (vkBindImageMemory(Device, Handle, Memory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(Device, Memory, nullptr);
            vkDestroyImage(Device, Handle, nullptr);
            Memory = VK_NULL_HANDLE;
            Handle = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to bind Vulkan image memory.");
        }

        Logger::Verbose("Vulkan",
                        "GPUImage created (" + std::to_string(Extent.width) + "x" + std::to_string(Extent.height) +
                            ", format=" + std::to_string(static_cast<int>(Format)) + ").");
    }

    VkImage GPUImage::GetHandle() const noexcept
    {
        return Handle;
    }

    VkFormat GPUImage::GetFormat() const noexcept
    {
        return Format;
    }

    VkExtent2D GPUImage::GetExtent() const noexcept
    {
        return Extent;
    }

    std::uint32_t GPUImage::FindMemoryType(VkPhysicalDevice      PhysicalDevice,
                                           std::uint32_t         TypeFilter,
                                           VkMemoryPropertyFlags RequiredProperties)
    {
        VkPhysicalDeviceMemoryProperties Properties{};
        vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &Properties);

        for (std::uint32_t Index = 0; Index < Properties.memoryTypeCount; ++Index)
        {
            const bool bTypeSupported = (TypeFilter & (1U << Index)) != 0;
            const bool bPropertiesSupported =
                (Properties.memoryTypes[Index].propertyFlags & RequiredProperties) == RequiredProperties;

            if (bTypeSupported && bPropertiesSupported)
            {
                return Index;
            }
        }

        throw std::runtime_error("Failed to find a suitable Vulkan memory type for GPU image.");
    }
} // namespace MDSS
