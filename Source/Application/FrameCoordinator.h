#pragma once

#include "SurfaceState/Solver/SimulationClock.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace MDSS
{
    class FDebugUI;
}
namespace MDSS::SurfaceState
{
    class FSurfaceStateSystem;
}

namespace MDSS
{
    // Owns simulation time and records simulation work into the frame command buffer.
    class FFrameCoordinator final
    {
    public:
        explicit FFrameCoordinator(SurfaceState::FSurfaceStateSystem& SurfaceStateSystem) noexcept;

        // Called after the frame fence wait, before swapchain acquisition.
        bool BeginFrame(FDebugUI& DebugUI, float DeltaTime, bool bSuspendClock);
        // Consume time only after an image was acquired so skipped frames retain their budget.
        void PrepareSteps(FDebugUI& DebugUI, bool bResetState);
        void RecordSimulation(VkCommandBuffer CommandBuffer,
                              VkQueryPool     QueryPool,
                              std::uint32_t   FirstStepQuery,
                              std::uint32_t   QueriesPerStep);
        void ResetClock() noexcept;
        void RestartState();

        [[nodiscard]] const std::vector<float>& GetSteps() const noexcept
        {
            return Steps;
        }
        [[nodiscard]] std::uint32_t GetStepCount() const noexcept
        {
            return static_cast<std::uint32_t>(Steps.size());
        }
        [[nodiscard]] double GetPendingSeconds() const noexcept
        {
            return Clock.GetPendingSeconds();
        }
        [[nodiscard]] double GetSimulatedSeconds() const noexcept
        {
            return Clock.GetSimulatedSeconds();
        }
        [[nodiscard]] float GetMaximumStep() const noexcept
        {
            return MaximumStep;
        }
        [[nodiscard]] float GetDroppedSeconds() const noexcept
        {
            return DroppedSeconds;
        }
        [[nodiscard]] std::uint64_t GetStepSerial() const noexcept
        {
            return StepSerial;
        }

    private:
        SurfaceState::FSurfaceStateSystem& SurfaceStateSystem;
        SurfaceState::FSimulationClock     Clock;
        std::vector<float>                 Steps;
        std::uint64_t                      StepSerial = 0;
        float                              MaximumStep = SurfaceState::FixedSimulationStepSeconds;
        float                              DroppedSeconds = 0.0F;
    };
}
