/**
 * @file TexelGeometryPreview.h
 * @brief 진단용 texel geometry 미리보기 및 cache 인터페이스를 선언한다.
 */
#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <map>

namespace MDSS
{
    struct alignas(16) TTexelGeometryVertex
    {
        glm::vec4 HeightAndNormal{0};
    };
    static_assert(sizeof(TTexelGeometryVertex) == 16);

    /** @brief Read-only State projection. Output is display geometry, never Solver feedback. */
    class TTexelGeometryPreview final
    {
    public:
        TTexelGeometryPreview(VkPhysicalDevice PhysicalDevice, VkDevice Device,
                              VkDescriptorSetLayout SurfaceLayout, std::size_t MaxInstances,
                              bool bEnableOccupancyScan = false);
        ~TTexelGeometryPreview();
        TTexelGeometryPreview(const TTexelGeometryPreview&) = delete;
        TTexelGeometryPreview& operator=(const TTexelGeometryPreview&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetOutputLayout() const noexcept { return OutputLayout; }
        [[nodiscard]] VkDescriptorSet GetOutputSet(std::size_t Instance) const;
        [[nodiscard]] const TGPUBuffer& GetOutputBuffer(std::size_t Instance) const;
        void SetOccupancyTileSize(std::uint32_t TileSize);
        void Record(VkCommandBuffer Command, std::size_t Instance,
                    const TSurfaceStateDescriptorResources& Descriptors, std::uint32_t TexelCount,
                    std::uint32_t Channel, std::uint32_t Channels, float AccumulationDisplayScale,
                    float GeometryDisplayScale, const glm::mat4& ModelMatrix, bool bStateAB, bool bAccumulation,
                    VkQueryPool TimestampQueryPool = VK_NULL_HANDLE, std::uint32_t HeightCompleteQuery = 0U,
                    bool bTotalHeight = false);

    private:
        void Destroy() noexcept;
        struct TOutput
        {
            std::unique_ptr<TGPUBuffer> Buffer;
            std::unique_ptr<TGPUBuffer> GeometryCache;
            VkDescriptorSet Set = VK_NULL_HANDLE;
            bool bOccupancyInitialized = false;
            float BaselineScale = 0.0F;
            bool bBaselineReady = false;
            bool bInputsObserved = false;
            std::array<std::uint64_t, 6> ObservedInputRevisions{};
        };
        VkPhysicalDevice PhysicalDevice;
        VkDevice Device;
        VkDescriptorSetLayout OutputLayout = VK_NULL_HANDLE;
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        VkPipeline Pipeline = VK_NULL_HANDLE;
        VkPipeline HeightPipeline = VK_NULL_HANDLE;
        VkPipeline BaselinePipeline = VK_NULL_HANDLE;
        VkPipeline OccupancyPipeline = VK_NULL_HANDLE;
        VkDescriptorPool Pool = VK_NULL_HANDLE;
        VkPhysicalDeviceLimits Limits{};
        bool bEnableOccupancyScan = false;
        std::uint32_t OccupancyTileSize = 16U;
        std::map<std::size_t, TOutput> Outputs;
    };
}
