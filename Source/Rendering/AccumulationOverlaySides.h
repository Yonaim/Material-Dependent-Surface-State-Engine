/**
 * @file AccumulationOverlaySides.h
 * @brief 누적 상태 overlay geometry 생성에 필요한 자료형과 함수를 선언한다.
 */
#pragma once

#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "Rendering/RenderContext.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

namespace MDSS::Rendering
{
    /** @brief Detects walls and packs only active segments for indirect drawing. */
    class TAccumulationOverlaySides final
    {
    public:
        struct TTriangleActivity
        {
            std::uint64_t Active = 0;
            std::uint64_t Total = 0;
        };

        TAccumulationOverlaySides(VkPhysicalDevice      PhysicalDevice,
                                  VkDevice              Device,
                                  VkDescriptorSetLayout SurfaceLayout,
                                  VkDescriptorSetLayout ComputedLayout,
                                  std::size_t           MaxInstances);
        ~TAccumulationOverlaySides();
        TAccumulationOverlaySides(const TAccumulationOverlaySides&) = delete;
        TAccumulationOverlaySides& operator=(const TAccumulationOverlaySides&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept
        {
            return OutputLayout;
        }
        [[nodiscard]] VkDescriptorSet GetSet(std::size_t Instance, std::uint32_t Channel,
                                             std::uint32_t MeshResolution) const;
        [[nodiscard]] std::uint32_t   GetBoundaryCount(std::size_t Instance, std::uint32_t Channel,
                                                       std::uint32_t MeshResolution) const;
        [[nodiscard]] VkBuffer        GetDrawBuffer(std::size_t Instance, std::uint32_t Channel,
                                                    std::uint32_t MeshResolution) const;
        [[nodiscard]] VkBuffer        GetTopDrawBuffer(std::size_t Instance, std::uint32_t Channel,
                                                       std::uint32_t MeshResolution) const;
        /** @brief Read the activity counters only after this frame slot's fence has signaled. */
        [[nodiscard]] TTriangleActivity CompleteFrame(std::size_t FrameIndex);
        void                            Record(VkCommandBuffer                                         Command,
                                               std::size_t                                             Instance,
                                               std::uint32_t                                           Channel,
                                               std::uint32_t                                           Channels,
                                               std::uint32_t                                           MeshResolution,
                                               const SurfaceState::TSurfaceSharedGeometryGPUResources& Geometry,
                                               const SurfaceState::TSurfaceStateDescriptorResources&   StateDescriptors,
                                               VkDescriptorSet                                         ComputedSet,
                                               bool                                                    bStateAB,
                                               std::size_t                                             FrameIndex = 0,
                                               VkQueryPool                                             TimestampQueryPool = VK_NULL_HANDLE,
                                               std::uint32_t                                           FirstSideTimestampQuery = 0,
                                               std::uint32_t                                           OccupancyTileSize = 16U,
                                               bool                                                    bSmoothCoverage = false);

    private:
        struct TOutput
        {
            std::unique_ptr<GPU::TGPUBuffer> Segments;
            std::unique_ptr<GPU::TGPUBuffer> DrawCommands;
            std::unique_ptr<GPU::TGPUBuffer> TopDrawCommands;
            VkDescriptorSet                  TopCommandSet = VK_NULL_HANDLE;
            std::array<std::unique_ptr<GPU::TGPUBuffer>, TRenderContext::MaxFramesInFlight> ActivityReadbacks;
            std::array<bool, TRenderContext::MaxFramesInFlight>                             ActivityPending{};
            VkDescriptorSet                                                                 Set = VK_NULL_HANDLE;
            std::size_t                                                                     CoveragePage = 0;
            VkDeviceSize                                                                    CoverageOffset = 0;
            std::uint32_t                                                                   VertexCount = 0;
            std::uint32_t                                                                   TriangleCount = 0;
            std::uint32_t                                                                   BoundaryCount = 0;
            std::uint32_t                                                                   SurfaceCount = 0;
        };
        VkPhysicalDevice                                         PhysicalDevice;
        VkDevice                                                 Device;
        VkPhysicalDeviceLimits                                   Limits{};
        VkDescriptorSetLayout                                    OutputLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout                                    TopCommandSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout                                         PipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayout                                         TopCommandLayout = VK_NULL_HANDLE;
        VkPipeline                                               CoveragePipeline = VK_NULL_HANDLE;
        VkPipeline                                               CoverageSmoothingPipeline = VK_NULL_HANDLE;
        VkPipeline                                               BoundaryPipeline = VK_NULL_HANDLE;
        VkPipeline                                               TopCommandPipeline = VK_NULL_HANDLE;
        VkDescriptorPool                                         Pool = VK_NULL_HANDLE;
        VkDescriptorPool                                         TopCommandPool = VK_NULL_HANDLE;
        std::vector<std::unique_ptr<GPU::TGPUBuffer>>            CoveragePages;
        std::vector<VkDeviceSize>                                CoveragePageUsed;
        std::map<std::tuple<std::size_t, std::uint32_t, std::uint32_t>, TOutput> Outputs;
    };
}
