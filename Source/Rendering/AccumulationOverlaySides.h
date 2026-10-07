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
        // Overlay resource lifecycle
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

        // Overlay draw buffers and frame readback
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
        [[nodiscard]] VkBuffer        GetTopIndexBuffer(std::size_t Instance, std::uint32_t Channel,
                                                        std::uint32_t MeshResolution) const;
        [[nodiscard]] VkDeviceSize   GetTopIndexOffset(std::size_t Instance, std::uint32_t Channel,
                                                       std::uint32_t MeshResolution) const;
        /** @brief Read the activity counters only after this frame slot's fence has signaled. */
        [[nodiscard]] TTriangleActivity CompleteFrame(std::size_t FrameIndex);
        // Overlay command recording
        void                            Record(VkCommandBuffer                                         Command,
                                               std::size_t                                             Instance,
                                               std::uint32_t                                           Channel,
                                               std::uint32_t                                           Channels,
                                               std::uint32_t                                           MeshResolution,
                                               const SurfaceState::TSurfaceSharedGeometryGPUResources& Geometry,
                                               const SurfaceState::TSurfaceStateDescriptorResources&   StateDescriptors,
                                               VkDescriptorSet                                         ComputedSet,
                                               const GPU::TGPUBuffer&                                  GeometryCacheBuffer,
                                               bool                                                    bStateAB,
                                               std::size_t                                             FrameIndex = 0,
                                               VkQueryPool                                             TimestampQueryPool = VK_NULL_HANDLE,
                                               std::uint32_t                                           FirstSideTimestampQuery = 0,
                                               std::uint32_t                                           OccupancyTileSize = 16U,
                                               bool                                                    bSmoothCoverage = false,
                                               bool                                                    bUseOpaqueBase = false,
                                               float                                                   HeightDisplayScale = 1.0F,
                                               std::array<std::uint32_t, 3>                           MaterialChannels = {
                                                   0xffffffffU, 0xffffffffU, 0xffffffffU},
                                               std::uint32_t                                           ActiveMaterialMask = 0x7U,
                                               bool                                                    bSharedCoverageHalo = false);

    private:
        struct TOutput
        {
            std::unique_ptr<GPU::TGPUBuffer> Segments;
            std::unique_ptr<GPU::TGPUBuffer> DrawCommands;
            std::unique_ptr<GPU::TGPUBuffer> TopDrawCommands;
            std::array<std::unique_ptr<GPU::TGPUBuffer>, TRenderContext::MaxFramesInFlight> ActivityReadbacks;
            std::array<bool, TRenderContext::MaxFramesInFlight>                             ActivityPending{};
            VkDescriptorSet                                                                 Set = VK_NULL_HANDLE;
            std::size_t                                                                     CoveragePage = 0;
            VkDeviceSize                                                                    CoverageOffset = 0;
            VkDeviceSize                                                                    TopIndexOffset = 0;
            std::uint32_t                                                                   VertexCount = 0;
            std::uint32_t                                                                   TriangleCount = 0;
            std::uint32_t                                                                   BoundaryCount = 0;
            std::uint32_t                                                                   SurfaceCount = 0;
            std::uint32_t                                                                   LastTileSize = 0;
            std::array<std::uint32_t, 3>                                                   LastMaterialChannels{};
            std::uint32_t                                                                   LastMaterialMask = 0;
            std::array<std::uint64_t, 6>                                                   LastGeometryRevisions{};
            std::uint64_t                                                                   LastProfileRevision = 0;
            float                                                                           LastHeightDisplayScale = 0.0F;
            bool                                                                            bLastSmoothCoverage = false;
            bool                                                                            bLastSharedCoverageHalo = false;
            bool                                                                            bLastUseOpaqueBase = false;
            bool                                                                            bCoverageInitialized = false;
        };
        VkPhysicalDevice                                         PhysicalDevice;
        VkDevice                                                 Device;
        VkPhysicalDeviceLimits                                   Limits{};
        VkDescriptorSetLayout                                    OutputLayout = VK_NULL_HANDLE;
        VkPipelineLayout                                         PipelineLayout = VK_NULL_HANDLE;
        VkPipeline                                               CoveragePipeline = VK_NULL_HANDLE;
        std::array<VkPipeline, 2>                                CoverageSmoothingPipelines{};
        VkPipeline                                               DrawResetPipeline = VK_NULL_HANDLE;
        VkPipeline                                               BoundaryPipeline = VK_NULL_HANDLE;
        VkDescriptorPool                                         Pool = VK_NULL_HANDLE;
        std::vector<std::unique_ptr<GPU::TGPUBuffer>>            CoveragePages;
        std::vector<VkDeviceSize>                                CoveragePageUsed;
        std::map<std::tuple<std::size_t, std::uint32_t, std::uint32_t>, TOutput> Outputs;
    };
}
