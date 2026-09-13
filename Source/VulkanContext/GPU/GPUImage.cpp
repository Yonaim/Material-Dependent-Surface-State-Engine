#include "VulkanContext/GPU/GPUImage.h"

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
        : Device(Device), Format(Format), Extent(Extent)
    {
        if (Extent.width == 0 || Extent.height == 0)
        {
            throw std::invalid_argument("GPU image extent must be non-zero.");
        }

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
    }

    GPUImage::~GPUImage()
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
