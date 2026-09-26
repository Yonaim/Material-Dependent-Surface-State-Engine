/**
 * @file SurfaceStateSolver.h
 * @brief Record the two compute passes that update per-instance Surface State.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

namespace MDSS
{
    class TSurfaceStateSolver final
    {
    public:
        TSurfaceStateSolver(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout);
        ~TSurfaceStateSolver();

        TSurfaceStateSolver(const TSurfaceStateSolver&) = delete;
        TSurfaceStateSolver& operator=(const TSurfaceStateSolver&) = delete;
        TSurfaceStateSolver(TSurfaceStateSolver&&) = delete;
        TSurfaceStateSolver& operator=(TSurfaceStateSolver&&) = delete;

        void RecordStep(VkCommandBuffer CommandBuffer,
                        const TSurfaceStateDescriptorResources& Descriptors,
                        bool bCurrentStateAB,
                        std::size_t TexelCount,
                        std::size_t ChannelCount,
                        float DeltaTime) const;

    private:
        static VkShaderModule CreateShaderModule(VkDevice Device, const char* Path);
        static VkPipeline CreateComputePipeline(VkDevice Device,
                                                VkPipelineLayout Layout,
                                                const char* ShaderPath);

        VkDevice         Device = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkPipeline       Pass1Pipeline = VK_NULL_HANDLE;
        VkPipeline       Pass2Pipeline = VK_NULL_HANDLE;
    };
} // namespace MDSS
