/**
 * @file SurfaceStateSolver.h
 * @brief 동적 형상 갱신과 두 Surface State solver pass 기록 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <span>

namespace MDSS::SurfaceState
{
    inline constexpr std::uint32_t SurfaceSolverAccumulationGeometryUpdateFlag = 1U << 6U;
    inline constexpr std::uint32_t SurfaceSolverDistanceWeightFlag = 1U << 7U;
    inline constexpr std::uint32_t SurfaceSolverNormalWeightFlag = 1U << 8U;
    inline constexpr std::uint32_t SurfaceSolverProfileBoundaryWeightFlag = 1U << 9U;
    inline constexpr std::uint32_t SurfaceSolverForceFullGeometryFlag = 1U << 10U;
    /** @brief Pass 1의 방향별 raw flux를 cache해 Pass 2 재계산을 제거한다. */
    inline constexpr std::uint32_t SurfaceSolverRawFluxCacheFlag = 1U << 12U;
    /** @brief 동적 geometry/weight 갱신을 dirty workgroup list 기반 indirect dispatch로 제한한다. */
    inline constexpr std::uint32_t SurfaceSolverSparseGeometryFlag = 1U << 13U;
    /** @brief RawEdgeFlux를 channel/direction-major, texel-minor로 배치해 Pass 1 write coalescing을 실험한다. */
    inline constexpr std::uint32_t SurfaceSolverCoalescedRawFluxLayoutFlag = 1U << 14U;
    /** @brief Solver Pass1/2를 active workgroup + topology 1-hop list로 indirect dispatch한다. */
    inline constexpr std::uint32_t SurfaceSolverSparseSolverFlag = 1U << 15U;
    /** @brief Pass2에서 변한 accumulation-affecting texel group만 Height pass로 보낸다. */
    inline constexpr std::uint32_t SurfaceSolverSparseAccumulationHeightFlag = 1U << 16U;
    /** @brief 현재 step에서 실제로 살아 있는 State channel만 solver channel loop에서 처리한다. */
    inline constexpr std::uint32_t SurfaceSolverActiveChannelMaskFlag = 1U << 17U;
    /** @brief 현재 step 뒤에 accumulation height가 필요함을 Pass2에 알려 sparse height schedule을 생성한다. */
    inline constexpr std::uint32_t SurfaceSolverPrepareAccumulationHeightFlag = 1U << 18U;
    /** @brief RawEdgeFlux cache를 두 fp16 값/uint word로 pack해 write/read bandwidth를 줄인다. */
    inline constexpr std::uint32_t SurfaceSolverHalfRawFluxCacheFlag = 1U << 19U;
    /** @brief Dynamic/static TransferWeight를 두 fp16 값/uint word로 pack한다. */
    inline constexpr std::uint32_t SurfaceSolverHalfDynamicWeightsFlag = 1U << 20U;
    /** @brief 현재 descriptor의 CurrentState가 A buffer임을 persistent activity metadata에 알려준다. */
    inline constexpr std::uint32_t SurfaceSolverCurrentStateABFlag = 1U << 21U;
    /** @brief persistent active set bootstrap step에서는 full dispatch/channel scan을 사용하면서 next set을 생성한다. */
    inline constexpr std::uint32_t SurfaceSolverSeedPersistentActivityFlag = 1U << 22U;

    enum class TSurfaceSolverTerm : std::uint8_t
    {
        SaturationDrive,
        GeometryDrive,
        Decay,
        ConcavityRetention,
        DistanceWeight,
        NormalWeight,
        ProfileBoundaryWeight,
        MesoDirectionNormal,
        Count
    };

    struct TSurfaceSolverDebugSettings
    {
        bool                                                                  bAccumulationGeometryUpdateEnabled = false;
        std::array<bool, static_cast<std::size_t>(TSurfaceSolverTerm::Count)> Enabled{
            true, true, true, true, true, true, true, true};

        [[nodiscard]] bool IsEnabled(TSurfaceSolverTerm Term) const noexcept
        {
            return Enabled[static_cast<std::size_t>(Term)];
        }

        void SetEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept
        {
            Enabled[static_cast<std::size_t>(Term)] = bEnabled;
        }
    };

    struct TSurfaceSolverInstanceStep
    {
        const TSurfaceStateDescriptorResources* Descriptors = nullptr;
        bool                                    bCurrentStateAB = true;
        std::size_t                             TexelCount = 0;
        std::size_t                             ChannelCount = 0;
        float                                   DeltaTime = 0.0F;
        glm::mat4                               ModelMatrix{1.0F};
        glm::vec3                               GravityWorld{0.0F, 0.0F, -1.0F};
        std::uint32_t                           SolverFlags = 0U;
        // Feedback ON이면 매 substep에서 height가 필요하고, OFF이면 렌더 직전 step에서만 true면 된다.
        bool                                    bPrepareAccumulationHeight = true;
        // CPU contact distribution이 sparse input group list를 업로드한 경우 reset/merge pass를 반드시 기록한다.
        bool                                    bHasInputActivation = false;
    };

    class TSurfaceStateSolver final
    {
    public:
        // Lifecycle
        TSurfaceStateSolver(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout);
        ~TSurfaceStateSolver();

        TSurfaceStateSolver(const TSurfaceStateSolver&) = delete;
        TSurfaceStateSolver& operator=(const TSurfaceStateSolver&) = delete;
        TSurfaceStateSolver(TSurfaceStateSolver&&) = delete;
        TSurfaceStateSolver& operator=(TSurfaceStateSolver&&) = delete;

        // Solver command recording
        void RecordStep(VkCommandBuffer                         CommandBuffer,
                        const TSurfaceStateDescriptorResources& Descriptors,
                        bool                                    bCurrentStateAB,
                        std::size_t                             TexelCount,
                        std::size_t                             ChannelCount,
                        float                                   DeltaTime,
                        const glm::mat4&                        ModelMatrix,
                        const glm::vec3&                        GravityWorld,
                        std::uint32_t                           SolverFlags = 0U,
                        VkQueryPool                             TimestampQueryPool = VK_NULL_HANDLE,
                        std::uint32_t                           FirstPassQuery = 0U,
                        bool                                    bPrepareAccumulationHeight = true) const;
        void RecordSteps(VkCommandBuffer                         CommandBuffer,
                         std::span<const TSurfaceSolverInstanceStep> InstanceSteps,
                         VkQueryPool                             TimestampQueryPool = VK_NULL_HANDLE,
                         std::uint32_t                           FirstStepQuery = 0U) const;

    private:
        // Compute pipeline creation
        static VkShaderModule CreateShaderModule(VkDevice Device, const char* Path);
        static VkPipeline     CreateComputePipeline(VkDevice         Device,
                                                    VkPipelineLayout Layout,
                                                    const char*      ShaderPath);

        VkDevice                  Device = VK_NULL_HANDLE;
        VkPipelineLayout          PipelineLayout = VK_NULL_HANDLE;
        VkPipeline                AccumulationHeightPipeline = VK_NULL_HANDLE;
        VkPipeline                SparseScheduleResetPipeline = VK_NULL_HANDLE;
        VkPipeline                AccumulationGeometryPipeline = VK_NULL_HANDLE;
        VkPipeline                DynamicTransferWeightPipeline = VK_NULL_HANDLE;
        VkPipeline                Pass1Pipeline = VK_NULL_HANDLE;
        VkPipeline                Pass2Pipeline = VK_NULL_HANDLE;
    };
} // namespace MDSS::SurfaceState
