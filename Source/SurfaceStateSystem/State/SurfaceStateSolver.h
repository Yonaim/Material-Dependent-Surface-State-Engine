/**
 * @file SurfaceStateSolver.h
 * @brief Record the two compute passes that update per-instance Surface State.
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
                                                const char* ShaderPath);

        VkDevice         Device = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkPipeline       Pass1Pipeline = VK_NULL_HANDLE;
        VkPipeline       Pass2Pipeline = VK_NULL_HANDLE;
    };
} // namespace MDSS
