/**
 * @file InstanceResources.h
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
    class TSurfaceInstanceGPUResources final
    {
    public:
        // Instance state buffer creation
        TSurfaceInstanceGPUResources(VkPhysicalDevice                    PhysicalDevice,
                                     VkDevice                            Device,
                                     std::size_t                         TexelCount,
                                     std::size_t                         ChannelCount,
                                     const std::vector<float>&           TransferWeights,
                                     const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                     const std::vector<float>&           WorldTexelAreas = {},
                                     TSurfaceGPUBufferPool*              Pool = nullptr);
        void RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept;

        // State and geometry buffers
        [[nodiscard]] const GPU::TGPUBuffer& GetStateABuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetStateBBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetOutgoingFluxScaleBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetInputDeltaBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTransferWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTransferWeightDebugAverageBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetRawOutgoingBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetWorldTexelAreaBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetDynamicGeometryBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetDynamicConcavityWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetAccumulationHeightBuffer() const noexcept;

        // Instance data updates and reset
        void UpdateWorldTexelAreas(const std::vector<float>& WorldTexelAreas);
        void UpdateTransferWeights(const std::vector<float>&           TransferWeights,
                                   const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                   bool                                bHalfPrecision = false);
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;
        [[nodiscard]] std::size_t GetChannelCount() const noexcept;
        void                      ResetState(const std::vector<float>& OutgoingFluxScale);

    private:
        std::size_t                      TexelCount = 0;
        std::size_t                      ChannelCount = 0;
        std::size_t                      ScalarCount = 0;
        std::unique_ptr<GPU::TGPUBuffer> StateABuffer;
        std::unique_ptr<GPU::TGPUBuffer> StateBBuffer;
        std::unique_ptr<GPU::TGPUBuffer> OutgoingFluxScaleBuffer;
        std::unique_ptr<GPU::TGPUBuffer> InputDeltaBuffer;
        std::unique_ptr<GPU::TGPUBuffer> TransferWeightBuffer;
        std::unique_ptr<GPU::TGPUBuffer> TransferWeightDebugAverageBuffer;
        std::unique_ptr<GPU::TGPUBuffer> RawOutgoingBuffer;
        std::unique_ptr<GPU::TGPUBuffer> WorldTexelAreaBuffer;
        std::unique_ptr<GPU::TGPUBuffer> DynamicGeometryBuffer;
        std::unique_ptr<GPU::TGPUBuffer> DynamicConcavityWeightBuffer;
        std::unique_ptr<GPU::TGPUBuffer> AccumulationHeightBuffer;
    };

} // namespace MDSS::SurfaceState
