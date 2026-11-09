/**
 * @file ProfilingUIState.h
 * @brief UI 계측의 누적 창, 최근 샘플과 표시용 집계 상태를 소유한다.
 */
#pragma once

#include <array>
#include <cstdint>
#include <deque>

namespace MDSS
{
    struct FProfilingUIState
    {
        struct FSample
        {
            double                TimeSeconds = 0.0;
            std::array<float, 43> Values{};
        };

        // ============================================================
        // 00. State Transitions
        // ============================================================
#pragma region 00_State_Transitions

        /** @brief 기존 UI reset 정책대로 평균과 최대값을 미측정(-1)으로 되돌린다. */
        void Reset() noexcept;
        /** @brief 유한한 양수 frame 시간과 현재 표시 mode에 맞는 metric만 호출자가 전달한다. */
        void Accumulate(double FrameSeconds, const std::array<float, 40>& MetricValues);
        /** @brief 누적 창이 1초에 도달했으면 평균/최대를 게시하고 현재 창만 비운다. */
        void PublishWindow();

#pragma endregion

        // ============================================================
        // 01. Owned Samples and Aggregates
        // ============================================================
#pragma region 01_Owned_Samples_and_Aggregates

        double                        WindowElapsed = 0.0;
        double                        ElapsedSeconds = 0.0;
        double                        FpsSum = 0.0;
        double                        FrameTimeSum = 0.0;
        std::uint32_t                 FrameSamples = 0;
        std::array<double, 40>        MetricSums{};
        std::array<std::uint32_t, 40> MetricSamples{};
        std::array<float, 43>         Averages{};
        std::array<float, 43>         Maximums = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);

            return Values;
        }();
        std::array<float, 43> WindowMaximums = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);

            return Values;
        }();
        std::array<float, 43> Recent100msAverages = []
        {
            std::array<float, 43> Values{};
            Values.fill(-1.0F);

            return Values;
        }();
        std::deque<FSample> RecentSamples;
        bool                bAverageAvailable = false;

#pragma endregion
    };
} // namespace MDSS
