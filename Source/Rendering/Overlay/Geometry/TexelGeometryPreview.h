/**
 * @file TexelGeometryPreview.h
 * @brief 진단용 texel geometry 미리보기 및 cache 인터페이스를 선언한다.
 */
#pragma once

#include "Rendering/Core/RenderContext.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"

#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

namespace MDSS::SurfaceState
{
    struct alignas(16) FTexelGeometryVertex
    {
        glm::vec4 HeightAndNormal{0};
    };
    static_assert(sizeof(FTexelGeometryVertex) == 16);

    /** @brief Read-only State projection. Output is display geometry, never State-Driven Geometry Update. */
    class FTexelGeometryPreview final
    {
    public:
        struct FTileActivity
        {
            std::uint64_t Active = 0;
            std::uint64_t Total = 0;
        };

        // Preview resource lifecycle
        FTexelGeometryPreview(VkPhysicalDevice      PhysicalDevice,
                              VkDevice              Device,
                              VkDescriptorSetLayout SurfaceLayout,
                              std::size_t           MaxInstances,
                              bool                  bEnableOccupancyScan = false);
        ~FTexelGeometryPreview();
        FTexelGeometryPreview(const FTexelGeometryPreview&) = delete;
        FTexelGeometryPreview& operator=(const FTexelGeometryPreview&) = delete;

        // Output access and preview settings
        [[nodiscard]] VkDescriptorSetLayout GetOutputLayout() const noexcept
        {
            return OutputLayout;
        }

        [[nodiscard]] VkDescriptorSet        GetOutputSet(std::size_t Instance, std::size_t OutputSlot = 0U) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetOutputBuffer(std::size_t Instance, std::size_t OutputSlot = 0U) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetGeometryCacheBuffer(std::size_t Instance,
                                                                    std::size_t OutputSlot = 0U) const;
        /** @brief Read active-tile counters after the corresponding frame slot fence has signaled. */
        [[nodiscard]] FTileActivity CompleteOccupancyFrame(std::size_t FrameIndex);
        void                        SetOccupancyTileSize(std::uint32_t TileSize);
        void                        SetLowAmountHeightFade(float Saturation);
        void                        InvalidateSparseOutput() noexcept;

        // Compute command recording
        void
        Record(VkCommandBuffer                         Command,
               std::size_t                             Instance,
               const FSurfaceStateDescriptorResources& Descriptors,
               std::uint32_t                           TexelCount,
               std::uint32_t                           Channel,
               std::uint32_t                           Channels,
               float                                   AccumulationDisplayScale,
               float                                   GeometryDisplayScale,
               const glm::mat4&                        ModelMatrix,
               bool                                    bStateAB,
               bool                                    bAccumulation,
               VkQueryPool                             TimestampQueryPool = VK_NULL_HANDLE,
               std::uint32_t                           HeightCompleteQuery = 0U,
               bool                                    bTotalHeight = false,
               std::size_t                             FrameIndex = 0U,
               std::array<std::uint32_t, 4> MaterialChannels = {0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU},
               std::uint32_t                ActiveMaterialMask = 0U,
               std::size_t                  OutputSlot = 0U);

        // Call only after GPU completion; descriptors and their buffers are released together.
        void ReleaseOutputs()
        {
            Outputs.clear();

            if (vkResetDescriptorPool(Device, Pool, 0) != VK_SUCCESS)
                throw std::runtime_error("Cannot reset render output descriptors.");
        }

    private:
        // Resource cleanup
        void Destroy() noexcept;
        struct FOutput
        {
            std::unique_ptr<GPU::FGPUBuffer> Buffer;
            std::unique_ptr<GPU::FGPUBuffer> GeometryCache;
            std::array<std::unique_ptr<GPU::FGPUBuffer>, Rendering::FRenderContext::MaxFramesInFlight>
                                                                           OccupancyReadbacks;
            std::array<bool, Rendering::FRenderContext::MaxFramesInFlight> OccupancyPending{};
            std::uint32_t                                                  OccupancyTotalTiles = 0;
            VkDescriptorSet                                                Set = VK_NULL_HANDLE;
            bool                                                           bOccupancyInitialized = false;
            bool                                                           bSparseReady = false;
            float                                                          BaselineScale = 0.0F;
            bool                                                           bBaselineReady = false;
            bool                                                           bInputsObserved = false;
            std::array<std::uint64_t, 6>                                   ObservedInputRevisions{};
        };
        VkPhysicalDevice                                       PhysicalDevice;
        VkDevice                                               Device;
        VkDescriptorSetLayout                                  OutputLayout = VK_NULL_HANDLE;
        VkPipelineLayout                                       Layout = VK_NULL_HANDLE;
        VkPipeline                                             Pipeline = VK_NULL_HANDLE;
        VkPipeline                                             HeightPipeline = VK_NULL_HANDLE;
        VkPipeline                                             BaselinePipeline = VK_NULL_HANDLE;
        VkPipeline                                             OccupancyPipeline = VK_NULL_HANDLE;
        VkDescriptorPool                                       Pool = VK_NULL_HANDLE;
        VkPhysicalDeviceLimits                                 Limits{};
        bool                                                   bEnableOccupancyScan = false;
        std::uint32_t                                          OccupancyTileSize = 16U;
        float                                                  LowAmountHeightFade = 0.05F;
        std::map<std::pair<std::size_t, std::size_t>, FOutput> Outputs;
    };
}
