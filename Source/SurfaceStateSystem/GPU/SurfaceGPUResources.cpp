/**
 * @file SurfaceGPUResources.cpp
 * @brief 패킹된 Surface 데이터를 업로드하고 솔버 디스크립터 세트를 생성한다.
 */

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS
{
    namespace
    {
        constexpr VkBufferUsageFlags StorageUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        constexpr VkMemoryPropertyFlags UploadMemory =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        constexpr std::uint32_t DescriptorBindingCount =
            static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count);

        constexpr std::uint32_t BindingIndex(TSurfaceGPUDescriptorBinding Binding)
        {
            return static_cast<std::uint32_t>(Binding);
        }

        std::size_t GetMaximumStorageBufferRange(VkPhysicalDevice PhysicalDevice)
        {
            VkPhysicalDeviceProperties Properties{};
            vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
            return Properties.limits.maxStorageBufferRange;
        }

        std::unique_ptr<TGPUBuffer> CreateUploadedBuffer(VkPhysicalDevice PhysicalDevice,
                                                         VkDevice         Device,
                                                         const void*      Data,
                                                         std::size_t      ElementCount,
                                                         std::size_t      ElementStride,
                                                         std::size_t      MaxStorageBufferRange)
        {
            const std::size_t ByteSize =
                GetSurfaceGPUBufferByteSize(ElementCount, ElementStride, MaxStorageBufferRange);
            auto Buffer = std::make_unique<TGPUBuffer>(
                PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), StorageUsage, UploadMemory);
            Buffer->Upload(Data, static_cast<VkDeviceSize>(ByteSize));
            return Buffer;
        }

        std::unique_ptr<TGPUBuffer> CreateZeroedScalarBuffer(VkPhysicalDevice PhysicalDevice,
                                                             VkDevice         Device,
                                                             std::size_t      ScalarCount,
                                                             std::size_t      MaxStorageBufferRange)
        {
            const std::size_t ByteSize = GetSurfaceGPUBufferByteSize(ScalarCount, sizeof(float), MaxStorageBufferRange);
            std::vector<float> Zeros(ScalarCount, 0.0F);
            auto               Buffer = std::make_unique<TGPUBuffer>(
                PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), StorageUsage, UploadMemory);
            Buffer->Upload(Zeros.data(), static_cast<VkDeviceSize>(ByteSize));
            return Buffer;
        }

    } // 내부 네임스페이스

    TSurfaceSharedGeometryGPUResources::TSurfaceSharedGeometryGPUResources(VkPhysicalDevice PhysicalDevice,
        VkDevice         Device,
        const TSharedSurfaceGeometryData& Geometry,
        std::span<const TSurfaceProfileIndex> ProfileIndexRemap)
        : TexelCount(Geometry.GetTexelCount())
    {
        const std::size_t MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const TSurfaceGPUSharedGeometryUpload Upload = PackSharedSurfaceGeometry(Geometry, ProfileIndexRemap);
        TexelSurfaceIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                       Device,
                                                       Upload.TexelSurfaceIndices.data(),
                                                       Upload.TexelSurfaceIndices.size(),
                                                       sizeof(std::uint32_t),
                                                       MaxRange);
        TexelProfileIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                       Device,
                                                       Upload.TexelProfileIndices.data(),
                                                       Upload.TexelProfileIndices.size(),
                                                       sizeof(std::uint32_t),
                                                       MaxRange);
        PositionBuffer = CreateUploadedBuffer(PhysicalDevice,
                                              Device,
                                              Upload.Positions.data(),
                                              Upload.Positions.size(),
                                              sizeof(TSurfaceGPUVec4),
                                              MaxRange);
        NormalBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, Upload.Normals.data(), Upload.Normals.size(), sizeof(TSurfaceGPUVec4), MaxRange);
        // 렌더링과 디버그 뷰가 복원 노멀을 GPU에서 텍셀별로 조회한다.
        MesoNormalBuffer = CreateUploadedBuffer(PhysicalDevice,
                                            Device,
                                                Upload.MesoNormals.data(),
                                                Upload.MesoNormals.size(),
                                            sizeof(TSurfaceGPUVec4),
                                            MaxRange);
        GeometryScalarBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                    Device,
                                                    Upload.GeometryScalars.data(),
                                                    Upload.GeometryScalars.size(),
                                                    sizeof(TSurfaceGPUGeometryScalar),
                                                    MaxRange);
        NeighborIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                   Device,
                                                   Upload.NeighborIndices.data(),
                                                   Upload.NeighborIndices.size(),
                                                   sizeof(TSurfaceGPUNeighborIndices),
                                                   MaxRange);
        ReverseNeighborSlotBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                         Device,
                                                         Upload.ReverseNeighborSlots.data(),
                                                         Upload.ReverseNeighborSlots.size(),
                                                         sizeof(std::uint32_t),
                                                         MaxRange);
        SurfaceRangeBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                  Device,
                                                  Upload.SurfaceRanges.data(),
                                                  Upload.SurfaceRanges.size(),
                                                  sizeof(TSurfaceGPUSurfaceRange),
                                                  MaxRange);
        TexelChartIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                     Device,
                                                     Upload.TexelChartIndices.data(),
                                                     Upload.TexelChartIndices.size(),
                                                     sizeof(std::uint32_t),
                                                     MaxRange);
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelSurfaceIndexBuffer() const noexcept
    {
        return *TexelSurfaceIndexBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelProfileIndexBuffer() const noexcept
    {
        return *TexelProfileIndexBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetPositionBuffer() const noexcept
    {
        return *PositionBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNormalBuffer() const noexcept
    {
        return *NormalBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetMesoNormalBuffer() const noexcept
    {
        return *MesoNormalBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetGeometryScalarBuffer() const noexcept
    {
        return *GeometryScalarBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNeighborIndexBuffer() const noexcept
    {
        return *NeighborIndexBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetReverseNeighborSlotBuffer() const noexcept
    {
        return *ReverseNeighborSlotBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetSurfaceRangeBuffer() const noexcept
    {
        return *SurfaceRangeBuffer;
    }

    const TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelChartIndexBuffer() const noexcept
    {
        return *TexelChartIndexBuffer;
    }

    std::size_t TSurfaceSharedGeometryGPUResources::GetTexelCount() const noexcept
    {
        return TexelCount;
    }

    TSurfaceProfileGPUResources::TSurfaceProfileGPUResources(VkPhysicalDevice PhysicalDevice,
        VkDevice         Device,
        const std::vector<TSurfaceResponseProfileData>& Profiles,
        const TSurfaceStateRegistry& Registry)
        : ProfileCount(Profiles.size()), ChannelCount(Registry.GetStateCount())
    {
        const std::size_t MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const TSurfaceGPUProfileUpload Upload = PackSurfaceProfiles(Profiles, Registry);
        SupportedChannels = Upload.Supported;
        ParametersBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                Device,
                                                Upload.Parameters.data(),
                                                Upload.Parameters.size(),
                                                sizeof(TSurfaceGPUProfileParameters),
                                                MaxRange);
        SupportedBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, Upload.Supported.data(), Upload.Supported.size(), sizeof(std::uint32_t), MaxRange);
    }

    const TGPUBuffer& TSurfaceProfileGPUResources::GetParametersBuffer() const noexcept
    {
        return *ParametersBuffer;
    }

    const TGPUBuffer& TSurfaceProfileGPUResources::GetSupportedBuffer() const noexcept
    {
        return *SupportedBuffer;
    }

    std::size_t TSurfaceProfileGPUResources::GetProfileCount() const noexcept
    {
        return ProfileCount;
    }

    std::size_t TSurfaceProfileGPUResources::GetChannelCount() const noexcept
    {
        return ChannelCount;
    }

    bool TSurfaceProfileGPUResources::IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept
    {
        return ProfileIndex < ProfileCount && ChannelIndex < ChannelCount &&
               SupportedChannels[ProfileIndex * ChannelCount + ChannelIndex] != 0U;
    }

    void TSurfaceProfileGPUResources::UpdateParameters(std::size_t ProfileIndex,
                                                       std::size_t ChannelIndex,
                                                       const TSurfaceStateParameters& Parameters)
    {
        if (ProfileIndex >= ProfileCount || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface Profile parameter index is outside the GPU table.");
        }

        const std::size_t RecordIndex = GetSurfaceGPUProfileRecordIndex(ProfileIndex, ChannelIndex, ChannelCount);
        if (RecordIndex >= SupportedChannels.size() || SupportedChannels[RecordIndex] == 0U)
        {
            throw std::invalid_argument("Cannot override a State that this Profile does not support.");
        }

        const TSurfaceGPUProfileParameters Packed{{Parameters.StateCapacity,
             Parameters.InputFactor,
             Parameters.SaturationTransferFactor,
             Parameters.GeometryTransferFactor},
            {Parameters.DecayRate,
             Parameters.CavityRetentionFactor,
             Parameters.AccumulationFactor,
             Parameters.CavityFillFactor}};
        const VkDeviceSize Offset = static_cast<VkDeviceSize>(RecordIndex * sizeof(Packed));
        ParametersBuffer->Upload(&Packed, sizeof(Packed), Offset);
    }

    TSurfaceInstanceGPUResources::TSurfaceInstanceGPUResources(
        VkPhysicalDevice                    PhysicalDevice,
                                                               VkDevice         Device,
                                                               std::size_t      TexelCount,
                                                               std::size_t      ChannelCount,
                                                               const std::vector<float>& TransferWeights,
                                                               const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages)
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
        ScalarCount = TexelCount * ChannelCount;
        const std::size_t MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        if (ScalarCount > std::numeric_limits<std::uint32_t>::max() / SurfaceNeighborCount)
        {
            throw std::length_error("Surface RawFlux indices exceed the shader uint32 range.");
        }
        const std::size_t RawFluxByteSize =
            GetSurfaceGPUBufferByteSize(ScalarCount, SurfaceNeighborCount * sizeof(float), MaxRange);
        StateABuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        StateBBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        OutgoingFluxScaleBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        InputDeltaBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        TransferWeightBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, TransferWeights.data(), TransferWeights.size(), sizeof(float), MaxRange);
        std::vector<TSurfaceGPUVec4> ZeroDebugAverages;
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
        TransferWeightDebugAverageBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, DebugAverages->data(), DebugAverages->size(), sizeof(TSurfaceGPUVec4), MaxRange);
        RawOutgoingBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        // Pass 1 overwrites all slots of active sources; alpha=0 guards untouched inactive scratch.
        RawFluxBuffer = std::make_unique<TGPUBuffer>(
            PhysicalDevice, Device, static_cast<VkDeviceSize>(RawFluxByteSize), StorageUsage, UploadMemory);
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetStateABuffer() const noexcept
    {
        return *StateABuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetStateBBuffer() const noexcept
    {
        return *StateBBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetOutgoingFluxScaleBuffer() const noexcept
    {
        return *OutgoingFluxScaleBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetInputDeltaBuffer() const noexcept
    {
        return *InputDeltaBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetTransferWeightBuffer() const noexcept
    {
        return *TransferWeightBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetTransferWeightDebugAverageBuffer() const noexcept
    {
        return *TransferWeightDebugAverageBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetRawOutgoingBuffer() const noexcept
    {
        return *RawOutgoingBuffer;
    }

    const TGPUBuffer& TSurfaceInstanceGPUResources::GetRawFluxBuffer() const noexcept
    {
        return *RawFluxBuffer;
    }

    void
    TSurfaceInstanceGPUResources::UpdateTransferWeights(const std::vector<float>&           TransferWeights,
        const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages)
    {
        if (TransferWeights.size() != TexelCount * SurfaceNeighborCount)
        {
            throw std::invalid_argument("Updated TransferWeight cache has an incompatible size.");
        }
        TransferWeightBuffer->Upload(TransferWeights.data(), TransferWeightBuffer->GetSize());
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
        const VkDeviceSize ByteSize = static_cast<VkDeviceSize>(ScalarCount * sizeof(float));
        StateABuffer->Upload(Zeros.data(), ByteSize);
        StateBBuffer->Upload(Zeros.data(), ByteSize);
        InputDeltaBuffer->Upload(Zeros.data(), ByteSize);
        RawOutgoingBuffer->Upload(Zeros.data(), ByteSize);
        // RawFlux needs no reset: Pass 1 refreshes active sources and alpha=0 guards inactive ones.
        OutgoingFluxScaleBuffer->Upload(ResetOutgoingFluxScale.data(), ByteSize);
    }

    TSurfaceStateDescriptorResources::TSurfaceStateDescriptorResources(
        VkDevice                                 Device,
        const TSurfaceSharedGeometryGPUResources& SharedGeometry,
        const TSurfaceProfileGPUResources&        Profiles,
        const TSurfaceInstanceGPUResources&       Instance)
        : Device(Device)
    {
        if (SharedGeometry.GetTexelCount() != Instance.GetTexelCount() ||
            Profiles.GetChannelCount() != Instance.GetChannelCount())
        {
            throw std::invalid_argument("Surface descriptor resources have incompatible texel or channel counts.");
        }

        std::array<VkDescriptorSetLayoutBinding, DescriptorBindingCount> Bindings{};
        for (std::uint32_t Binding = 0; Binding < DescriptorBindingCount; ++Binding)
        {
            Bindings[Binding].binding = Binding;
            Bindings[Binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Bindings[Binding].descriptorCount = 1;
            Bindings[Binding].stageFlags =
                VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        }

        VkDescriptorSetLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
        LayoutInfo.pBindings = Bindings.data();
        if (vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &Layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Surface solver descriptor set layout.");
        }

        VkDescriptorPoolSize PoolSize{};
        PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        PoolSize.descriptorCount = DescriptorBindingCount * static_cast<std::uint32_t>(Sets.size());

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.maxSets = static_cast<std::uint32_t>(Sets.size());
        PoolInfo.poolSizeCount = 1;
        PoolInfo.pPoolSizes = &PoolSize;
        if (vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool) != VK_SUCCESS)
        {
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to create Surface solver descriptor pool.");
        }

        const std::array<VkDescriptorSetLayout, 2> Layouts{Layout, Layout};
        VkDescriptorSetAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocateInfo.descriptorPool = Pool;
        AllocateInfo.descriptorSetCount = static_cast<std::uint32_t>(Layouts.size());
        AllocateInfo.pSetLayouts = Layouts.data();
        if (vkAllocateDescriptorSets(Device, &AllocateInfo, Sets.data()) != VK_SUCCESS)
        {
            vkDestroyDescriptorPool(Device, Pool, nullptr);
            Pool = VK_NULL_HANDLE;
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate Surface solver descriptor sets.");
        }

        const std::array<const TGPUBuffer*, DescriptorBindingCount> SharedAndProfileBuffers{
            &SharedGeometry.GetTexelSurfaceIndexBuffer(),
            &SharedGeometry.GetTexelProfileIndexBuffer(),
            &SharedGeometry.GetPositionBuffer(),
            &SharedGeometry.GetNormalBuffer(),
            &SharedGeometry.GetGeometryScalarBuffer(),
            &SharedGeometry.GetNeighborIndexBuffer(),
            &Profiles.GetParametersBuffer(),
            &Profiles.GetSupportedBuffer(),
            nullptr,
            nullptr,
            &Instance.GetOutgoingFluxScaleBuffer(),
            &Instance.GetInputDeltaBuffer(),
            &SharedGeometry.GetSurfaceRangeBuffer(),
            &SharedGeometry.GetTexelChartIndexBuffer(),
            &Instance.GetTransferWeightBuffer(),
            &Instance.GetRawOutgoingBuffer(),
            &Instance.GetTransferWeightDebugAverageBuffer(),
            // 바인딩 17에는 공유 Meso 노멀 버퍼를 연결한다.
            &SharedGeometry.GetMesoNormalBuffer(),
            &SharedGeometry.GetReverseNeighborSlotBuffer(),
            &Instance.GetRawFluxBuffer()};

        for (std::size_t SetIndex = 0; SetIndex < Sets.size(); ++SetIndex)
        {
            const bool bAB = SetIndex == 0;
            const TGPUBuffer* StateCurrent = bAB ? &Instance.GetStateABuffer() : &Instance.GetStateBBuffer();
            const TGPUBuffer* StateNext = bAB ? &Instance.GetStateBBuffer() : &Instance.GetStateABuffer();
            std::array<VkDescriptorBufferInfo, DescriptorBindingCount> BufferInfos{};
            std::array<VkWriteDescriptorSet, DescriptorBindingCount> Writes{};
            for (std::uint32_t BindingNumber = 0; BindingNumber < DescriptorBindingCount; ++BindingNumber)
            {
                const TSurfaceGPUDescriptorBinding Binding = static_cast<TSurfaceGPUDescriptorBinding>(BindingNumber);
                const TGPUBuffer* Buffer = SharedAndProfileBuffers[BindingNumber];
                if (Binding == TSurfaceGPUDescriptorBinding::CurrentState)
                {
                    Buffer = StateCurrent;
                }
                else if (Binding == TSurfaceGPUDescriptorBinding::NextState)
                {
                    Buffer = StateNext;
                }
                BufferInfos[BindingNumber] = {Buffer->GetHandle(), 0, Buffer->GetSize()};
                BoundBufferHandles[SetIndex][BindingNumber] = Buffer->GetHandle();

                VkWriteDescriptorSet& Write = Writes[BindingNumber];
                Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Write.dstSet = Sets[SetIndex];
                Write.dstBinding = BindingIndex(Binding);
                Write.descriptorCount = 1;
                Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Write.pBufferInfo = &BufferInfos[BindingNumber];
            }
            vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
        }
    }

    TSurfaceStateDescriptorResources::~TSurfaceStateDescriptorResources()
    {
        if (Pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Device, Pool, nullptr);
            Pool = VK_NULL_HANDLE;
        }
        if (Layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            Layout = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSetLayout TSurfaceStateDescriptorResources::GetLayout() const noexcept
    {
        return Layout;
    }

    VkDescriptorSet TSurfaceStateDescriptorResources::GetABSet() const noexcept
    {
        return Sets[0];
    }

    VkDescriptorSet TSurfaceStateDescriptorResources::GetBASet() const noexcept
    {
        return Sets[1];
    }

    VkBuffer TSurfaceStateDescriptorResources::GetBoundBufferHandle(TSurfaceGPUDescriptorBinding Binding,
                                                                    bool bAB) const
    {
        const std::size_t BindingNumber = static_cast<std::size_t>(Binding);
        if (BindingNumber >= static_cast<std::size_t>(TSurfaceGPUDescriptorBinding::Count))
        {
            throw std::out_of_range("Surface descriptor binding is outside the declared layout.");
        }
        return BoundBufferHandles[bAB ? 0U : 1U][BindingNumber];
    }

} // MDSS 네임스페이스
