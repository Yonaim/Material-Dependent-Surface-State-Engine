#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <optional>
#include <string>

namespace MDSS
{
    struct TSurfaceTexelSelection
    {
        std::size_t   Instance = 0;
        std::uint32_t Surface = 0;
        std::uint32_t Texel = 0;
        std::uint32_t Triangle = 0;
        glm::uvec2    XY{0};
        std::string   Profile;
    };

    struct TSurfaceTexelSnapshot
    {
        TSurfaceTexelSelection Selection;
        TStateId               Channel = InvalidStateId;
        std::uint64_t          Step = 0;
        std::uint64_t          Serial = 0;
        bool                   bStateAB = true;
        // State/capacity/saturation/reference amount; factors/meso/reference;
        // cavity depth/fill/excess/status; heights; area/scale; final local normal.
        std::array<glm::vec4, 6> Values{};
    };

    /** @brief One-texel GPU diagnostic with frame-fence-owned readback; never waits for the whole device. */
    class TTexelInspector final
    {
    public:
        TTexelInspector(VkPhysicalDevice      PhysicalDevice,
                        VkDevice              Device,
                        VkDescriptorSetLayout SurfaceLayout,
                        std::size_t           FrameCount);
        ~TTexelInspector();
        TTexelInspector(const TTexelInspector&) = delete;
        TTexelInspector& operator=(const TTexelInspector&) = delete;
        void             Record(VkCommandBuffer                         Command,
                                std::size_t                             Frame,
                                const TSurfaceStateDescriptorResources& Descriptors,
                                const TSurfaceTexelSelection&           Selection,
                                TStateId                                Channel,
                                std::uint32_t                           Channels,
                                float                                   AccumulationDisplayScale,
                                const glm::mat4&                        ModelMatrix,
                                bool                                    bStateAB,
                                std::uint64_t                           Step);
        /** @brief Call only after the corresponding frame fence has signaled. */
        void                                                      CompleteFrame(std::size_t Frame);
        void                                                      Invalidate() noexcept;
        [[nodiscard]] const std::optional<TSurfaceTexelSnapshot>& GetSnapshot() const noexcept
        {
            return Snapshot;
        }

    private:
        void Destroy() noexcept;
        struct TFrame
        {
            std::unique_ptr<TGPUBuffer>          Buffer;
            VkDescriptorSet                      Set = VK_NULL_HANDLE;
            std::optional<TSurfaceTexelSnapshot> Pending;
            std::uint64_t                        Generation = 0;
        };
        VkDevice                             Device;
        VkDescriptorSetLayout                OutputLayout = VK_NULL_HANDLE;
        VkDescriptorPool                     Pool = VK_NULL_HANDLE;
        VkPipelineLayout                     Layout = VK_NULL_HANDLE;
        VkPipeline                           Pipeline = VK_NULL_HANDLE;
        std::vector<TFrame>                  Frames;
        std::uint64_t                        Generation = 0;
        std::uint64_t                        Serial = 0;
        std::optional<TSurfaceTexelSnapshot> Snapshot;
    };
}
