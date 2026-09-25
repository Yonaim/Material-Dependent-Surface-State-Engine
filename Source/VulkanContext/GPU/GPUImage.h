/**
 * @file GPUImage.h
 * @brief Vulkan image와 device memory의 생성·재생성·해제.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class TGPUImage
    {
    public:
        /** @brief 이미지와 backing memory를 생성한다. 실패하면 std::runtime_error를 던진다. */
        TGPUImage(VkPhysicalDevice      PhysicalDevice,
                 VkDevice              Device,
                 VkExtent2D            Extent,
                 VkFormat              Format,
                 VkImageTiling         Tiling,
                 VkImageUsageFlags     Usage,
                 VkMemoryPropertyFlags MemoryProperties);
        ~TGPUImage();

        TGPUImage(const TGPUImage&) = delete;
        TGPUImage& operator=(const TGPUImage&) = delete;
        TGPUImage(TGPUImage&&) = delete;
        TGPUImage& operator=(TGPUImage&&) = delete;

        /** @brief 기존 image 자원을 해제하고 새 사양으로 다시 생성한다. */
        void Recreate(VkPhysicalDevice      PhysicalDevice,
                      VkExtent2D            Extent,
                      VkFormat              Format,
                      VkImageTiling         Tiling,
                      VkImageUsageFlags     Usage,
                      VkMemoryPropertyFlags MemoryProperties);
        /** @brief image와 memory를 해제하고 객체를 빈 상태로 되돌린다. */
        void Reset();

        [[nodiscard]] VkImage    GetHandle() const noexcept;
        [[nodiscard]] VkFormat   GetFormat() const noexcept;
        [[nodiscard]] VkExtent2D GetExtent() const noexcept;

    private:
        static std::uint32_t FindMemoryType(VkPhysicalDevice      PhysicalDevice,
                                            std::uint32_t         TypeFilter,
                                            VkMemoryPropertyFlags RequiredProperties);
        void                 Create(VkPhysicalDevice      PhysicalDevice,
                                    VkExtent2D            Extent,
                                    VkFormat              Format,
                                    VkImageTiling         Tiling,
                                    VkImageUsageFlags     Usage,
                                    VkMemoryPropertyFlags MemoryProperties);

        VkDevice       Device = VK_NULL_HANDLE;
        VkImage        Handle = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkFormat       Format = VK_FORMAT_UNDEFINED;
        VkExtent2D     Extent{};
    };
} // namespace MDSS
