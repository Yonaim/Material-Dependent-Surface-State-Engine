/**
 * @file HeightFieldSmoothing.h
 * @brief 표시용 height field smoothing pass의 인터페이스를 선언한다.
 */
#pragma once

#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <map>
#include <memory>
#include <utility>

namespace MDSS::Rendering
{
    /** @brief Optional, render-only Gaussian filter for one accumulation State's texel heights. */
    class THeightFieldSmoothing final
    {
    public:
        // Smoothing resource lifecycle
        THeightFieldSmoothing(VkPhysicalDevice      PhysicalDevice,
                              VkDevice              Device,
                              VkDescriptorSetLayout SurfaceLayout,
                              VkDescriptorSetLayout HeightLayout,
                              std::size_t           MaxInstances);
        ~THeightFieldSmoothing();
        THeightFieldSmoothing(const THeightFieldSmoothing&) = delete;
        THeightFieldSmoothing& operator=(const THeightFieldSmoothing&) = delete;

        // Smoothing outputs and command recording
        [[nodiscard]] VkDescriptorSet        GetOutputSet(std::size_t Instance, std::uint32_t Channel) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetOutputBuffer(std::size_t Instance, std::uint32_t Channel) const;
        void                                 Invalidate() noexcept;
        /** @brief Releases lazily-created smoothing outputs. Caller must ensure no in-flight GPU work uses them. */
        void                                 ReleaseOutputs() noexcept;
        void                                 Record(VkCommandBuffer                                       Command,
                                                    std::size_t                                           Instance,
                                                    std::uint32_t                                         Channel,
                                                    std::uint32_t                                         Channels,
                                                    std::uint32_t                                         TexelCount,
                                                    const SurfaceState::TSurfaceStateDescriptorResources& StateDescriptors,
                                                    VkDescriptorSet                                       InputSet,
                                                    const GPU::TGPUBuffer&                                InputBuffer,
                                                    const GPU::TGPUBuffer&                                GeometryCacheBuffer,
                                                    bool                                                  bStateAB,
                                                    float                                                 AccumulationDisplayScale,
                                                    std::uint32_t                                         OccupancyTileSize,
                                                    bool                                                  bSparse);

    private:
        struct TOutput
        {
            std::unique_ptr<GPU::TGPUBuffer> Buffer;
            VkDescriptorSet                  Set = VK_NULL_HANDLE;
            VkBuffer                         LastInputBuffer = VK_NULL_HANDLE;
            float                            LastDisplayScale = 0.0F;
            std::uint32_t                    LastTileSize = 0U;
            bool                             bInitialized = false;
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
