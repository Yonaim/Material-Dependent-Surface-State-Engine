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
    class FAssetManager;
}

namespace MDSS::SurfaceState
{
    struct FSurfaceScratchStorageFormat
    {
        bool bCacheRawFlux = true;
        bool bHalfRawFlux = false;
        bool bHalfWeights = false;
        bool bCompact = false;
    };

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
    class FSurfaceInstanceGPUResources final
    {
    public:
        // Instance state buffer creation
        /**
         * @brief instance의 State·Solver 임시 버퍼를 할당한다.
         * @param bResearchReactionLedger 연구용 Rule extent 장부 N×M float를 추가 할당할지 여부.
         * @note Solver의 Reaction ledger flag는 이 추가 공간을 할당한 instance에서만 사용한다.
         */
        FSurfaceInstanceGPUResources(VkPhysicalDevice                    PhysicalDevice,
                                     VkDevice                            Device,
                                     std::size_t                         TexelCount,
                                     std::size_t                         ChannelCount,
                                     const std::vector<float>&           TransferWeights,
                                     const std::vector<FSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                     const std::vector<float>&           WorldTexelAreas = {},
                                     FSurfaceGPUBufferPool*              Pool = nullptr,
                                     FSurfaceScratchStorageFormat        Storage = {},
                                     std::uint32_t                       MaxReactionCount = 0,
                                     bool                                bResearchReactionLedger = false);
        void RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept;

        // Caller must wait for GPU completion and rebind descriptors when this returns true.
        bool ConfigureScratchStorage(VkPhysicalDevice PhysicalDevice,
                                     VkDevice         Device,
                                     bool             bCache,
                                     bool             bHalfRaw,
                                     bool             bHalfWeights,
                                     bool             bCompact);

        [[nodiscard]] const GPU::FGPUBuffer& GetReactionScratchBuffer() const noexcept
        {
            return *ReactionScratchBuffer;
        }

        [[nodiscard]] std::uint32_t GetReactionCapacity() const noexcept
        {
            return ReactionCapacity;
        }

        // State and geometry buffers
        [[nodiscard]] const GPU::FGPUBuffer& GetStateABuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetStateBBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetOutgoingFluxScaleBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetInputDeltaBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetTransferWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetTransferWeightDebugAverageBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetRawOutgoingBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetWorldTexelAreaBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetDynamicGeometryBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetDynamicConcavityWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetAccumulationHeightBuffer() const noexcept;

        // Instance data updates and reset
        void UpdateWorldTexelAreas(const std::vector<float>& WorldTexelAreas);
        void UpdateTransferWeights(const std::vector<float>&           TransferWeights,
                                   const std::vector<FSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                   bool                                bHalfPrecision = false);
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;
        [[nodiscard]] std::size_t GetChannelCount() const noexcept;
        void                      ResetState(const std::vector<float>& OutgoingFluxScale);

    private:
        std::unique_ptr<GPU::FGPUBuffer> ReactionScratchBuffer;
        std::uint32_t                    ReactionCapacity = 0;
        std::size_t                      TexelCount = 0;
        std::size_t                      ChannelCount = 0;
        std::size_t                      ScalarCount = 0;
        bool                             bStoredHalfWeights = false;
        std::unique_ptr<GPU::FGPUBuffer> StateABuffer;
        std::unique_ptr<GPU::FGPUBuffer> StateBBuffer;
        std::unique_ptr<GPU::FGPUBuffer> OutgoingFluxScaleBuffer;
        std::unique_ptr<GPU::FGPUBuffer> InputDeltaBuffer;
        std::unique_ptr<GPU::FGPUBuffer> TransferWeightBuffer;
        std::unique_ptr<GPU::FGPUBuffer> TransferWeightDebugAverageBuffer;
        std::unique_ptr<GPU::FGPUBuffer> RawOutgoingBuffer;
        std::unique_ptr<GPU::FGPUBuffer> WorldTexelAreaBuffer;
        std::unique_ptr<GPU::FGPUBuffer> DynamicGeometryBuffer;
        std::unique_ptr<GPU::FGPUBuffer> DynamicConcavityWeightBuffer;
        std::unique_ptr<GPU::FGPUBuffer> AccumulationHeightBuffer;
    };

} // namespace MDSS::SurfaceState
