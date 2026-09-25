/**
 * @file GPUSampler.cpp
 * @brief 텍스처 샘플링과 주소 지정 설정을 담는 Vulkan sampler.
 */

#include "VulkanContext/GPU/GPUSampler.h"

#include "Logger/Logger.h"

#include <stdexcept>

namespace MDSS
{
    GPUSampler::GPUSampler(VkDevice Device) : Device(Device)
    {
        VkSamplerCreateInfo Info{};
        Info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        Info.magFilter = VK_FILTER_LINEAR;
        Info.minFilter = VK_FILTER_LINEAR;
        Info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        Info.anisotropyEnable = VK_FALSE;
        Info.maxAnisotropy = 1.0F;
        Info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        Info.unnormalizedCoordinates = VK_FALSE;
        Info.compareEnable = VK_FALSE;
        Info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        Info.minLod = 0.0F;
        Info.maxLod = 0.0F;

        if (vkCreateSampler(Device, &Info, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan sampler.");
        }
        Logger::Verbose("Vulkan", "GPUSampler created (linear filtering, repeat addressing).");
    }

    GPUSampler::~GPUSampler()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroySampler(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }
    }

    VkSampler GPUSampler::GetHandle() const noexcept
    {
        return Handle;
    }
} // namespace MDSS
