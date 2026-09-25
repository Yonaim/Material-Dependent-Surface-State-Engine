/**
 * @file GPUSampler.h
 * @brief 텍스처 샘플링과 주소 지정 설정을 담는 Vulkan sampler.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class TGPUSampler
    {
    public:
        /** @brief 프로젝트 기본 filtering 및 address mode를 사용하는 sampler를 생성한다. */
        explicit TGPUSampler(VkDevice Device);
        ~TGPUSampler();

        TGPUSampler(const TGPUSampler&) = delete;
        TGPUSampler& operator=(const TGPUSampler&) = delete;
        TGPUSampler(TGPUSampler&&) = delete;
        TGPUSampler& operator=(TGPUSampler&&) = delete;

        [[nodiscard]] VkSampler GetHandle() const noexcept;

    private:
        VkDevice  Device = VK_NULL_HANDLE;
        VkSampler Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
