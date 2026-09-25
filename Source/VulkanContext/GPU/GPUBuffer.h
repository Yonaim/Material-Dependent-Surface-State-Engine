/**
 * @file GPUBuffer.h
 * @brief Vulkan buffer와 device memory의 생성·갱신·해제.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class GPUBuffer
    {
    public:
        /**
         * @brief 지정한 usage와 memory property로 Vulkan buffer와 memory를 생성한다.
         * @throws std::runtime_error buffer 또는 memory 생성에 실패한 경우.
         */
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

        /**
         * @brief host-visible buffer memory에 바이트 범위를 복사한다.
         * @param Data 복사할 데이터 주소.
         * @param DataSize 복사할 byte 수.
         * @param Offset buffer 시작점으로부터의 byte offset.
         * @throws std::runtime_error mapping 실패 시 발생한다.
         */
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
