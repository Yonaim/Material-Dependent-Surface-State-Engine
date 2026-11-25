/**
 * @file SurfaceSimulationTiming.cpp
 * @brief Vulkan 타임스탬프 차이를 시간으로 변환하고 최근 그리기 시간을 집계한다.
 */

#include "Rendering/Surface/Simulation/SurfaceSimulationTiming.h"
#include "GPU/Vulkan/VulkanContext.h"
#include <algorithm>
#include <numeric>
#include <vector>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Query Pool Lifetime
    // ============================================================
    #pragma region 00_Query_Pool_Lifetime
        namespace
        {
            constexpr auto TimestampsPerSlot = static_cast<std::uint32_t>(ESurfaceSimulationTimestamp::DrawEnd) + 1U;
        }
        // 그래픽 큐에 유효 타임스탬프 비트가 있을 때만 쿼리 풀을 준비한다.

        FSurfaceSimulationTiming::FSurfaceSimulationTiming(const GPU::FVulkanContext &Context)
            : Device(Context.GetDevice())
        {
            const auto    PhysicalDevice = Context.GetPhysicalDevice();
            std::uint32_t FamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &FamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> Families(FamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &FamilyCount, Families.data());
            ValidBits = Families[Context.GetQueues().GetFamilyIndices().GraphicsFamily.value()].timestampValidBits;
            if (ValidBits == 0)
                return;
            VkPhysicalDeviceProperties Properties;
            vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
            Nanoseconds = Properties.limits.timestampPeriod;
            VkQueryPoolCreateInfo Info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            Info.queryType = VK_QUERY_TYPE_TIMESTAMP;
            Info.queryCount = 2 * TimestampsPerSlot;
            if (vkCreateQueryPool(Device, &Info, nullptr, &Pool) != VK_SUCCESS)
                Pool = VK_NULL_HANDLE;
        }

        FSurfaceSimulationTiming::~FSurfaceSimulationTiming()
        {
            vkDestroyQueryPool(Device, Pool, nullptr);
        }

    #pragma endregion

    // ============================================================
    // 01. Frame Slot Sampling
    // ============================================================
    #pragma region 01_Frame_Slot_Sampling
        // 슬롯마다 두 쿼리를 쓰고 완료된 차이만 밀리초로 누적한다.

        void FSurfaceSimulationTiming::Collect(std::uint32_t Slot)
        {
            if (!Pool || !Submitted[Slot])
                return;
            Submitted[Slot] = false;
            std::array<std::uint64_t, TimestampsPerSlot> Ticks{};
            if (vkGetQueryPoolResults(Device,
                                      Pool,
                                      Slot * TimestampsPerSlot,
                                      TimestampsPerSlot,
                                      sizeof(Ticks),
                                      Ticks.data(),
                                      sizeof(Ticks.front()),
                                      VK_QUERY_RESULT_64_BIT) != VK_SUCCESS)
                return;
            if (Warmup < 30)
            {
                ++Warmup;
                return;
            }
            const auto Milliseconds = [&](std::size_t First, std::size_t Last)
            {
                const std::uint64_t Mask = ValidBits < 64 ? (std::uint64_t{1} << ValidBits) - 1 : UINT64_MAX;
                return static_cast<double>((Ticks[Last] - Ticks[First]) & Mask) * Nanoseconds * 1.e-6;
            };
            if (HasSteps[Slot])
            {
                Solver.Add(Milliseconds(0, 1));
                Snapshot.Add(Milliseconds(1, 2));
            }
            Draw.Add(Milliseconds(2, 4));
            if (HasRefraction[Slot])
                RefractionPrepass.Add(Milliseconds(2, 3));
            else
                RefractionPrepass = {};
            ColorPass.Add(Milliseconds(3, 4));
        }

        void
        FSurfaceSimulationTiming::Begin(VkCommandBuffer Commands, std::uint32_t Slot, bool bHasSteps, bool bHasRefraction)
        {
            if (!Pool)
                return;
            vkCmdResetQueryPool(Commands, Pool, Slot * TimestampsPerSlot, TimestampsPerSlot);
            vkCmdWriteTimestamp(Commands, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool, Slot * TimestampsPerSlot);
            HasSteps[Slot] = bHasSteps;
            HasRefraction[Slot] = bHasRefraction;
            Submitted[Slot] = true;
        }

        void FSurfaceSimulationTiming::Mark(VkCommandBuffer             Commands,
                                            std::uint32_t               Slot,
                                            ESurfaceSimulationTimestamp Stage) const
        {
            if (Pool)
                vkCmdWriteTimestamp(Commands,
                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    Pool,
                                    Slot * TimestampsPerSlot + static_cast<std::uint32_t>(Stage));
        }

    #pragma endregion

    // ============================================================
    // 02. Statistics
    // ============================================================
    #pragma region 02_Statistics
        // 최근 유효 표본의 평균, 90백분위, 마지막 값을 계산한다.

        FSurfaceSimulationDisplayStats FSurfaceSimulationTiming::Stats() const
        {
            return {Solver.Stats(), Snapshot.Stats(), Draw.Stats(), RefractionPrepass.Stats(), ColorPass.Stats()};
        }

        void FSurfaceSimulationTiming::FHistory::Add(double Duration)
        {
            Milliseconds[Count % Milliseconds.size()] = Duration;
            ++Count;
        }

        FSurfaceSimulationTimingStats FSurfaceSimulationTiming::FHistory::Stats() const
        {
            const auto Samples = std::min(Count, Milliseconds.size());
            if (Samples == 0)
                return {};
            FSurfaceSimulationTimingStats Result;
            Result.Samples = Samples;
            Result.LatestMilliseconds = Milliseconds[(Count - 1) % Milliseconds.size()];
            Result.MeanMilliseconds = std::accumulate(Milliseconds.begin(), Milliseconds.begin() + Samples, 0.0) / Samples;
            auto Sorted = Milliseconds;
            std::sort(Sorted.begin(), Sorted.begin() + Samples);
            Result.P90Milliseconds = Sorted[(Samples - 1) * 9 / 10];
            return Result;
        }

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
