/**
 * @file ResourceBufferHelpers.h
 * @brief Surface 자원 생성에 필요한 GPU 버퍼 할당·업로드·재사용을 공통화한다.
 */

#pragma once
#include "SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"

#include <stdexcept>
#include <vector>

namespace MDSS::SurfaceState::Detail
{
    constexpr VkBufferUsageFlags    StorageUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    constexpr VkMemoryPropertyFlags UploadMemory =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    constexpr std::uint32_t DescriptorBindingCount = static_cast<std::uint32_t>(ESurfaceGPUDescriptorBinding::Count);

    constexpr std::uint32_t BindingIndex(ESurfaceGPUDescriptorBinding Binding)
    {
        return static_cast<std::uint32_t>(Binding);
    }

    inline std::size_t GetMaximumStorageBufferRange(VkPhysicalDevice PhysicalDevice)
    {
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        return Properties.limits.maxStorageBufferRange;
    }

    inline std::unique_ptr<GPU::FGPUBuffer> AcquireOrCreateBuffer(VkPhysicalDevice              PhysicalDevice,
                                                                  VkDevice                      Device,
                                                                  VkDeviceSize                  Size,
                                                                  VkBufferUsageFlags            Usage,
                                                                  GPU::EGPUBufferMemoryCategory Category,
                                                                  FSurfaceGPUBufferPool*        Pool)
    {
        if (Pool)
        {
            if (auto Buffer = Pool->Acquire(Size, Usage, UploadMemory, Category))
                return Buffer;
        }

        try
        {
            return std::make_unique<GPU::FGPUBuffer>(PhysicalDevice, Device, Size, Usage, UploadMemory, Category);
        }
        catch (const std::runtime_error&)
        {
            // Retained buffers must not prevent a Scene with new buffer sizes from loading.
            if (!Pool || Pool->GetRetainedBytes() == 0)
                throw;
            Pool->Clear();

            return std::make_unique<GPU::FGPUBuffer>(PhysicalDevice, Device, Size, Usage, UploadMemory, Category);
        }
    }

    inline std::unique_ptr<GPU::FGPUBuffer>
    CreateUploadedBuffer(VkPhysicalDevice              PhysicalDevice,
                         VkDevice                      Device,
                         const void*                   Data,
                         std::size_t                   ElementCount,
                         std::size_t                   ElementStride,
                         std::size_t                   MaxStorageBufferRange,
                         GPU::EGPUBufferMemoryCategory Category = GPU::EGPUBufferMemoryCategory::SurfaceGeometry,
                         FSurfaceGPUBufferPool*        Pool = nullptr)
    {
        const std::size_t ByteSize = GetSurfaceGPUBufferByteSize(ElementCount, ElementStride, MaxStorageBufferRange);
        auto              Buffer = AcquireOrCreateBuffer(
            PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), StorageUsage, Category, Pool);
        Buffer->Upload(Data, static_cast<VkDeviceSize>(ByteSize));

        return Buffer;
    }

    inline std::unique_ptr<GPU::FGPUBuffer>
    CreateZeroedScalarBuffer(VkPhysicalDevice              PhysicalDevice,
                             VkDevice                      Device,
                             std::size_t                   ScalarCount,
                             std::size_t                   MaxStorageBufferRange,
                             VkBufferUsageFlags            Usage = StorageUsage,
                             GPU::EGPUBufferMemoryCategory Category = GPU::EGPUBufferMemoryCategory::SurfaceState,
                             FSurfaceGPUBufferPool*        Pool = nullptr)
    {
        const std::size_t  ByteSize = GetSurfaceGPUBufferByteSize(ScalarCount, sizeof(float), MaxStorageBufferRange);
        std::vector<float> Zeros(ScalarCount, 0.0F);
        auto               Buffer =
            AcquireOrCreateBuffer(PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), Usage, Category, Pool);
        Buffer->Upload(Zeros.data(), static_cast<VkDeviceSize>(ByteSize));

        return Buffer;
    }

}
