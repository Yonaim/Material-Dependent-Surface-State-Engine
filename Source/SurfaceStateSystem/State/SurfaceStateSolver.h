/**
 * @file SurfaceStateSolver.h
 * @brief Record optional accumulation geometry updates and the two Surface State solver passes.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace MDSS
{
    inline constexpr std::uint32_t SurfaceSolverDisableRawFluxCacheFlag = 1U << 5U;
    inline constexpr std::uint32_t SurfaceSolverAccumulationFeedbackFlag = 1U << 6U;
    inline constexpr std::uint32_t SurfaceSolverDistanceWeightFlag = 1U << 7U;
    inline constexpr std::uint32_t SurfaceSolverNormalWeightFlag = 1U << 8U;
    inline constexpr std::uint32_t SurfaceSolverProfileBoundaryWeightFlag = 1U << 9U;
    inline constexpr std::uint32_t SurfaceSolverCurvatureWeightFlag = 1U << 10U;

    enum class TSurfaceSolverTerm : std::uint8_t
    {
        SaturationDrive,
        GeometryDrive,
        Decay,
        ConcavityRetention,
        DistanceWeight,
        NormalWeight,
        ProfileBoundaryWeight,
        CurvatureWeight,
        MesoDirectionNormal,
        Count
    };

    struct TSurfaceSolverDebugSettings
    {
        bool bRawFluxCacheEnabled = true;
        bool bAccumulationFeedbackEnabled = false;
        std::array<bool, static_cast<std::size_t>(TSurfaceSolverTerm::Count)> Enabled{
            true, true, true, true, true, true, true, false, true};

        [[nodiscard]] bool IsEnabled(TSurfaceSolverTerm Term) const noexcept
        {
            return Enabled[static_cast<std::size_t>(Term)];
        }

        void SetEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept
        {
            Enabled[static_cast<std::size_t>(Term)] = bEnabled;
        }
    };

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
                        float DeltaTime,
                        const glm::mat4& ModelMatrix,
                        const glm::vec3& GravityWorld,
                        std::uint32_t SolverFlags = 0U,
                        VkQueryPool TimestampQueryPool = VK_NULL_HANDLE,
                        std::uint32_t FirstPassQuery = 0U) const;

    private:
        static VkShaderModule CreateShaderModule(VkDevice Device, const char* Path);
        static VkPipeline CreateComputePipeline(VkDevice Device,
                                                VkPipelineLayout Layout,
                                                const char* ShaderPath,
                                                bool bRawFluxCacheEnabled);

        VkDevice         Device = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkPipeline AccumulationGeometryPipeline = VK_NULL_HANDLE;
        VkPipeline DynamicTransferWeightPipeline = VK_NULL_HANDLE;
        std::array<VkPipeline, 2> Pass1Pipelines{};
        std::array<VkPipeline, 2> Pass2Pipelines{};
    };
} // namespace MDSS
