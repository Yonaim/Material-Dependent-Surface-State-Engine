/**
 * @file ProfileResources.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace MDSS::Asset
{
    class TAssetManager;
}
namespace MDSS::SurfaceState
{
    class TSurfaceDataManager;
    class TSurfaceGPUBufferPool;
}
namespace MDSS::GPU
{
    class TVulkanContext;
}

namespace MDSS
{
    struct TTransform;
    class TScene;
}

namespace MDSS::SurfaceState
{
    class TSurfaceProfileGPUResources final
    {
    public:
        // Profile table creation and access
        TSurfaceProfileGPUResources(VkPhysicalDevice                                PhysicalDevice,
                                    VkDevice                                        Device,
                                    const std::vector<TSurfaceResponseProfileData>& Profiles,
                                    const TSurfaceStateRegistry&                    Registry,
                                    TSurfaceGPUBufferPool*                          Pool = nullptr);
        [[nodiscard]] const GPU::TGPUBuffer& GetReactionsBuffer() const noexcept
        {
            return *ReactionsBuffer;
        }
        [[nodiscard]] std::uint32_t GetMaxReactionCount() const noexcept
        {
            return MaxReactionCount;
        }
        void RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept;

        [[nodiscard]] const GPU::TGPUBuffer& GetParametersBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetSupportedBuffer() const noexcept;
        [[nodiscard]] std::size_t            GetProfileCount() const noexcept;
        [[nodiscard]] std::size_t            GetChannelCount() const noexcept;
        [[nodiscard]] bool IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept;
        void
        UpdateParameters(std::size_t ProfileIndex, std::size_t ChannelIndex, const TSurfaceStateParameters& Parameters);

    private:
        std::unique_ptr<GPU::TGPUBuffer> ReactionsBuffer;
        std::uint32_t                    MaxReactionCount = 0;
        std::size_t                      ProfileCount = 0;
        std::size_t                      ChannelCount = 0;
        std::vector<std::uint32_t>       SupportedChannels;
        std::unique_ptr<GPU::TGPUBuffer> ParametersBuffer;
        std::unique_ptr<GPU::TGPUBuffer> SupportedBuffer;
    };

} // namespace MDSS::SurfaceState
