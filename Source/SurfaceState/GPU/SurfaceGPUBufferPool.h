#pragma once

#include "GPU/Vulkan/Resource/GPUBuffer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace MDSS::SurfaceState
{
    /** Retains completed Scene buffers for an exact-size, exact-usage reuse on a later reload. */
    class TSurfaceGPUBufferPool final
    {
    public:
        static constexpr VkDeviceSize MaximumRetainedBytes = 256ULL * 1024ULL * 1024ULL;

        [[nodiscard]] std::unique_ptr<GPU::TGPUBuffer> Acquire(VkDeviceSize                  Size,
                                                               VkBufferUsageFlags            Usage,
                                                               VkMemoryPropertyFlags         MemoryProperties,
                                                               GPU::TGPUBufferMemoryCategory Category) noexcept
        {
            const auto Found = std::find_if(Buffers.begin(),
                                            Buffers.end(),
                                            [&](const auto& Buffer)
                                            {
                                                return Buffer->GetSize() == Size && Buffer->GetUsage() == Usage &&
                                                       Buffer->GetMemoryProperties() == MemoryProperties &&
                                                       Buffer->GetMemoryCategory() == Category;
                                            });
            if (Found == Buffers.end())
                return nullptr;
            RetainedBytes -= (*Found)->GetSize();
            auto Buffer = std::move(*Found);
            Buffers.erase(Found);
            ++ReuseCount;
            ReusedBytes += Size;
            return Buffer;
        }

        void Release(std::unique_ptr<GPU::TGPUBuffer> Buffer) noexcept
        {
            if (!Buffer || Buffer->GetSize() > MaximumRetainedBytes)
                return;
            while (RetainedBytes > MaximumRetainedBytes - Buffer->GetSize() && !Buffers.empty())
            {
                RetainedBytes -= Buffers.front()->GetSize();
                Buffers.erase(Buffers.begin());
            }
            try
            {
                RetainedBytes += Buffer->GetSize();
                Buffers.push_back(std::move(Buffer));
            }
            catch (...)
            {
                RetainedBytes -= Buffer->GetSize();
            }
        }

        void Clear() noexcept
        {
            Buffers.clear();
            RetainedBytes = 0;
        }

        [[nodiscard]] VkDeviceSize GetRetainedBytes() const noexcept
        {
            return RetainedBytes;
        }
        [[nodiscard]] std::uint64_t GetReuseCount() const noexcept
        {
            return ReuseCount;
        }
        [[nodiscard]] VkDeviceSize GetReusedBytes() const noexcept
        {
            return ReusedBytes;
        }

    private:
        std::vector<std::unique_ptr<GPU::TGPUBuffer>> Buffers;
        VkDeviceSize                                  RetainedBytes = 0;
        std::uint64_t                                 ReuseCount = 0;
        VkDeviceSize                                  ReusedBytes = 0;
    };
} // namespace MDSS::SurfaceState
