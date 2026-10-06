/**
 * @file GPUResourceMemoryStats.h
 * @brief 앱이 직접 할당한 Vulkan buffer/image 메모리 사용량을 집계한다.
 */

#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>

namespace MDSS::GPU
{
    enum class TGPUBufferMemoryCategory : std::uint32_t
    {
        SurfaceState,
        SurfaceGeometry,
        SurfaceProfile,
        MeshAsset,
        Rendering,
        Debug,
        Other,
        Count
    };

    inline constexpr std::size_t GPUBufferMemoryCategoryCount =
        static_cast<std::size_t>(TGPUBufferMemoryCategory::Count);

    struct TGPUResourceMemorySnapshot
    {
        std::uint64_t CurrentBufferBytes = 0;
        std::uint64_t PeakBufferBytes = 0;
        std::uint64_t CurrentImageBytes = 0;
        std::uint64_t PeakImageBytes = 0;
        std::uint64_t CurrentTotalBytes = 0;
        std::uint64_t PeakTotalBytes = 0;
        std::array<std::uint64_t, GPUBufferMemoryCategoryCount> BufferCategoryCurrentBytes{};
        std::array<std::uint64_t, GPUBufferMemoryCategoryCount> BufferCategoryPeakBytes{};
    };

    /** @brief Separate Vulkan allocations owned by TGPUBuffer and TGPUImage; excludes swapchain allocations. */
    class TGPUResourceMemoryStats final
    {
    public:
        // GPU resource allocation statistics
        static void BufferAllocated(std::uint64_t Bytes, TGPUBufferMemoryCategory Category) noexcept
        {
            const std::size_t Index = GetCategoryIndex(Category);
            Add(Bytes, BufferCategoryCurrent[Index], BufferCategoryPeak[Index]);
            Add(Bytes, BufferCurrent, BufferPeak);
            UpdatePeak(TotalPeak, TotalCurrent.fetch_add(Bytes, std::memory_order_relaxed) + Bytes);
        }
        static void BufferFreed(std::uint64_t Bytes, TGPUBufferMemoryCategory Category) noexcept
        {
            BufferCategoryCurrent[GetCategoryIndex(Category)].fetch_sub(Bytes, std::memory_order_relaxed);
            BufferCurrent.fetch_sub(Bytes, std::memory_order_relaxed);
            TotalCurrent.fetch_sub(Bytes, std::memory_order_relaxed);
        }
        static void ImageAllocated(std::uint64_t Bytes) noexcept
        {
            Add(Bytes, ImageCurrent, ImagePeak);
            UpdatePeak(TotalPeak, TotalCurrent.fetch_add(Bytes, std::memory_order_relaxed) + Bytes);
        }
        static void ImageFreed(std::uint64_t Bytes) noexcept { Subtract(Bytes, ImageCurrent, TotalCurrent); }

        [[nodiscard]] static TGPUResourceMemorySnapshot GetSnapshot() noexcept
        {
            TGPUResourceMemorySnapshot Snapshot;
            Snapshot.CurrentBufferBytes = BufferCurrent.load(std::memory_order_relaxed);
            Snapshot.PeakBufferBytes = BufferPeak.load(std::memory_order_relaxed);
            Snapshot.CurrentImageBytes = ImageCurrent.load(std::memory_order_relaxed);
            Snapshot.PeakImageBytes = ImagePeak.load(std::memory_order_relaxed);
            Snapshot.CurrentTotalBytes = TotalCurrent.load(std::memory_order_relaxed);
            Snapshot.PeakTotalBytes = TotalPeak.load(std::memory_order_relaxed);
            for (std::size_t Index = 0; Index < GPUBufferMemoryCategoryCount; ++Index)
            {
                Snapshot.BufferCategoryCurrentBytes[Index] =
                    BufferCategoryCurrent[Index].load(std::memory_order_relaxed);
                Snapshot.BufferCategoryPeakBytes[Index] = BufferCategoryPeak[Index].load(std::memory_order_relaxed);
            }
            return Snapshot;
        }

    private:
        static std::size_t GetCategoryIndex(TGPUBufferMemoryCategory Category) noexcept
        {
            const std::size_t Index = static_cast<std::size_t>(Category);
            return Index < GPUBufferMemoryCategoryCount ? Index : static_cast<std::size_t>(TGPUBufferMemoryCategory::Other);
        }

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
        inline static std::array<std::atomic<std::uint64_t>, GPUBufferMemoryCategoryCount> BufferCategoryCurrent{};
        inline static std::array<std::atomic<std::uint64_t>, GPUBufferMemoryCategoryCount> BufferCategoryPeak{};
        inline static std::atomic<std::uint64_t> ImageCurrent{0};
        inline static std::atomic<std::uint64_t> ImagePeak{0};
        inline static std::atomic<std::uint64_t> TotalCurrent{0};
        inline static std::atomic<std::uint64_t> TotalPeak{0};
    };
} // namespace MDSS::GPU
