#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class GPUImage
    {
    public:
        GPUImage(VkPhysicalDevice      PhysicalDevice,
                 VkDevice              Device,
                 VkExtent2D            Extent,
                 VkFormat              Format,
                 VkImageTiling         Tiling,
                 VkImageUsageFlags     Usage,
                 VkMemoryPropertyFlags MemoryProperties);
        ~GPUImage();

        GPUImage(const GPUImage&) = delete;
        GPUImage& operator=(const GPUImage&) = delete;
        GPUImage(GPUImage&&) = delete;
        GPUImage& operator=(GPUImage&&) = delete;

        void Recreate(VkPhysicalDevice      PhysicalDevice,
                      VkExtent2D            Extent,
                      VkFormat              Format,
                      VkImageTiling         Tiling,
                      VkImageUsageFlags     Usage,
                      VkMemoryPropertyFlags MemoryProperties);
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
