/**
 * @file SimulationClock.h
 * @brief Surface simulation step의 시간 간격과 누적 시간을 관리하는 자료형을 선언한다.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace MDSS::SurfaceState
{
    inline constexpr std::uint32_t MaxSimulationStepsPerFrame = 8;
    inline constexpr double        MaxRealtimePendingTicks = 4.0;
    inline constexpr float         FixedSimulationStepSeconds = 1.0F / 60.0F;
    inline constexpr bool          DefaultFixedSimulationTimestep = true;
    inline constexpr bool          DefaultAutoSubstepping = false;

    class TSimulationClock
    {
    public:
        void Accumulate(double ElapsedSeconds, double TimeScale, bool bPaused)
        {
            if (!std::isfinite(ElapsedSeconds) || ElapsedSeconds < 0.0 || !std::isfinite(TimeScale) || TimeScale < 0.0)
                throw std::invalid_argument("Invalid simulation elapsed time or time scale.");
            if (!bPaused)
                PendingSeconds += ElapsedSeconds * TimeScale;
        }

        // Interactive playback drops elapsed time that cannot fit in the frame's
        // work budget. Call after Accumulate, before Consume; paused manual steps
        // remain independent of the pending budget.
        double LimitPendingSeconds(double MaximumSeconds)
        {
            if (!std::isfinite(MaximumSeconds) || MaximumSeconds < 0.0)
                throw std::invalid_argument("Invalid maximum pending simulation time.");
            const double DroppedSeconds = std::max(0.0, PendingSeconds - MaximumSeconds);
            PendingSeconds -= DroppedSeconds;
            FixedTickRemainingSeconds = std::min(FixedTickRemainingSeconds, PendingSeconds);
            return DroppedSeconds;
        }

        [[nodiscard]] std::vector<float>
        Consume(float TransportMaximumStep, bool bFixed, bool bAutoSubstepping, bool bPaused, bool bSingleStep)
        {
            if (bAutoSubstepping && (!std::isfinite(TransportMaximumStep) || TransportMaximumStep <= 0.0F))
                throw std::invalid_argument("Invalid simulation step limit.");
            const double FixedStep = FixedSimulationStepSeconds;
            const double MaximumStep = bAutoSubstepping ? std::min(double(TransportMaximumStep), FixedStep) : FixedStep;
            std::vector<float> Steps;
            if (bPaused)
            {
                if (bSingleStep)
                    Steps.push_back(static_cast<float>(MaximumStep));
            }
            else
            {
                if (!bFixed)
                    FixedTickRemainingSeconds = 0.0;
                const double StepTolerance = FixedStep * 1.0e-5;
                while (Steps.size() < MaxSimulationStepsPerFrame && PendingSeconds > 0.0)
                {
                    double BudgetStep;
                    if (bFixed)
                    {
                        if (FixedTickRemainingSeconds == 0.0)
                        {
                            // A fixed tick starts only when its full time budget has accumulated.
                            if (PendingSeconds + StepTolerance < FixedStep)
                                break;
                            FixedTickRemainingSeconds = FixedStep;
                        }
                        BudgetStep = bAutoSubstepping ? std::min(FixedTickRemainingSeconds, MaximumStep)
                                                      : FixedTickRemainingSeconds;
                        FixedTickRemainingSeconds = std::max(0.0, FixedTickRemainingSeconds - BudgetStep);
                    }
                    else
                        BudgetStep = bAutoSubstepping ? std::min(PendingSeconds, MaximumStep) : PendingSeconds;
                    const float Step = static_cast<float>(BudgetStep);
                    Steps.push_back(Step);
                    PendingSeconds = std::max(0.0, PendingSeconds - BudgetStep);
                }
            }
            for (float Step : Steps)
                SimulatedSeconds += Step;
            return Steps;
        }

        void Reset() noexcept
        {
            PendingSeconds = 0.0;
            SimulatedSeconds = 0.0;
            FixedTickRemainingSeconds = 0.0;
        }
        [[nodiscard]] double GetPendingSeconds() const noexcept
        {
            return PendingSeconds;
        }
        [[nodiscard]] double GetSimulatedSeconds() const noexcept
        {
            return SimulatedSeconds;
        }

    private:
        double PendingSeconds = 0.0;
        double SimulatedSeconds = 0.0;
        // This is part of PendingSeconds, not a separate accumulated time budget.
        double FixedTickRemainingSeconds = 0.0;
    };
}
