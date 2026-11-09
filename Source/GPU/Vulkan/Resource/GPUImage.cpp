/**
 * @file GPUImage.cpp
 * @brief Vulkan image와 device memory의 생성·재생성·해제.
 */

#include "GPU/Vulkan/Resource/GPUImage.h"

#include "GPU/Vulkan/Resource/GPUResourceMemoryStats.h"
#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS::GPU
{
// ============================================================
// 00. Image Lifecycle and Creation
// ============================================================
#pragma region 00_Image_Lifecycle_and_Creation

    FGPUImage::FGPUImage(VkPhysicalDevice      PhysicalDevice,
                         VkDevice              Device,
                         VkExtent2D            Extent,
                         VkFormat              Format,
                         VkImageTiling         Tiling,
                         VkImageUsageFlags     Usage,
                         VkMemoryPropertyFlags MemoryProperties,
                         std::uint32_t         ArrayLayers)
        : Device(Device)
    {
        Create(PhysicalDevice, Extent, Format, Tiling, Usage, MemoryProperties, ArrayLayers);
    }

    FGPUImage::~FGPUImage()
    {
        Reset();
    }

    void FGPUImage::Recreate(VkPhysicalDevice      PhysicalDevice,
                             VkExtent2D            NewExtent,
                             VkFormat              NewFormat,
                             VkImageTiling         Tiling,
                             VkImageUsageFlags     Usage,
                             VkMemoryPropertyFlags MemoryProperties,
                             std::uint32_t         ArrayLayers)
    {
        Reset();
        Create(PhysicalDevice, NewExtent, NewFormat, Tiling, Usage, MemoryProperties, ArrayLayers);
    }

    void FGPUImage::Reset()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyImage(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }

        if (Memory != VK_NULL_HANDLE)
        {
            FGPUResourceMemoryStats::ImageFreed(AllocationSize);
            vkFreeMemory(Device, Memory, nullptr);
            Memory = VK_NULL_HANDLE;
        }

        AllocationSize = 0;
        Format = VK_FORMAT_UNDEFINED;
        Extent = {};
    }

    void FGPUImage::Create(VkPhysicalDevice      PhysicalDevice,
                           VkExtent2D            NewExtent,
                           VkFormat              NewFormat,
                           VkImageTiling         Tiling,
                           VkImageUsageFlags     Usage,
                           VkMemoryPropertyFlags MemoryProperties,
                           std::uint32_t         ArrayLayers)
    {
        if (NewExtent.width == 0 || NewExtent.height == 0 || ArrayLayers == 0)
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
        ImageInfo.arrayLayers = ArrayLayers;
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
        AllocationSize = MemoryRequirements.size;
        AllocateInfo.memoryTypeIndex =
            FindMemoryType(PhysicalDevice, MemoryRequirements.memoryTypeBits, MemoryProperties);

        if (vkAllocateMemory(Device, &AllocateInfo, nullptr, &Memory) != VK_SUCCESS)
        {
            vkDestroyImage(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
            AllocationSize = 0;
            throw std::runtime_error("Failed to allocate Vulkan image memory.");
        }

        if (vkBindImageMemory(Device, Handle, Memory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(Device, Memory, nullptr);
            vkDestroyImage(Device, Handle, nullptr);
            Memory = VK_NULL_HANDLE;
            Handle = VK_NULL_HANDLE;
            AllocationSize = 0;
            throw std::runtime_error("Failed to bind Vulkan image memory.");
        }

        FGPUResourceMemoryStats::ImageAllocated(AllocationSize);

        FLogger::Verbose("Vulkan",
                         "TGPUImage created (" + std::to_string(Extent.width) + "x" + std::to_string(Extent.height) +
                             ", format=" + std::to_string(static_cast<int>(Format)) + ").");
    }

#pragma endregion

// ============================================================
// 01. Image Accessors
// ============================================================
#pragma region 01_Image_Accessors

    VkImage FGPUImage::GetHandle() const noexcept
    {
        return Handle;
    }

    VkFormat FGPUImage::GetFormat() const noexcept
    {
        return Format;
    }

    VkExtent2D FGPUImage::GetExtent() const noexcept
    {
        return Extent;
    }

#pragma endregion

// ============================================================
// 02. Memory Type Selection
// ============================================================
#pragma region 02_Memory_Type_Selection

    std::uint32_t FGPUImage::FindMemoryType(VkPhysicalDevice      PhysicalDevice,
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
#pragma endregion
} // namespace MDSS::GPU
