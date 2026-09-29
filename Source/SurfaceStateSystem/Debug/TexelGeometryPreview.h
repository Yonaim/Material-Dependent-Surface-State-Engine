#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <map>

namespace MDSS
{
    struct alignas(16) TTexelGeometryVertex
    {
        glm::vec4 PositionAndHeight{0};
        glm::vec4 Normal{0};
    };
    static_assert(sizeof(TTexelGeometryVertex) == 32);
    static_assert(offsetof(TTexelGeometryVertex, Normal) == 16);

    /** @brief Read-only State projection. Output is display geometry, never Solver feedback. */
    class TTexelGeometryPreview final
    {
    public:
        TTexelGeometryPreview(VkPhysicalDevice PhysicalDevice, VkDevice Device,
                              VkDescriptorSetLayout SurfaceLayout, std::size_t MaxInstances);
        ~TTexelGeometryPreview();
        TTexelGeometryPreview(const TTexelGeometryPreview&) = delete;
        TTexelGeometryPreview& operator=(const TTexelGeometryPreview&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetOutputLayout() const noexcept { return OutputLayout; }
        [[nodiscard]] VkDescriptorSet GetOutputSet(std::size_t Instance) const;
        [[nodiscard]] const TGPUBuffer& GetOutputBuffer(std::size_t Instance) const;
        void Record(VkCommandBuffer Command, std::size_t Instance,
                    const TSurfaceStateDescriptorResources& Descriptors, std::uint32_t TexelCount,
                    std::uint32_t Channel, std::uint32_t Channels, float HeightReference,
                    float DisplayScale, bool bStateAB, bool bAccumulation);

    private:
        void Destroy() noexcept;
        struct TOutput
        {
            std::unique_ptr<TGPUBuffer> Buffer;
            VkDescriptorSet Set = VK_NULL_HANDLE;
        };
        VkPhysicalDevice PhysicalDevice;
        VkDevice Device;
        VkDescriptorSetLayout OutputLayout = VK_NULL_HANDLE;
        VkPipelineLayout Layout = VK_NULL_HANDLE;
        VkPipeline Pipeline = VK_NULL_HANDLE;
        VkDescriptorPool Pool = VK_NULL_HANDLE;
        VkPhysicalDeviceLimits Limits{};
        std::map<std::size_t, TOutput> Outputs;
    };
}
