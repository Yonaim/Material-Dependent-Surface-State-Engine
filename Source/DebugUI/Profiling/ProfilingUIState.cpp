#include "DebugUI/Profiling/ProfilingUIState.h"

#include <algorithm>
#include <cmath>

namespace MDSS
{
// ============================================================
// 00. Profiling State Transitions
// ============================================================
#pragma region 00_Profiling_State_Transitions

    void FProfilingUIState::Reset() noexcept
    {
        WindowElapsed = FpsSum = FrameTimeSum = 0.0;
        FrameSamples = 0;
        MetricSums.fill(0.0);
        MetricSamples.fill(0);
        Averages.fill(-1.0F);
        Maximums.fill(-1.0F);
        WindowMaximums.fill(-1.0F);
        Recent100msAverages.fill(-1.0F);
        RecentSamples.clear();
        ElapsedSeconds = 0.0;
        bAverageAvailable = false;
    }

    void FProfilingUIState::Accumulate(double FrameSeconds, const std::array<float, 40>& MetricValues)
    {
        ElapsedSeconds += FrameSeconds;
        WindowElapsed += FrameSeconds;
        FpsSum += 1.0 / FrameSeconds;
        FrameTimeSum += FrameSeconds * 1000.0;
        ++FrameSamples;
        FSample Sample;
        Sample.TimeSeconds = ElapsedSeconds;
        Sample.Values.fill(-1.0F);
        Sample.Values[0] = static_cast<float>(1.0 / FrameSeconds);
        Sample.Values[1] = static_cast<float>(FrameSeconds * 1000.0);
        WindowMaximums[0] = std::max(WindowMaximums[0], Sample.Values[0]);
        WindowMaximums[1] = std::max(WindowMaximums[1], Sample.Values[1]);

        for (std::size_t Index = 0; Index < MetricValues.size(); ++Index)
        {
            if (std::isfinite(MetricValues[Index]) && MetricValues[Index] >= 0.0F)
            {
                MetricSums[Index] += MetricValues[Index];
                ++MetricSamples[Index];
                WindowMaximums[Index + 2U] = std::max(WindowMaximums[Index + 2U], MetricValues[Index]);
                Sample.Values[Index + 2U] = MetricValues[Index];
            }
        }

        RecentSamples.push_back(Sample);

        while (!RecentSamples.empty() && RecentSamples.front().TimeSeconds < ElapsedSeconds - 0.1)
            RecentSamples.pop_front();
        std::array<double, 43>        RecentSums{};
        std::array<std::uint32_t, 43> RecentCounts{};

        for (const FSample& RecentSample : RecentSamples)
        {
            for (std::size_t Index = 0; Index < RecentSample.Values.size(); ++Index)
            {
                const float Value = RecentSample.Values[Index];

                if (std::isfinite(Value) && Value >= 0.0F)
                {
                    RecentSums[Index] += Value;
                    ++RecentCounts[Index];
                }
            }
        }

        Recent100msAverages.fill(-1.0F);

        for (std::size_t Index = 0; Index < Recent100msAverages.size(); ++Index)
        {
            if (RecentCounts[Index] != 0U)
                Recent100msAverages[Index] = static_cast<float>(RecentSums[Index] / RecentCounts[Index]);
        }
    }

    void FProfilingUIState::PublishWindow()
    {
        if (WindowElapsed >= 1.0 && FrameSamples > 0)
        {
            Averages[0] = static_cast<float>(FpsSum / FrameSamples);
            Averages[1] = static_cast<float>(FrameTimeSum / FrameSamples);

            for (std::size_t Index = 0; Index < MetricSums.size(); ++Index)
            {
                Averages[Index + 2] =
                    MetricSamples[Index] > 0 ? static_cast<float>(MetricSums[Index] / MetricSamples[Index]) : -1.0F;
            }

            Maximums = WindowMaximums;
            bAverageAvailable = true;
            WindowElapsed = 0.0;
            FpsSum = 0.0;
            FrameTimeSum = 0.0;
            FrameSamples = 0;
            MetricSums.fill(0.0);
            MetricSamples.fill(0U);
            WindowMaximums.fill(-1.0F);
        }
    }

#pragma endregion
} // namespace MDSS
