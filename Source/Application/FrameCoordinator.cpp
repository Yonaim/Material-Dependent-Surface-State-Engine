/**
 * @file FrameCoordinator.cpp
 * @brief 고정 시간 step과 프레임별 Solver 기록·완료 시점을 조율한다.
 */

#include "Application/FrameCoordinator.h"

#include "DebugUI/Core/DebugUI.h"
#include "SurfaceState/SurfaceStateSystem.h"

#include <algorithm>
#include <cmath>

namespace MDSS
{
// ============================================================
// 00. Frame Preparation
// ============================================================
#pragma region 00_Frame_Preparation
// 이 frame이 사용할 입력·시간·완료 자원을 준비한 뒤 기록 단계로 넘긴다.

    FFrameCoordinator::FFrameCoordinator(SurfaceState::FSurfaceStateSystem& InSurfaceStateSystem) noexcept
        : SurfaceStateSystem(InSurfaceStateSystem)
    {
    }

    bool FFrameCoordinator::BeginFrame(FDebugUI& DebugUI, float DeltaTime, bool bSuspendClock)
    {
        SurfaceStateSystem.PrepareScratchStorage();
        const bool bResetState = DebugUI.ConsumeSolverResetRequest();

        if (bResetState)
        {
            SurfaceStateSystem.ResetState();
            ResetClock();
        }

        if (!bSuspendClock)
            Clock.Accumulate(DeltaTime, DebugUI.GetSimulationTimeScale(), bResetState || DebugUI.IsSimulationPaused());
        const double PendingTickBudget = std::max(SurfaceState::MaxRealtimePendingTicks,
                                                  std::ceil(static_cast<double>(DebugUI.GetSimulationTimeScale())));
        DroppedSeconds = static_cast<float>(Clock.LimitPendingSeconds(
            bSuspendClock ? 0.0 : PendingTickBudget * SurfaceState::FixedSimulationStepSeconds));
        if (bSuspendClock)
            SurfaceStateSystem.PrepareTransferWeightCachesForSettingChange();
        return bResetState;
    }

    void FFrameCoordinator::PrepareSteps(FDebugUI& DebugUI, bool bResetState)
    {
        const bool bRequestStep = DebugUI.ConsumeSolverStepRequest();
        MaximumStep = DebugUI.IsAutoSubsteppingEnabled() ? SurfaceStateSystem.GetMaximumStableDeltaTime()
                                                         : SurfaceState::FixedSimulationStepSeconds;
        Steps = bResetState ? std::vector<float>{}
                            : Clock.Consume(MaximumStep,
                                            DebugUI.IsFixedSimulationTimestep(),
                                            DebugUI.IsAutoSubsteppingEnabled(),
                                            DebugUI.IsSimulationPaused(),
                                            bRequestStep);
        if (!DebugUI.IsFixedSimulationTimestep() && !DebugUI.IsAutoSubsteppingEnabled() && !Steps.empty())
            MaximumStep = Steps.front();
    }

#pragma endregion

// ============================================================
// 01. Simulation Recording
// ============================================================
#pragma region 01_Simulation_Recording
// 정해진 step 수와 시간 간격으로 Surface Solver 명령을 기록한다.

    void FFrameCoordinator::RecordSimulation(VkCommandBuffer CommandBuffer,
                                             VkQueryPool     QueryPool,
                                             std::uint32_t   FirstStepQuery,
                                             std::uint32_t   QueriesPerStep)
    {
        for (std::size_t Step = 0; Step < Steps.size(); ++Step)
            SurfaceStateSystem.RecordStep(CommandBuffer,
                                          Steps[Step],
                                          QueryPool,
                                          FirstStepQuery + static_cast<std::uint32_t>(Step) * QueriesPerStep,
                                          Step + 1U == Steps.size());
        StepSerial += Steps.size();
    }

#pragma endregion

// ============================================================
// 02. Clock and State Lifecycle
// ============================================================
#pragma region 02_Clock_and_State_Lifecycle
// 재시작·리셋 시 Solver의 시간 누적과 실행 serial을 함께 맞춘다.

    void FFrameCoordinator::ResetClock() noexcept
    {
        Clock.Reset();
        Steps.clear();
        StepSerial = 0;
        MaximumStep = SurfaceState::FixedSimulationStepSeconds;
        DroppedSeconds = 0.0F;
    }

    void FFrameCoordinator::RestartState()
    {
        SurfaceStateSystem.RestartState();
        ResetClock();
    }
#pragma endregion

}
