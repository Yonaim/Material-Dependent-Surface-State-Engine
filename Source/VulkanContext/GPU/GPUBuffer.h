#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class GPUBuffer
    {
    public:
        GPUBuffer(VkPhysicalDevice      PhysicalDevice,
                  VkDevice              Device,
                  VkDeviceSize          Size,
                  VkBufferUsageFlags    Usage,
                  VkMemoryPropertyFlags MemoryProperties);
        ~GPUBuffer();

        GPUBuffer(const GPUBuffer&) = delete;
        GPUBuffer& operator=(const GPUBuffer&) = delete;
        GPUBuffer(GPUBuffer&&) = delete;
        GPUBuffer& operator=(GPUBuffer&&) = delete;

        void Upload(const void* Data, VkDeviceSize DataSize, VkDeviceSize Offset = 0) const;

        [[nodiscard]] VkBuffer     GetHandle() const noexcept;
        [[nodiscard]] VkDeviceSize GetSize() const noexcept;

    private:
        static std::uint32_t FindMemoryType(VkPhysicalDevice      PhysicalDevice,
                                            std::uint32_t         TypeFilter,
                                            VkMemoryPropertyFlags RequiredProperties);

        VkDevice              Device = VK_NULL_HANDLE;
        VkBuffer              Handle = VK_NULL_HANDLE;
        VkDeviceMemory        Memory = VK_NULL_HANDLE;
        VkDeviceSize          Size = 0;
        VkMemoryPropertyFlags MemoryProperties = 0;
    };
} // namespace MDSS
