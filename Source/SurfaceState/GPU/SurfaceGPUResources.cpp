/**
 * @file SurfaceGPUResources.cpp
 * @brief 패킹된 Surface 데이터를 업로드하고 솔버 디스크립터 세트를 생성한다.
 */

#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    namespace
    {
        constexpr VkBufferUsageFlags    StorageUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
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

        std::unique_ptr<GPU::TGPUBuffer> CreateUploadedBuffer(VkPhysicalDevice PhysicalDevice,
                                                              VkDevice         Device,
                                                              const void*      Data,
                                                              std::size_t      ElementCount,
                                                              std::size_t      ElementStride,
                                                              std::size_t      MaxStorageBufferRange)
        {
            const std::size_t ByteSize =
                GetSurfaceGPUBufferByteSize(ElementCount, ElementStride, MaxStorageBufferRange);
            auto Buffer = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), StorageUsage, UploadMemory);
            Buffer->Upload(Data, static_cast<VkDeviceSize>(ByteSize));
            return Buffer;
        }

        std::unique_ptr<GPU::TGPUBuffer> CreateZeroedScalarBuffer(VkPhysicalDevice   PhysicalDevice,
                                                                  VkDevice           Device,
                                                                  std::size_t        ScalarCount,
                                                                  std::size_t        MaxStorageBufferRange,
                                                                  VkBufferUsageFlags Usage = StorageUsage)
        {
            const std::size_t ByteSize = GetSurfaceGPUBufferByteSize(ScalarCount, sizeof(float), MaxStorageBufferRange);
            std::vector<float> Zeros(ScalarCount, 0.0F);
            auto               Buffer = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice, Device, static_cast<VkDeviceSize>(ByteSize), Usage, UploadMemory);
            Buffer->Upload(Zeros.data(), static_cast<VkDeviceSize>(ByteSize));
            return Buffer;
        }

    } // 내부 네임스페이스

    TSurfaceSharedGeometryGPUResources::TSurfaceSharedGeometryGPUResources(
        VkPhysicalDevice                            PhysicalDevice,
        VkDevice                                    Device,
        const TSharedSurfaceGeometryData&           Geometry,
        std::span<const TSurfaceProfileIndex>       ProfileIndexRemap,
        std::span<const Asset::TVertex>             SourceVertices,
        std::span<const Asset::TMeshTriangleSource> SourceTriangles)
        : TexelCount(Geometry.GetTexelCount())
    {
        const std::size_t                     MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const TSurfaceGPUSharedGeometryUpload Upload = PackSharedSurfaceGeometry(Geometry, ProfileIndexRemap);
        SurfaceRanges = Upload.SurfaceRanges;
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
        ReverseNeighborDirectionIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                         Device,
                                                         Upload.ReverseNeighborDirectionIndices.data(),
                                                         Upload.ReverseNeighborDirectionIndices.size(),
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
        std::vector<std::uint32_t> BoundaryFlags(TexelCount, 0U);
        for (std::uint32_t Surface = 0; Surface < SurfaceRanges.size(); ++Surface)
        {
            const auto& Range = SurfaceRanges[Surface];
            for (std::uint32_t Y = 0; Y < Range.Height; ++Y)
                for (std::uint32_t X = 0; X < Range.Width; ++X)
                {
                    const std::size_t Center = Range.FirstTexel + std::size_t(Y) * Range.Width + X;
                    const auto Profile = Upload.TexelProfileIndices[Center];
                    const auto Chart = Upload.TexelChartIndices[Center];
                    if (Upload.TexelSurfaceIndices[Center] != Surface)
                    {
                        BoundaryFlags[Center] = 1U;
                        continue;
                    }
                    for (int OffsetY = -1; OffsetY <= 1; ++OffsetY)
                        for (int OffsetX = -1; OffsetX <= 1; ++OffsetX)
                        {
                            const auto NeighborX = static_cast<std::uint32_t>(
                                std::clamp<int>(static_cast<int>(X) + OffsetX, 0, static_cast<int>(Range.Width) - 1));
                            const auto NeighborY = static_cast<std::uint32_t>(
                                std::clamp<int>(static_cast<int>(Y) + OffsetY, 0, static_cast<int>(Range.Height) - 1));
                            const std::size_t Neighbor =
                                Range.FirstTexel + std::size_t(NeighborY) * Range.Width + NeighborX;
                            if (Upload.TexelSurfaceIndices[Neighbor] != Surface ||
                                Upload.TexelProfileIndices[Neighbor] != Profile ||
                                Upload.TexelChartIndices[Neighbor] != Chart)
                                BoundaryFlags[Center] = 1U;
                        }
                }
        }
        RenderSamplingBoundaryFlagBuffer = CreateUploadedBuffer(PhysicalDevice, Device,
                                                                 BoundaryFlags.data(), BoundaryFlags.size(),
                                                                 sizeof(std::uint32_t), MaxRange);
        SimulationResolution = Geometry.GetSurfaces().empty() ? 0U : Geometry.GetSurfaces().front().Resolution.Width;
        const auto UploadTexelMesh = [&](TSurfaceTexelMesh Mesh, TSurfaceTexelMeshGPUVariant& Output)
        {
            Output.Ranges = std::move(Mesh.Surfaces);
            Output.VertexCount = static_cast<std::uint32_t>(Mesh.Vertices.size());
            Output.BoundaryCount = static_cast<std::uint32_t>(Mesh.BoundaryEdges.size());
            if (Mesh.Indices.empty())
                return;
            const auto Bytes = static_cast<VkDeviceSize>(Mesh.Indices.size() * sizeof(std::uint32_t));
            Output.IndexBuffer =
                std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                  Device,
                                                  Bytes,
                                                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                  UploadMemory);
            Output.IndexBuffer->Upload(Mesh.Indices.data(), Bytes);
            const auto VertexBytes = static_cast<VkDeviceSize>(Mesh.Vertices.size() * sizeof(TSurfaceTexelMeshVertex));
            Output.VertexBuffer = std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                                    Device,
                                                                    VertexBytes,
                                                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                    UploadMemory);
            Output.VertexBuffer->Upload(Mesh.Vertices.data(), VertexBytes);
            const glm::uvec4 EmptyEdge{0};
            const auto*      Edges = Mesh.BoundaryEdges.empty() ? &EmptyEdge : Mesh.BoundaryEdges.data();
            const auto       BoundaryBytes =
                static_cast<VkDeviceSize>(std::max<std::size_t>(Mesh.BoundaryEdges.size(), 1) * sizeof(glm::uvec4));
            Output.BoundaryBuffer = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice, Device, BoundaryBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, UploadMemory);
            Output.BoundaryBuffer->Upload(Edges, BoundaryBytes);
        };
        UploadTexelMesh(BuildSurfaceTexelMesh(Geometry, SourceVertices, SourceTriangles), FullTexelMesh);
        for (const auto& Preset : SurfaceRenderMeshResolutionPresets)
        {
            if (Preset.Resolution == SimulationResolution)
                continue;
            TSurfaceTexelMeshGPUVariant Variant;
            UploadTexelMesh(BuildSurfaceTexelMesh(Geometry, SourceVertices, SourceTriangles, Preset.Resolution),
                            Variant);
            TexelMeshVariants.emplace(Preset.Resolution, std::move(Variant));
        }
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelSurfaceIndexBuffer() const noexcept
    {
        return *TexelSurfaceIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelProfileIndexBuffer() const noexcept
    {
        return *TexelProfileIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetPositionBuffer() const noexcept
    {
        return *PositionBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNormalBuffer() const noexcept
    {
        return *NormalBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetMesoNormalBuffer() const noexcept
    {
        return *MesoNormalBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetGeometryScalarBuffer() const noexcept
    {
        return *GeometryScalarBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNeighborIndexBuffer() const noexcept
    {
        return *NeighborIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetReverseNeighborDirectionIndexBuffer() const noexcept
    {
        return *ReverseNeighborDirectionIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetSurfaceRangeBuffer() const noexcept
    {
        return *SurfaceRangeBuffer;
    }

    const std::vector<TSurfaceGPUSurfaceRange>& TSurfaceSharedGeometryGPUResources::GetSurfaceRanges() const noexcept
    {
        return SurfaceRanges;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelChartIndexBuffer() const noexcept
    {
        return *TexelChartIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetRenderSamplingBoundaryFlagBuffer() const noexcept
    {
        return *RenderSamplingBoundaryFlagBuffer;
    }

    std::size_t TSurfaceSharedGeometryGPUResources::GetTexelCount() const noexcept
    {
        return TexelCount;
    }

    TSurfaceProfileGPUResources::TSurfaceProfileGPUResources(VkPhysicalDevice PhysicalDevice,
                                                             VkDevice         Device,
                                                             const std::vector<TSurfaceResponseProfileData>& Profiles,
                                                             const TSurfaceStateRegistry&                    Registry)
        : ProfileCount(Profiles.size()), ChannelCount(Registry.GetStateCount())
    {
        const std::size_t              MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
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

    const GPU::TGPUBuffer& TSurfaceProfileGPUResources::GetParametersBuffer() const noexcept
    {
        return *ParametersBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceProfileGPUResources::GetSupportedBuffer() const noexcept
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

    void TSurfaceProfileGPUResources::UpdateParameters(std::size_t                    ProfileIndex,
                                                       std::size_t                    ChannelIndex,
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

        const TSurfaceGPUProfileParameters Packed{
            {Parameters.StateCapacity,
             Parameters.InputFactor,
             Parameters.SaturationTransferFactor,
             Parameters.GeometryTransferFactor},
            {Parameters.DecayRate,
             Parameters.CavityRetentionFactor,
             Parameters.AccumulationFactor,
             Parameters.CavityFillFactor},
            {Parameters.ThicknessPerAmount, Parameters.CavityTransportRetentionFactor, 0.0F, 0.0F}};
        const VkDeviceSize Offset = static_cast<VkDeviceSize>(RecordIndex * sizeof(Packed));
        ParametersBuffer->Upload(&Packed, sizeof(Packed), Offset);
    }

    TSurfaceInstanceGPUResources::TSurfaceInstanceGPUResources(
        VkPhysicalDevice                    PhysicalDevice,
        VkDevice                            Device,
        std::size_t                         TexelCount,
        std::size_t                         ChannelCount,
        const std::vector<float>&           TransferWeights,
        const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages,
        const std::vector<float>&           WorldTexelAreas)
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
        WorldTexelAreaBuffer =
            CreateUploadedBuffer(PhysicalDevice, Device, Areas.data(), Areas.size(), sizeof(float), MaxRange);
        std::vector<TSurfaceGPUVec4> ZeroDynamicGeometry(TexelCount * 2U);
        DynamicGeometryBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                     Device,
                                                     ZeroDynamicGeometry.data(),
                                                     ZeroDynamicGeometry.size(),
                                                     sizeof(TSurfaceGPUVec4),
                                                     MaxRange);
        DynamicConcavityWeightBuffer =
            CreateZeroedScalarBuffer(PhysicalDevice, Device, TexelCount, MaxRange);
        const std::size_t WorkgroupCount = TexelCount / 64U + (TexelCount % 64U != 0U);
        if (WorkgroupCount > std::numeric_limits<std::size_t>::max() - 3U ||
            TexelCount > (std::numeric_limits<std::size_t>::max() - WorkgroupCount - 3U) / 3U)
            throw std::overflow_error("Surface accumulation height and dirty flag count overflowed.");
        // Height, two dirty planes, one flag per 64-texel workgroup, and a
        // three-word VkDispatchIndirectCommand share the existing descriptor.
        AccumulationHeightBuffer = CreateZeroedScalarBuffer(PhysicalDevice,
                                                            Device,
                                                            TexelCount * 3U + WorkgroupCount + 3U,
                                                            MaxRange,
                                                            StorageUsage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
        StateABuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        StateBBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        OutgoingFluxScaleBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        InputDeltaBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
        TransferWeightBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, TransferWeights.data(), TransferWeights.size(), sizeof(float), MaxRange);
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
        TransferWeightDebugAverageBuffer = CreateUploadedBuffer(
            PhysicalDevice, Device, DebugAverages->data(), DebugAverages->size(), sizeof(TSurfaceGPUVec4), MaxRange);
        RawOutgoingBuffer = CreateZeroedScalarBuffer(PhysicalDevice, Device, ScalarCount, MaxRange);
    }

    const GPU::TGPUBuffer& TSurfaceInstanceGPUResources::GetStateABuffer() const noexcept
    {
        return *StateABuffer;
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
        const VkDeviceSize       ByteSize = static_cast<VkDeviceSize>(ScalarCount * sizeof(float));
        StateABuffer->Upload(Zeros.data(), ByteSize);
        StateBBuffer->Upload(Zeros.data(), ByteSize);
        InputDeltaBuffer->Upload(Zeros.data(), ByteSize);
        RawOutgoingBuffer->Upload(Zeros.data(), ByteSize);
        OutgoingFluxScaleBuffer->Upload(ResetOutgoingFluxScale.data(), ByteSize);
    }

    TSurfaceStateDescriptorResources::TSurfaceStateDescriptorResources(
        VkDevice                                  Device,
        const TSurfaceSharedGeometryGPUResources& SharedGeometry,
        const TSurfaceProfileGPUResources&        Profiles,
        const TSurfaceInstanceGPUResources&       Instance)
        : Device(Device), SharedGeometryResources(&SharedGeometry)
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
                Binding == static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::TransferWeightDebugAverages)
                    ? VK_SHADER_STAGE_FRAGMENT_BIT
                    : VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
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
        VkDescriptorSetAllocateInfo                AllocateInfo{};
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

        const std::array<const GPU::TGPUBuffer*, DescriptorBindingCount> SharedAndProfileBuffers{
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
            &SharedGeometry.GetReverseNeighborDirectionIndexBuffer(),
            &Instance.GetWorldTexelAreaBuffer(),
            &Instance.GetDynamicGeometryBuffer(),
            &Instance.GetAccumulationHeightBuffer(),
            &Instance.GetDynamicConcavityWeightBuffer()};

        for (std::size_t SetIndex = 0; SetIndex < Sets.size(); ++SetIndex)
        {
            const bool             bAB = SetIndex == 0;
            const GPU::TGPUBuffer* StateCurrent = bAB ? &Instance.GetStateABuffer() : &Instance.GetStateBBuffer();
            const GPU::TGPUBuffer* StateNext = bAB ? &Instance.GetStateBBuffer() : &Instance.GetStateABuffer();
            std::array<VkDescriptorBufferInfo, DescriptorBindingCount> BufferInfos{};
            std::array<VkWriteDescriptorSet, DescriptorBindingCount>   Writes{};
            for (std::uint32_t BindingNumber = 0; BindingNumber < DescriptorBindingCount; ++BindingNumber)
            {
                const TSurfaceGPUDescriptorBinding Binding = static_cast<TSurfaceGPUDescriptorBinding>(BindingNumber);
                const GPU::TGPUBuffer*             Buffer = SharedAndProfileBuffers[BindingNumber];
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
                                                                    bool                         bAB) const
    {
        const std::size_t BindingNumber = static_cast<std::size_t>(Binding);
        if (BindingNumber >= static_cast<std::size_t>(TSurfaceGPUDescriptorBinding::Count))
        {
            throw std::out_of_range("Surface descriptor binding is outside the declared layout.");
        }
        return BoundBufferHandles[bAB ? 0U : 1U][BindingNumber];
    }

    std::array<std::uint64_t, 6> TSurfaceStateDescriptorResources::GetGeometryInputRevisions() const noexcept
    {
        return {SharedGeometryResources->GetTexelSurfaceIndexBuffer().GetUploadRevision(),
                SharedGeometryResources->GetPositionBuffer().GetUploadRevision(),
                SharedGeometryResources->GetNormalBuffer().GetUploadRevision(),
                SharedGeometryResources->GetGeometryScalarBuffer().GetUploadRevision(),
                SharedGeometryResources->GetNeighborIndexBuffer().GetUploadRevision(),
                SharedGeometryResources->GetMesoNormalBuffer().GetUploadRevision()};
    }

} // MDSS 네임스페이스
