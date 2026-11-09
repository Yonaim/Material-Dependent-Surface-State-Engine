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
    class FAssetManager;
}
namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
    class FSurfaceGPUBufferPool;
}
namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    struct FTransform;
    class FScene;
}

namespace MDSS::SurfaceState
{
    class FSurfaceProfileGPUResources final
    {
    public:
        // Profile table creation and access
        FSurfaceProfileGPUResources(VkPhysicalDevice                                PhysicalDevice,
                                    VkDevice                                        Device,
                                    const std::vector<FSurfaceResponseProfileData>& Profiles,
                                    const FSurfaceStateRegistry&                    Registry,
                                    FSurfaceGPUBufferPool*                          Pool = nullptr);
        [[nodiscard]] const GPU::FGPUBuffer& GetReactionsBuffer() const noexcept
        {
            return *ReactionsBuffer;
        }
        [[nodiscard]] std::uint32_t GetMaxReactionCount() const noexcept
        {
            return MaxReactionCount;
        }
        void RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept;

        [[nodiscard]] const GPU::FGPUBuffer& GetParametersBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetSupportedBuffer() const noexcept;
        [[nodiscard]] std::size_t            GetProfileCount() const noexcept;
        [[nodiscard]] std::size_t            GetChannelCount() const noexcept;
        [[nodiscard]] bool IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept;
        void
        UpdateParameters(std::size_t ProfileIndex, std::size_t ChannelIndex, const FSurfaceStateParameters& Parameters);

    private:
        std::unique_ptr<GPU::FGPUBuffer> ReactionsBuffer;
        std::uint32_t                    MaxReactionCount = 0;
        std::size_t                      ProfileCount = 0;
        std::size_t                      ChannelCount = 0;
        std::vector<std::uint32_t>       SupportedChannels;
        std::unique_ptr<GPU::FGPUBuffer> ParametersBuffer;
        std::unique_ptr<GPU::FGPUBuffer> SupportedBuffer;
    };

} // namespace MDSS::SurfaceState
