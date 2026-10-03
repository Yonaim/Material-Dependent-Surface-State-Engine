/**
 * @file HeightFieldSmoothing.h
 * @brief 표시용 height field smoothing pass의 인터페이스를 선언한다.
 */
#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "VulkanContext/GPU/GPUBuffer.h"

#include <map>
#include <memory>
#include <utility>

namespace MDSS
{
    /** @brief Optional, render-only Gaussian filter for one accumulation State's texel heights. */
    class THeightFieldSmoothing final
    {
    public:
        THeightFieldSmoothing(VkPhysicalDevice      PhysicalDevice,
                              VkDevice              Device,
                              VkDescriptorSetLayout SurfaceLayout,
                              VkDescriptorSetLayout HeightLayout,
                              std::size_t           MaxInstances);
        ~THeightFieldSmoothing();
        THeightFieldSmoothing(const THeightFieldSmoothing&) = delete;
        THeightFieldSmoothing& operator=(const THeightFieldSmoothing&) = delete;

        [[nodiscard]] VkDescriptorSet   GetOutputSet(std::size_t Instance, std::uint32_t Channel) const;
        [[nodiscard]] const TGPUBuffer& GetOutputBuffer(std::size_t Instance, std::uint32_t Channel) const;
        void                            Record(VkCommandBuffer                         Command,
                                               std::size_t                             Instance,
                                               std::uint32_t                           Channel,
                                               std::uint32_t                           Channels,
                                               std::uint32_t                           TexelCount,
                                               const TSurfaceStateDescriptorResources& StateDescriptors,
                                               VkDescriptorSet                         InputSet,
                                               const TGPUBuffer&                       InputBuffer,
                                               bool                                    bStateAB,
                                               float                                   AccumulationDisplayScale);

    private:
        struct TOutput
        {
            std::unique_ptr<TGPUBuffer> Buffer;
            VkDescriptorSet             Set = VK_NULL_HANDLE;
        };
        VkPhysicalDevice                                         PhysicalDevice;
        VkDevice                                                 Device;
        VkPhysicalDeviceLimits                                   Limits{};
        VkDescriptorSetLayout                                    HeightLayout;
        VkPipelineLayout                                         PipelineLayout = VK_NULL_HANDLE;
        VkPipeline                                               Pipeline = VK_NULL_HANDLE;
        VkDescriptorPool                                         Pool = VK_NULL_HANDLE;
        std::map<std::pair<std::size_t, std::uint32_t>, TOutput> Outputs;
    };
}
