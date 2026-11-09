/**
 * @file TexelInspector.h
 * @brief 선택한 texel의 진단 데이터를 조회하는 인터페이스를 선언한다.
 */
#pragma once

#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"

#include <optional>
#include <string>

namespace MDSS::SurfaceState
{
    struct FSurfaceTexelSelection
    {
        std::size_t   Instance = 0;
        std::uint32_t Surface = 0;
        std::uint32_t Texel = 0;
        std::uint32_t Triangle = 0;
        glm::uvec2    XY{0};
        std::string   Profile;
    };

    struct FSurfaceTexelSnapshot
    {
        FSurfaceTexelSelection Selection;
        FStateId               Channel = InvalidStateId;
        std::uint64_t          Step = 0;
        std::uint64_t          Serial = 0;
        bool                   bStateAB = true;
        // State/capacity/saturation/reference amount; factors/meso/reference;
        // cavity depth/fill/excess/status; heights; area/scale; final local normal.
        std::array<glm::vec4, 7> Values{};
    };

    /** @brief One-texel GPU diagnostic with frame-fence-owned readback; never waits for the whole device. */
    class FTexelInspector final
    {
    public:
        // Inspector resource lifecycle
        FTexelInspector(VkPhysicalDevice      PhysicalDevice,
                        VkDevice              Device,
                        VkDescriptorSetLayout SurfaceLayout,
                        std::size_t           FrameCount);
        ~FTexelInspector();
        FTexelInspector(const FTexelInspector&) = delete;
        FTexelInspector& operator=(const FTexelInspector&) = delete;

        // Selection recording and snapshot readback
        void Record(VkCommandBuffer                         Command,
                    std::size_t                             Frame,
                    const FSurfaceStateDescriptorResources& Descriptors,
                    const FSurfaceTexelSelection&           Selection,
                    FStateId                                Channel,
                    std::uint32_t                           Channels,
                    float                                   AccumulationDisplayScale,
                    const glm::mat4&                        ModelMatrix,
                    bool                                    bStateAB,
                    std::uint64_t                           Step);
        /** @brief Call only after the corresponding frame fence has signaled. */
        void                                                      CompleteFrame(std::size_t Frame);
        void                                                      Invalidate() noexcept;
        [[nodiscard]] const std::optional<FSurfaceTexelSnapshot>& GetSnapshot() const noexcept
        {
            return Snapshot;
        }

    private:
        // Resource cleanup
        void Destroy() noexcept;
        struct FFrame
        {
            std::unique_ptr<GPU::FGPUBuffer>     Buffer;
            VkDescriptorSet                      Set = VK_NULL_HANDLE;
            std::optional<FSurfaceTexelSnapshot> Pending;
            std::uint64_t                        Generation = 0;
        };
        VkDevice                             Device;
        VkDescriptorSetLayout                OutputLayout = VK_NULL_HANDLE;
        VkDescriptorPool                     Pool = VK_NULL_HANDLE;
        VkPipelineLayout                     Layout = VK_NULL_HANDLE;
        VkPipeline                           Pipeline = VK_NULL_HANDLE;
        std::vector<FFrame>                  Frames;
        std::uint64_t                        Generation = 0;
        std::uint64_t                        Serial = 0;
        std::optional<FSurfaceTexelSnapshot> Snapshot;
    };
}
