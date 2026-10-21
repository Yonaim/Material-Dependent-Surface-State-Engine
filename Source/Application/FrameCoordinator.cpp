#include "Application/FrameCoordinator.h"

#include "DebugUI/DebugUI.h"
#include "SurfaceState/SurfaceStateSystem.h"

#include <algorithm>
#include <cmath>

namespace MDSS
{
    TFrameCoordinator::TFrameCoordinator(SurfaceState::TSurfaceStateSystem& InSurfaceStateSystem) noexcept
        : SurfaceStateSystem(InSurfaceStateSystem)
    {
    }

    bool TFrameCoordinator::BeginFrame(TDebugUI& DebugUI, float DeltaTime, bool bSuspendClock)
    {
        const bool bResetState = DebugUI.ConsumeSolverResetRequest();
        if (bResetState)
        {
            SurfaceStateSystem.ResetState();
            ResetClock();
        }
        if (!bSuspendClock)
            Clock.Accumulate(DeltaTime, DebugUI.GetSimulationTimeScale(),
                             bResetState || DebugUI.IsSimulationPaused());
        const double PendingTickBudget =
            std::max(SurfaceState::MaxRealtimePendingTicks,
                     std::ceil(static_cast<double>(DebugUI.GetSimulationTimeScale())));
        DroppedSeconds = static_cast<float>(Clock.LimitPendingSeconds(
            bSuspendClock ? 0.0 : PendingTickBudget * SurfaceState::FixedSimulationStepSeconds));
        if (bSuspendClock)
            SurfaceStateSystem.PrepareTransferWeightCachesForSettingChange();
        return bResetState;
    }

    void TFrameCoordinator::PrepareSteps(TDebugUI& DebugUI, bool bResetState)
    {
        const bool bRequestStep = DebugUI.ConsumeSolverStepRequest();
        MaximumStep = DebugUI.IsAutoSubsteppingEnabled() ? SurfaceStateSystem.GetMaximumStableDeltaTime()
                                                         : SurfaceState::FixedSimulationStepSeconds;
        Steps = bResetState ? std::vector<float>{}
                            : Clock.Consume(MaximumStep, DebugUI.IsFixedSimulationTimestep(),
                                            DebugUI.IsAutoSubsteppingEnabled(), DebugUI.IsSimulationPaused(),
                                            bRequestStep);
        if (!DebugUI.IsFixedSimulationTimestep() && !DebugUI.IsAutoSubsteppingEnabled() && !Steps.empty())
            MaximumStep = Steps.front();
    }

    void TFrameCoordinator::RecordSimulation(VkCommandBuffer CommandBuffer, VkQueryPool QueryPool,
                                              std::uint32_t FirstStepQuery, std::uint32_t QueriesPerStep)
    {
        for (std::size_t Step = 0; Step < Steps.size(); ++Step)
            SurfaceStateSystem.RecordStep(CommandBuffer, Steps[Step], QueryPool,
                                     FirstStepQuery + static_cast<std::uint32_t>(Step) * QueriesPerStep,
                                     Step + 1U == Steps.size());
        StepSerial += Steps.size();
    }

    void TFrameCoordinator::ResetClock() noexcept
    {
        Clock.Reset();
        Steps.clear();
        StepSerial = 0;
        MaximumStep = SurfaceState::FixedSimulationStepSeconds;
        DroppedSeconds = 0.0F;
    }

    void TFrameCoordinator::RestartState()
    {
        SurfaceStateSystem.RestartState();
        ResetClock();
    }
}
