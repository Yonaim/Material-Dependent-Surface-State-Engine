#include "VulkanContext/GPU/GPUBuffer.h"

#include <cstring>
#include <stdexcept>

namespace MDSS
{
    GPUBuffer::GPUBuffer(VkPhysicalDevice      PhysicalDevice,
                         VkDevice              Device,
                         VkDeviceSize          Size,
                         VkBufferUsageFlags    Usage,
                         VkMemoryPropertyFlags MemoryProperties)
        : Device(Device), Size(Size), MemoryProperties(MemoryProperties)
    {
        if (Size == 0)
        {
            throw std::invalid_argument("GPU buffer size must be greater than zero.");
        }

        VkBufferCreateInfo BufferInfo{};
        BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        BufferInfo.size = Size;
        BufferInfo.usage = Usage;
        BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(Device, &BufferInfo, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan buffer.");
        }

        try
        {
            VkMemoryRequirements MemoryRequirements{};
            vkGetBufferMemoryRequirements(Device, Handle, &MemoryRequirements);

            VkMemoryAllocateInfo AllocateInfo{};
            AllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            AllocateInfo.allocationSize = MemoryRequirements.size;
            AllocateInfo.memoryTypeIndex =
                FindMemoryType(PhysicalDevice, MemoryRequirements.memoryTypeBits, MemoryProperties);

            if (vkAllocateMemory(Device, &AllocateInfo, nullptr, &Memory) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate Vulkan buffer memory.");
            }

            if (vkBindBufferMemory(Device, Handle, Memory, 0) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to bind Vulkan buffer memory.");
            }
        }
        catch (...)
        {
            if (Memory != VK_NULL_HANDLE)
            {
                vkFreeMemory(Device, Memory, nullptr);
                Memory = VK_NULL_HANDLE;
            }
            if (Handle != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(Device, Handle, nullptr);
                Handle = VK_NULL_HANDLE;
            }
            throw;
        }
    }

    GPUBuffer::~GPUBuffer()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }

        if (Memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(Device, Memory, nullptr);
            Memory = VK_NULL_HANDLE;
        }
    }

    void GPUBuffer::Upload(const void* Data, VkDeviceSize DataSize, VkDeviceSize Offset) const
    {
        if (Data == nullptr)
        {
            throw std::invalid_argument("GPU buffer upload data must not be null.");
        }

        if ((MemoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
        {
            throw std::runtime_error("GPU buffer memory is not host visible.");
        }

        if ((MemoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            throw std::runtime_error("GPUBuffer::Upload currently requires host-coherent memory.");
        }

        if (Offset > Size || DataSize > (Size - Offset))
        {
            throw std::out_of_range("GPU buffer upload range exceeds the buffer size.");
        }

        void* MappedMemory = nullptr;
        if (vkMapMemory(Device, Memory, Offset, DataSize, 0, &MappedMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map Vulkan buffer memory.");
        }

        std::memcpy(MappedMemory, Data, static_cast<std::size_t>(DataSize));
        vkUnmapMemory(Device, Memory);
    }

    VkBuffer GPUBuffer::GetHandle() const noexcept
    {
        return Handle;
    }

    VkDeviceSize GPUBuffer::GetSize() const noexcept
    {
        return Size;
    }

    std::uint32_t GPUBuffer::FindMemoryType(VkPhysicalDevice      PhysicalDevice,
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

        throw std::runtime_error("Failed to find a suitable Vulkan memory type for GPU buffer.");
    }
} // namespace MDSS
