/**
 * @file GPUResourceMemoryStats.h
 * @brief 앱이 직접 할당한 Vulkan buffer/image 메모리 사용량을 집계한다.
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace MDSS::GPU
{
    struct TGPUResourceMemorySnapshot
    {
        std::uint64_t CurrentBufferBytes = 0;
        std::uint64_t PeakBufferBytes = 0;
        std::uint64_t CurrentImageBytes = 0;
        std::uint64_t PeakImageBytes = 0;
        std::uint64_t CurrentTotalBytes = 0;
        std::uint64_t PeakTotalBytes = 0;
    };

    /** @brief Separate Vulkan allocations owned by TGPUBuffer and TGPUImage; excludes swapchain allocations. */
    class TGPUResourceMemoryStats final
    {
    public:
        static void BufferAllocated(std::uint64_t Bytes) noexcept { Add(Bytes, BufferCurrent, BufferPeak); }
        static void BufferFreed(std::uint64_t Bytes) noexcept { Subtract(Bytes, BufferCurrent, TotalCurrent); }
        static void ImageAllocated(std::uint64_t Bytes) noexcept { Add(Bytes, ImageCurrent, ImagePeak); }
        static void ImageFreed(std::uint64_t Bytes) noexcept { Subtract(Bytes, ImageCurrent, TotalCurrent); }

        [[nodiscard]] static TGPUResourceMemorySnapshot GetSnapshot() noexcept
        {
            return {BufferCurrent.load(std::memory_order_relaxed),
                    BufferPeak.load(std::memory_order_relaxed),
                    ImageCurrent.load(std::memory_order_relaxed),
                    ImagePeak.load(std::memory_order_relaxed),
                    TotalCurrent.load(std::memory_order_relaxed),
                    TotalPeak.load(std::memory_order_relaxed)};
        }

    private:
        static void UpdatePeak(std::atomic<std::uint64_t>& Peak, std::uint64_t Value) noexcept
        {
            std::uint64_t Previous = Peak.load(std::memory_order_relaxed);
            while (Previous < Value &&
                   !Peak.compare_exchange_weak(Previous, Value, std::memory_order_relaxed, std::memory_order_relaxed))
            {
            }
        }

        static void Add(std::uint64_t Bytes,
                        std::atomic<std::uint64_t>& Current,
                        std::atomic<std::uint64_t>& Peak) noexcept
        {
            UpdatePeak(Peak, Current.fetch_add(Bytes, std::memory_order_relaxed) + Bytes);
            UpdatePeak(TotalPeak, TotalCurrent.fetch_add(Bytes, std::memory_order_relaxed) + Bytes);
        }

        static void Subtract(std::uint64_t Bytes,
                             std::atomic<std::uint64_t>& Current,
                             std::atomic<std::uint64_t>& Total) noexcept
        {
            Current.fetch_sub(Bytes, std::memory_order_relaxed);
            Total.fetch_sub(Bytes, std::memory_order_relaxed);
        }

        inline static std::atomic<std::uint64_t> BufferCurrent{0};
        inline static std::atomic<std::uint64_t> BufferPeak{0};
        inline static std::atomic<std::uint64_t> ImageCurrent{0};
        inline static std::atomic<std::uint64_t> ImagePeak{0};
        inline static std::atomic<std::uint64_t> TotalCurrent{0};
        inline static std::atomic<std::uint64_t> TotalPeak{0};
    };
} // namespace MDSS::GPU
