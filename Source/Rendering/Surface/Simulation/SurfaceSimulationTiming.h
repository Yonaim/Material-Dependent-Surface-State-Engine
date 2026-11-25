/**
 * @file SurfaceSimulationTiming.h
 * @brief 프레임 슬롯별 GPU 타임스탬프 수집과 시간 통계의 호출 계약을 정의한다.
 */

#pragma once

#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Display Statistics
    // ============================================================
    #pragma region 00_Display_Statistics
        // 유효한 그리기 표본 수와 GPU 실행 시간의 밀리초 단위를 함께 노출한다.

        /** @brief 최근 유효한 그리기 구간 최대 120개의 통계다. 표본이 없으면 모든 값은 0이다. */
        struct FSurfaceSimulationTimingStats
        {
            double      MeanMilliseconds = 0.0;
            double      P90Milliseconds = 0.0;
            double      LatestMilliseconds = 0.0;
            std::size_t Samples = 0;
        };

        struct FSurfaceSimulationDisplayStats
        {
            FSurfaceSimulationTimingStats Solver;
            FSurfaceSimulationTimingStats Snapshot;
            FSurfaceSimulationTimingStats Draw;
            FSurfaceSimulationTimingStats RefractionPrepass;
            FSurfaceSimulationTimingStats ColorPass;
        };

        enum class ESurfaceSimulationTimestamp : std::uint32_t
        {
            SolverEnd = 1,
            SnapshotEnd,
            RefractionPrepassEnd,
            DrawEnd
        };

    #pragma endregion

    // ============================================================
    // 01. Timestamp Queries
    // ============================================================
    #pragma region 01_Timestamp_Queries
        // 프레임 슬롯이 재사용될 때 완료된 쿼리만 읽도록 호출 순서를 정한다.

        /**
         * @brief 두 프레임 슬롯에 각각 시작과 끝 타임스탬프를 저장한다.
         * @pre Context의 Vulkan 장치는 이 객체가 소멸할 때까지 유효해야 한다.
         */
        class FSurfaceSimulationTiming
        {
            public:
            explicit FSurfaceSimulationTiming(const GPU::FVulkanContext &Context);
            ~FSurfaceSimulationTiming();
            FSurfaceSimulationTiming(const FSurfaceSimulationTiming &) = delete;
            FSurfaceSimulationTiming &operator=(const FSurfaceSimulationTiming &) = delete;

            /**
             * @brief 완료된 슬롯의 쿼리를 대기 없이 읽고 워밍업 30표본 이후부터 집계한다.
             * @pre Slot은 0 또는 1이며 해당 프레임의 펜스가 신호를 보낸 뒤 호출한다.
             */
            void Collect(std::uint32_t Slot);
            /**
             * @brief 슬롯의 이전 타임스탬프를 지우고 새 측정 구간을 시작한다.
             * @pre Slot은 0 또는 1이다. 제출된 이전 측정이 있으면 먼저 Collect를 호출한다.
             */
            void Begin(VkCommandBuffer Commands, std::uint32_t Slot, bool bHasSteps, bool bHasRefraction = false);
            /** @brief 같은 명령 버퍼와 Slot의 계산·복사·그리기 구간 경계를 기록한다. */
            void Mark(VkCommandBuffer Commands, std::uint32_t Slot, ESurfaceSimulationTimestamp Stage) const;
            [[nodiscard]] FSurfaceSimulationDisplayStats Stats() const;

            private:
            VkDevice            Device;
            VkQueryPool         Pool = VK_NULL_HANDLE;
            std::uint32_t       ValidBits = 0;
            double              Nanoseconds = 0.0;
            std::array<bool, 2> Submitted{};
            struct FHistory
            {
                std::array<double, 120>                     Milliseconds{};
                std::size_t                                 Count = 0;
                void                                        Add(double Duration);
                [[nodiscard]] FSurfaceSimulationTimingStats Stats() const;
            };

            std::array<bool, 2> HasSteps{};
            std::array<bool, 2> HasRefraction{};
            FHistory            Solver;
            FHistory            Snapshot;
            FHistory            Draw;
            FHistory            RefractionPrepass;
            FHistory            ColorPass;
            std::size_t         Warmup = 0;
        };

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
