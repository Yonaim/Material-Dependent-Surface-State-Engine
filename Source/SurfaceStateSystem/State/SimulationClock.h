#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace MDSS
{
    inline constexpr std::uint32_t MaxSimulationStepsPerFrame = 8;
    inline constexpr float FixedSimulationStepSeconds = 1.0F / 60.0F;

    class TSimulationClock
    {
    public:
        void Accumulate(double ElapsedSeconds, double TimeScale, bool bPaused)
        {
            if (!std::isfinite(ElapsedSeconds) || ElapsedSeconds < 0.0 ||
                !std::isfinite(TimeScale) || TimeScale < 0.0)
                throw std::invalid_argument("Invalid simulation elapsed time or time scale.");
            if (!bPaused) PendingSeconds += ElapsedSeconds * TimeScale;
        }

        [[nodiscard]] std::vector<float> Consume(float MaximumStep, bool bFixed, bool bPaused, bool bSingleStep)
        {
            if (!std::isfinite(MaximumStep) || MaximumStep <= 0.0F)
                throw std::invalid_argument("Invalid simulation step limit.");
            std::vector<float> Steps;
            if (bPaused)
            {
                if (bSingleStep) Steps.push_back(MaximumStep);
            }
            else
            {
                const double StepTolerance = double(MaximumStep) * 1.0e-5;
                while (Steps.size() < MaxSimulationStepsPerFrame && PendingSeconds > 0.0)
                {
                    if (bFixed && PendingSeconds + StepTolerance < MaximumStep) break;
                    const double BudgetStep = bFixed ? double(MaximumStep) :
                        std::min(PendingSeconds, double(MaximumStep));
                    Steps.push_back(static_cast<float>(BudgetStep));
                    PendingSeconds = std::max(0.0, PendingSeconds - BudgetStep);
                }
            }
            for (float Step : Steps) SimulatedSeconds += Step;
            return Steps;
        }

        void Reset() noexcept { PendingSeconds = 0.0; SimulatedSeconds = 0.0; }
        [[nodiscard]] double GetPendingSeconds() const noexcept { return PendingSeconds; }
        [[nodiscard]] double GetSimulatedSeconds() const noexcept { return SimulatedSeconds; }

    private:
        double PendingSeconds = 0.0;
        double SimulatedSeconds = 0.0;
    };
}
