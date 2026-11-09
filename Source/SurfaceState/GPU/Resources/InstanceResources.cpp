#include "SurfaceState/GPU/Resources/InstanceResources.h"

#include "SurfaceState/GPU/Resources/ResourceBufferHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    using namespace Detail;

#pragma region Instance_State_Resources

    TSurfaceInstanceGPUResources::TSurfaceInstanceGPUResources(
        VkPhysicalDevice                    PhysicalDevice,
        VkDevice                            Device,
        std::size_t                         TexelCount,
        std::size_t                         ChannelCount,
        const std::vector<float>&           TransferWeights,
        const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages,
        const std::vector<float>&           WorldTexelAreas,
        TSurfaceGPUBufferPool*              Pool,
        TSurfaceScratchStorageFormat        Storage,
        std::uint32_t                       MaxReactionCount)
        : TexelCount(TexelCount), ChannelCount(ChannelCount)
    {
        if (TexelCount == 0 || ChannelCount == 0)
        {
            throw std::invalid_argument("Surface Instance GPU resources require texels and State channels.");
        }
        if (TexelCount > std::numeric_limits<std::size_t>::max() / ChannelCount)
        {
            throw std::overflow_error("Surface Instance GPU scalar count overflowed.");
        }
        if (TexelCount > std::numeric_limits<std::size_t>::max() / SurfaceNeighborCount ||
            TransferWeights.size() != TexelCount * SurfaceNeighborCount)
        {
            throw std::invalid_argument("TransferWeight cache size must be texel count × neighbor count.");
        }

        const std::vector<float> Areas =
            WorldTexelAreas.empty() ? std::vector<float>(TexelCount, SurfaceStateReferenceArea) : WorldTexelAreas;
        if (Areas.size() != TexelCount ||
            std::any_of(Areas.begin(), Areas.end(), [](float A) { return !std::isfinite(A) || A < 0.0F; }))
            throw std::invalid_argument("World texel areas must be finite, nonnegative and match texel count.");

        ScalarCount = TexelCount * ChannelCount;
        const std::size_t MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        ReactionCapacity = MaxReactionCount;
        const std::size_t ReactionStride = ChannelCount * 5U + MaxReactionCount + 4U;
        if (MaxReactionCount && TexelCount > std::numeric_limits<std::size_t>::max() / ReactionStride)
            throw std::overflow_error("Reaction scratch size overflow.");
        ReactionScratchBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                         Device,
                                                         MaxReactionCount ? TexelCount * ReactionStride : 1U,
                                                         MaxRange,
                                                         StorageUsage,
                                                         GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                         Pool);

        WorldTexelAreaBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                    Device,
                                                    Areas.data(),
                                                    Areas.size(),
                                                    sizeof(float),
                                                    MaxRange,
                                                    GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                    Pool);

        std::vector<TSurfaceGPUVec4> ZeroDynamicGeometry(TexelCount * 2U);
        DynamicGeometryBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                     Device,
                                                     ZeroDynamicGeometry.data(),
                                                     ZeroDynamicGeometry.size(),
                                                     sizeof(TSurfaceGPUVec4),
                                                     MaxRange,
                                                     GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                     Pool);
        DynamicConcavityWeightBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                                Device,
                                                                TexelCount,
                                                                MaxRange,
                                                                StorageUsage,
                                                                GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                                Pool);

        const TSurfaceSparseMetadataLayout SparseLayout = GetSurfaceSparseMetadataLayout(TexelCount);
        // 3 texel planes + persistent solver A/B sets + CPU-input activation list + accumulation/geometry schedules.
        // The two solver sets ping-pong with State A/B so Pass2 can directly build the next step's active set.
        const std::size_t AccumulationWordCount = SparseLayout.TotalWordCount;
        AccumulationHeightBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                            Device,
                                                            AccumulationWordCount,
                                                            MaxRange,
                                                            StorageUsage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                            GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                            Pool);

        StateABuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                Device,
                                                ScalarCount,
                                                MaxRange,
                                                StorageUsage,
                                                GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                Pool);
        StateBBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                Device,
                                                ScalarCount,
                                                MaxRange,
                                                StorageUsage,
                                                GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                Pool);
        OutgoingFluxScaleBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                           Device,
                                                           ScalarCount,
                                                           MaxRange,
                                                           StorageUsage,
                                                           GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                           Pool);
        InputDeltaBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                    Device,
                                                    ScalarCount,
                                                    MaxRange,
                                                    StorageUsage,
                                                    GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                    Pool);
        if (Storage.Compact && Storage.HalfWeights)
        {
            std::vector<std::uint32_t> Packed(TransferWeights.size() / 2U);
            for (std::size_t Pair = 0; Pair < Packed.size(); ++Pair)
                Packed[Pair] = glm::packHalf2x16(glm::vec2(std::clamp(TransferWeights[Pair * 2U], 0.0F, 1.0F),
                                                           std::clamp(TransferWeights[Pair * 2U + 1U], 0.0F, 1.0F)));
            TransferWeightBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                        Device,
                                                        Packed.data(),
                                                        Packed.size(),
                                                        sizeof(std::uint32_t),
                                                        MaxRange,
                                                        GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                        Pool);
            bStoredHalfWeights = true;
        }
        else
            TransferWeightBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                        Device,
                                                        TransferWeights.data(),
                                                        TransferWeights.size(),
                                                        sizeof(float),
                                                        MaxRange,
                                                        GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                        Pool);

        std::vector<TSurfaceGPUVec4>        ZeroDebugAverages;
        const std::vector<TSurfaceGPUVec4>* DebugAverages = &TransferWeightDebugAverages;
        if (DebugAverages->empty())
        {
            ZeroDebugAverages.resize(TexelCount);
            DebugAverages = &ZeroDebugAverages;
        }
        if (DebugAverages->size() != TexelCount)
        {
            throw std::invalid_argument("TransferWeight debug averages must match the texel count.");
        }

        TransferWeightDebugAverageBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                                Device,
                                                                DebugAverages->data(),
                                                                DebugAverages->size(),
                                                                sizeof(TSurfaceGPUVec4),
                                                                MaxRange,
                                                                GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                                Pool);

        // 첫 ScalarCount는 기존 RawOutgoing 합, 뒤 8*ScalarCount는 선택적 방향별 RawEdgeFlux cache다.
        // 같은 descriptor를 확장해 storage-buffer binding 수를 늘리지 않는다.
        if (ScalarCount > std::numeric_limits<std::size_t>::max() / (SurfaceNeighborCount + 1U))
            throw std::overflow_error("Surface raw-flux cache size overflowed.");

        const auto EdgeWords = !Storage.Compact        ? SurfaceNeighborCount
                               : !Storage.CacheRawFlux ? 0U
                               : Storage.HalfRawFlux   ? SurfaceNeighborCount / 2U
                                                       : SurfaceNeighborCount;
        RawOutgoingBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                     Device,
                                                     ScalarCount * (EdgeWords + 1U),
                                                     MaxRange,
                                                     StorageUsage,
                                                     GPU::TGPUBufferMemoryCategory::SurfaceState,
                                                     Pool);
        if (Storage.HalfWeights && !bStoredHalfWeights)
            UpdateTransferWeights(TransferWeights, {}, true);
    }

    bool TSurfaceInstanceGPUResources::ConfigureScratchStorage(
        VkPhysicalDevice PhysicalDevice, VkDevice Device, bool Cache, bool HalfRaw, bool HalfWeights, bool Compact)
    {
        const auto MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const auto EdgeWords = !Compact  ? SurfaceNeighborCount
                               : !Cache  ? 0U
                               : HalfRaw ? SurfaceNeighborCount / 2U
                                         : SurfaceNeighborCount;
        const auto RawWords = ScalarCount * (1U + EdgeWords);
        const auto WeightWords =
            TexelCount * (Compact && HalfWeights ? SurfaceNeighborCount / 2U : SurfaceNeighborCount);
        bool Changed = false;
        if (RawOutgoingBuffer->GetSize() != RawWords * sizeof(std::uint32_t))
        {
            auto Replacement = CreateZeroedScalarBuffer(PhysicalDevice, Device, RawWords, MaxRange);
            RawOutgoingBuffer = std::move(Replacement);
            Changed = true;
        }
        if (TransferWeightBuffer->GetSize() != WeightWords * sizeof(std::uint32_t))
        {
            std::vector<std::uint32_t> Old(TransferWeightBuffer->GetSize() / sizeof(std::uint32_t));
            TransferWeightBuffer->Download(Old.data(), TransferWeightBuffer->GetSize());
            std::vector<float> Weights(TexelCount * SurfaceNeighborCount);
            if (bStoredHalfWeights)
            {
                for (std::size_t Pair = 0; Pair < Weights.size() / 2U; ++Pair)
                {
                    const auto Value = glm::unpackHalf2x16(Old[Pair]);
                    Weights[Pair * 2U] = Value.x;
                    Weights[Pair * 2U + 1U] = Value.y;
                }
            }
            else
                std::memcpy(Weights.data(), Old.data(), Weights.size() * sizeof(float));
            auto Replacement = CreateZeroedScalarBuffer(PhysicalDevice, Device, WeightWords, MaxRange);
            TransferWeightBuffer = std::move(Replacement);
            UpdateTransferWeights(Weights, {}, HalfWeights);
            Changed = true;
        }
        return Changed;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetStateABuffer() const noexcept
    {
        return *StateABuffer;
    }

    void TSurfaceInstanceGPUResources::RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept
    {
        Pool.Release(std::move(ReactionScratchBuffer));
        Pool.Release(std::move(StateABuffer));
        Pool.Release(std::move(StateBBuffer));
        Pool.Release(std::move(OutgoingFluxScaleBuffer));
        Pool.Release(std::move(InputDeltaBuffer));
        Pool.Release(std::move(TransferWeightBuffer));
        Pool.Release(std::move(TransferWeightDebugAverageBuffer));
        Pool.Release(std::move(RawOutgoingBuffer));
        Pool.Release(std::move(WorldTexelAreaBuffer));
        Pool.Release(std::move(DynamicGeometryBuffer));
        Pool.Release(std::move(DynamicConcavityWeightBuffer));
        Pool.Release(std::move(AccumulationHeightBuffer));
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetStateBBuffer() const noexcept
    {
        return *StateBBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetOutgoingFluxScaleBuffer() const noexcept
    {
        return *OutgoingFluxScaleBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetInputDeltaBuffer() const noexcept
    {
        return *InputDeltaBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetTransferWeightBuffer() const noexcept
    {
        return *TransferWeightBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetTransferWeightDebugAverageBuffer() const noexcept
    {
        return *TransferWeightDebugAverageBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetRawOutgoingBuffer() const noexcept
    {
        return *RawOutgoingBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetWorldTexelAreaBuffer() const noexcept
    {
        return *WorldTexelAreaBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetDynamicGeometryBuffer() const noexcept
    {
        return *DynamicGeometryBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetDynamicConcavityWeightBuffer() const noexcept
    {
        return *DynamicConcavityWeightBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetAccumulationHeightBuffer() const noexcept
    {
        return *AccumulationHeightBuffer;
    }

    void TSurfaceInstanceGPUResources::UpdateWorldTexelAreas(const std::vector<float>& Areas)
    {
        if (Areas.size() != TexelCount ||
            std::any_of(Areas.begin(), Areas.end(), [](float A) { return !std::isfinite(A) || A < 0.0F; }))
            throw std::invalid_argument("Invalid updated world texel areas.");
        WorldTexelAreaBuffer->Upload(Areas.data(), WorldTexelAreaBuffer->GetSize());
    }

    void
    TSurfaceInstanceGPUResources::UpdateTransferWeights(const std::vector<float>&           TransferWeights,
                                                        const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages,
                                                        bool                                bHalfPrecision)
    {
        if (TransferWeights.size() != TexelCount * SurfaceNeighborCount)
        {
            throw std::invalid_argument("Updated TransferWeight cache has an incompatible size.");
        }
        bStoredHalfWeights = bHalfPrecision;
        if (bHalfPrecision)
        {
            // FP16 mode uses four uint words per texel; compact storage allocates exactly that range.
            std::vector<std::uint32_t> Packed(TexelCount * (SurfaceNeighborCount / 2U), 0U);
            for (std::size_t Texel = 0; Texel < TexelCount; ++Texel)
            {
                for (std::size_t Pair = 0; Pair < SurfaceNeighborCount / 2U; ++Pair)
                {
                    const std::size_t D0 = Pair * 2U;
                    const std::size_t D1 = D0 + 1U;
                    Packed[Texel * (SurfaceNeighborCount / 2U) + Pair] = glm::packHalf2x16(
                        glm::vec2(std::clamp(TransferWeights[Texel * SurfaceNeighborCount + D0], 0.0F, 1.0F),
                                  std::clamp(TransferWeights[Texel * SurfaceNeighborCount + D1], 0.0F, 1.0F)));
                }
            }
            TransferWeightBuffer->Upload(Packed.data(),
                                         static_cast<VkDeviceSize>(Packed.size() * sizeof(std::uint32_t)));
        }
        else
        {
            // Buffer is uint-addressed in GLSL, but IEEE-754 float bytes are intentionally uploaded verbatim.
            TransferWeightBuffer->Upload(TransferWeights.data(), TransferWeights.size() * sizeof(float));
        }
        if (!TransferWeightDebugAverages.empty())
        {
            if (TransferWeightDebugAverages.size() != TexelCount)
            {
                throw std::invalid_argument("Updated TransferWeight debug averages have an incompatible size.");
            }
            TransferWeightDebugAverageBuffer->Upload(TransferWeightDebugAverages.data(),
                                                     TransferWeightDebugAverageBuffer->GetSize());
        }
    }

    std::size_t TSurfaceInstanceGPUResources::GetTexelCount() const noexcept
    {
        return TexelCount;
    }

    std::size_t TSurfaceInstanceGPUResources::GetChannelCount() const noexcept
    {
        return ChannelCount;
    }

    void TSurfaceInstanceGPUResources::ResetState(const std::vector<float>& ResetOutgoingFluxScale)
    {
        if (ResetOutgoingFluxScale.size() != ScalarCount)
        {
            throw std::invalid_argument("Reset OutgoingFluxScale must match the instance scalar count.");
        }
        const std::vector<float> Zeros(ScalarCount, 0.0F);
        const VkDeviceSize       ByteSize = static_cast<VkDeviceSize>(ScalarCount * sizeof(float));
        StateABuffer->Upload(Zeros.data(), ByteSize);
        StateBBuffer->Upload(Zeros.data(), ByteSize);
        InputDeltaBuffer->Upload(Zeros.data(), ByteSize);
        RawOutgoingBuffer->Upload(Zeros.data(), ByteSize);
        OutgoingFluxScaleBuffer->Upload(ResetOutgoingFluxScale.data(), ByteSize);

        // Rendering now consumes the solver-produced accumulation height directly.  Reset it together
        // with State so a paused/reset frame cannot display stale derived height before the next step.
        const std::size_t AccumulationWordCount =
            static_cast<std::size_t>(AccumulationHeightBuffer->GetSize() / sizeof(std::uint32_t));
        const std::vector<std::uint32_t> ZeroAccumulation(AccumulationWordCount, 0U);
        AccumulationHeightBuffer->Upload(ZeroAccumulation.data(), AccumulationHeightBuffer->GetSize());
    }

#pragma endregion
}
