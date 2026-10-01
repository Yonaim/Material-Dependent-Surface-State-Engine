#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "VulkanContext/GPU/GPUBuffer.h"

#include <map>
#include <memory>
#include <utility>

namespace MDSS
{
    /** @brief Compute builds one possible wall segment per texel-mesh triangle or open edge. */
    class TAccumulationOverlaySides final
    {
    public:
        TAccumulationOverlaySides(VkPhysicalDevice PhysicalDevice, VkDevice Device,
                                  VkDescriptorSetLayout SurfaceLayout, VkDescriptorSetLayout ComputedLayout,
                                  std::size_t MaxInstances);
        ~TAccumulationOverlaySides();
        TAccumulationOverlaySides(const TAccumulationOverlaySides&) = delete;
        TAccumulationOverlaySides& operator=(const TAccumulationOverlaySides&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept { return OutputLayout; }
        [[nodiscard]] VkDescriptorSet GetSet(std::size_t Instance, std::uint32_t Channel) const;
        [[nodiscard]] std::uint32_t GetTriangleCount(std::size_t Instance, std::uint32_t Channel) const;
        [[nodiscard]] std::uint32_t GetBoundaryCount(std::size_t Instance, std::uint32_t Channel) const;
        void Record(VkCommandBuffer Command, std::size_t Instance, std::uint32_t Channel,
                    std::uint32_t Channels, const TSurfaceSharedGeometryGPUResources& Geometry,
                    const TSurfaceStateDescriptorResources& StateDescriptors, VkDescriptorSet ComputedSet,
                    bool bStateAB, VkQueryPool TimestampQueryPool = VK_NULL_HANDLE,
                    std::uint32_t FirstSideTimestampQuery = 0);

    private:
        struct TOutput
        {
            std::unique_ptr<TGPUBuffer> Segments;
            VkDescriptorSet Set = VK_NULL_HANDLE;
            std::size_t CoveragePage = 0;
            VkDeviceSize CoverageOffset = 0;
            std::uint32_t VertexCount = 0;
            std::uint32_t TriangleCount = 0;
            std::uint32_t BoundaryCount = 0;
        };
        VkPhysicalDevice PhysicalDevice;
        VkDevice Device;
        VkPhysicalDeviceLimits Limits{};
        VkDescriptorSetLayout OutputLayout = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkPipeline CoveragePipeline = VK_NULL_HANDLE;
        VkPipeline BoundaryPipeline = VK_NULL_HANDLE;
        VkPipeline Pipeline = VK_NULL_HANDLE;
        VkDescriptorPool Pool = VK_NULL_HANDLE;
        std::vector<std::unique_ptr<TGPUBuffer>> CoveragePages;
        std::vector<VkDeviceSize> CoveragePageUsed;
        std::map<std::pair<std::size_t, std::uint32_t>, TOutput> Outputs;
    };
}
