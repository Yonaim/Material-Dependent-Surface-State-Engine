/**
 * @file AccumulationOverlaySides.cpp
 * @brief 누적 상태 overlay의 상단과 경계 옆면 geometry 생성을 구성한다.
 */
#include "Rendering/AccumulationOverlaySides.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS::Rendering
{
    namespace
    {
        constexpr std::uint32_t MaxOverlayLayersPerInstance = 3;
        constexpr std::uint32_t MaxOverlayMeshResolutions =
            static_cast<std::uint32_t>(SurfaceState::SurfaceRenderMeshResolutionPresets.size());

        struct alignas(16) TSideSegment
        {
            glm::vec4 BaseA, TopA, BaseB, TopB, Inside;
        };
        static_assert(sizeof(TSideSegment) == 80);
        static_assert(sizeof(VkDrawIndirectCommand) == 16);
        static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20);

        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS)
                throw std::runtime_error("Failed to create accumulation overlay side resources.");
        }
    }

    TAccumulationOverlaySides::TAccumulationOverlaySides(VkPhysicalDevice      PhysicalDevice,
                                                         VkDevice              Device,
                                                         VkDescriptorSetLayout SurfaceLayout,
                                                         VkDescriptorSetLayout ComputedLayout,
                                                         std::size_t           MaxInstances)
        : PhysicalDevice(PhysicalDevice), Device(Device)
    {
        constexpr std::uint32_t SetsPerInstance = MaxOverlayLayersPerInstance * MaxOverlayMeshResolutions;
        constexpr std::uint32_t StorageDescriptorsPerInstance = SetsPerInstance * 7U;
        if (!SurfaceLayout || !ComputedLayout || MaxInstances == 0 ||
            MaxInstances > std::numeric_limits<std::uint32_t>::max() / StorageDescriptorsPerInstance)
            throw std::invalid_argument("Accumulation overlay sides require valid layouts and instance slots.");
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        Limits = Properties.limits;
        const auto StorageBindings =
            static_cast<std::uint32_t>(SurfaceState::TSurfaceGPUDescriptorBinding::Count) - 1U + 9U;
        if (Limits.maxPerStageDescriptorStorageBuffers < StorageBindings ||
            Limits.maxDescriptorSetStorageBuffers < StorageBindings || Limits.maxComputeWorkGroupInvocations < 64 ||
            Limits.maxComputeWorkGroupSize[0] < 64)
            throw std::runtime_error("Vulkan device lacks accumulation overlay side limits.");

        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            std::array<VkDescriptorSetLayoutBinding, 7> Bindings{};
            for (std::uint32_t I = 0; I < Bindings.size(); ++I)
            {
                Bindings[I].binding = I;
                Bindings[I].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Bindings[I].descriptorCount = 1;
                Bindings[I].stageFlags =
                    VK_SHADER_STAGE_COMPUTE_BIT | ((I == 3 || I == 4) ? VK_SHADER_STAGE_VERTEX_BIT : 0);
            }
            VkDescriptorSetLayoutCreateInfo OutputInfo{};
            OutputInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            OutputInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
            OutputInfo.pBindings = Bindings.data();
            RequireVk(vkCreateDescriptorSetLayout(Device, &OutputInfo, nullptr, &OutputLayout));

            const std::array<VkDescriptorSetLayout, 3> Layouts{SurfaceLayout, ComputedLayout, OutputLayout};
            const VkPushConstantRange                  Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 24};
            VkPipelineLayoutCreateInfo                 LayoutInfo{};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            LayoutInfo.setLayoutCount = static_cast<std::uint32_t>(Layouts.size());
            LayoutInfo.pSetLayouts = Layouts.data();
            LayoutInfo.pushConstantRangeCount = 1;
            LayoutInfo.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout));

            const auto CreatePipeline =
                [&](const char* ShaderName, VkPipelineLayout SelectedLayout, VkPipeline& Destination)
            {
                std::ifstream File(std::string(MDSS_SHADER_DIR) + "/Rendering/StateOverlay/" + ShaderName + ".spv",
                                   std::ios::binary | std::ios::ate);
                if (!File)
                    throw std::runtime_error(std::string("Cannot open ") + ShaderName + " compute shader.");
                const auto Size = File.tellg();
                if (Size <= 0 || Size % 4 != 0)
                    throw std::runtime_error(std::string("Invalid ") + ShaderName + " SPIR-V.");
                std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
                File.seekg(0);
                File.read(reinterpret_cast<char*>(Code.data()), Size);
                if (!File)
                    throw std::runtime_error(std::string("Cannot read ") + ShaderName + " compute shader.");
                VkShaderModuleCreateInfo ModuleInfo{};
                ModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                ModuleInfo.codeSize = static_cast<std::size_t>(Size);
                ModuleInfo.pCode = Code.data();
                RequireVk(vkCreateShaderModule(Device, &ModuleInfo, nullptr, &Module));
                VkComputePipelineCreateInfo PipelineInfo{};
                PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                PipelineInfo.layout = SelectedLayout;
                PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                PipelineInfo.stage.module = Module;
                PipelineInfo.stage.pName = "main";
                const VkResult Result =
                    vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Destination);
                vkDestroyShaderModule(Device, Module, nullptr);
                Module = VK_NULL_HANDLE;
                RequireVk(Result);
            };
            CreatePipeline("OverlaySideCoverage.comp", PipelineLayout, CoveragePipeline);
            CreatePipeline("OverlayCoverageSmoothing.comp", PipelineLayout, CoverageSmoothingPipeline);
            CreatePipeline("OverlaySideBoundary.comp", PipelineLayout, BoundaryPipeline);

            // Keep one output set per instance, overlay layer, and selectable mesh resolution.
            const std::uint32_t SetCount = static_cast<std::uint32_t>(MaxInstances) * SetsPerInstance;
            const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SetCount * 7U};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = SetCount;
            PoolInfo.poolSizeCount = 1;
            PoolInfo.pPoolSizes = &PoolSize;
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));
        }
        catch (...)
        {
            if (Module)
                vkDestroyShaderModule(Device, Module, nullptr);
            if (Pool)
                vkDestroyDescriptorPool(Device, Pool, nullptr);
            if (CoveragePipeline)
                vkDestroyPipeline(Device, CoveragePipeline, nullptr);
            if (CoverageSmoothingPipeline)
                vkDestroyPipeline(Device, CoverageSmoothingPipeline, nullptr);
            if (BoundaryPipeline)
                vkDestroyPipeline(Device, BoundaryPipeline, nullptr);
            if (PipelineLayout)
                vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            if (OutputLayout)
                vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
            throw;
        }
    }

    TAccumulationOverlaySides::~TAccumulationOverlaySides()
    {
        Outputs.clear();
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        if (CoveragePipeline)
            vkDestroyPipeline(Device, CoveragePipeline, nullptr);
        if (CoverageSmoothingPipeline)
            vkDestroyPipeline(Device, CoverageSmoothingPipeline, nullptr);
        if (BoundaryPipeline)
            vkDestroyPipeline(Device, BoundaryPipeline, nullptr);
        if (PipelineLayout)
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        if (OutputLayout)
            vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
    }

    VkDescriptorSet TAccumulationOverlaySides::GetSet(std::size_t Instance,
                                                       std::uint32_t Channel,
                                                       std::uint32_t MeshResolution) const
    {
        return Outputs.at({Instance, Channel, MeshResolution}).Set;
    }
    std::uint32_t TAccumulationOverlaySides::GetBoundaryCount(std::size_t Instance,
                                                               std::uint32_t Channel,
                                                               std::uint32_t MeshResolution) const
    {
        return Outputs.at({Instance, Channel, MeshResolution}).BoundaryCount;
    }
    VkBuffer TAccumulationOverlaySides::GetDrawBuffer(std::size_t Instance,
                                                       std::uint32_t Channel,
                                                       std::uint32_t MeshResolution) const
    {
        return Outputs.at({Instance, Channel, MeshResolution}).DrawCommands->GetHandle();
    }

    VkBuffer TAccumulationOverlaySides::GetTopDrawBuffer(std::size_t Instance,
                                                          std::uint32_t Channel,
                                                          std::uint32_t MeshResolution) const
    {
        return Outputs.at({Instance, Channel, MeshResolution}).TopDrawCommands->GetHandle();
    }

    TAccumulationOverlaySides::TTriangleActivity TAccumulationOverlaySides::CompleteFrame(std::size_t FrameIndex)
    {
        if (FrameIndex >= TRenderContext::MaxFramesInFlight)
            throw std::out_of_range("Overlay activity frame slot is invalid.");
        TTriangleActivity Activity;
        for (auto& [Key, Output] : Outputs)
        {
            if (!Output.ActivityPending[FrameIndex])
                continue;
            std::uint32_t Active = 0;
            Output.ActivityReadbacks[FrameIndex]->Download(&Active, sizeof(Active));
            Activity.Active += Active;
            Activity.Total += Output.TriangleCount;
            Output.ActivityPending[FrameIndex] = false;
        }
        return Activity;
    }

    void TAccumulationOverlaySides::Record(VkCommandBuffer                                         Command,
                                           std::size_t                                             Instance,
                                           std::uint32_t                                           Channel,
                                           std::uint32_t                                           Channels,
                                           std::uint32_t                                           MeshResolution,
                                           const SurfaceState::TSurfaceSharedGeometryGPUResources& Geometry,
                                           const SurfaceState::TSurfaceStateDescriptorResources&   StateDescriptors,
                                           VkDescriptorSet                                         ComputedSet,
                                           bool                                                    bStateAB,
                                           std::size_t                                             FrameIndex,
                                           VkQueryPool                                             TimestampQueryPool,
                                           std::uint32_t FirstSideTimestampQuery,
                                           std::uint32_t OccupancyTileSize,
                                           bool          bSmoothCoverage)
    {
        if (FrameIndex >= TRenderContext::MaxFramesInFlight)
            throw std::out_of_range("Overlay activity frame slot is invalid.");
        const auto WriteStageTimestamp = [&](std::uint32_t Offset)
        {
            if (TimestampQueryPool != VK_NULL_HANDLE)
                vkCmdWriteTimestamp(Command,
                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    TimestampQueryPool,
                                    FirstSideTimestampQuery + Offset);
        };
        const auto WriteEmptyStageTimestamps = [&]()
        {
            WriteStageTimestamp(1U);
            WriteStageTimestamp(2U);
            WriteStageTimestamp(3U);
            WriteStageTimestamp(4U);
            WriteStageTimestamp(5U);
            WriteStageTimestamp(6U);
            WriteStageTimestamp(7U);
        };
        const auto& Mesh = Geometry.GetTexelMeshVariant(MeshResolution);
        if (!Mesh.IndexBuffer || !Mesh.VertexBuffer || !Mesh.BoundaryBuffer)
        {
            WriteEmptyStageTimestamps();
            return;
        }
        const auto Triangles =
            static_cast<std::uint32_t>(Mesh.IndexBuffer->GetSize() / (3U * sizeof(std::uint32_t)));
        const auto          Vertices = Mesh.VertexCount;
        const auto          Boundaries = Mesh.BoundaryCount;
        const std::uint32_t SegmentCount = Triangles + Boundaries;
        if (SegmentCount == 0 || Vertices == 0)
        {
            WriteEmptyStageTimestamps();
            return;
        }
        const auto Bytes =
            SurfaceState::GetSurfaceGPUBufferByteSize(SegmentCount, sizeof(TSideSegment), Limits.maxStorageBufferRange);
        const auto CoverageBytes =
            SurfaceState::GetSurfaceGPUBufferByteSize(Vertices, sizeof(float), Limits.maxStorageBufferRange);
        const auto SurfaceCount = static_cast<std::uint32_t>(Mesh.Ranges.size());
        const auto DrawBytes = SurfaceState::GetSurfaceGPUBufferByteSize(
            static_cast<std::size_t>(SurfaceCount) + 2U, sizeof(VkDrawIndirectCommand), Limits.maxStorageBufferRange);
        const auto TopDrawBytes = SurfaceState::GetSurfaceGPUBufferByteSize(
            SurfaceCount, sizeof(VkDrawIndexedIndirectCommand), Limits.maxStorageBufferRange);
        if (SegmentCount > std::numeric_limits<std::uint32_t>::max() / 6U)
            throw std::overflow_error("Overlay side vertex count exceeds the draw range.");
        const auto Key = std::make_tuple(Instance, Channel, MeshResolution);
        auto       It = Outputs.find(Key);
        if (It == Outputs.end())
        {
            const VkDeviceSize CoverageAlignment =
                std::max<VkDeviceSize>(Limits.minStorageBufferOffsetAlignment, alignof(float));
            const auto AlignCoverageOffset = [CoverageAlignment](VkDeviceSize Offset)
            {
                const VkDeviceSize Remainder = Offset % CoverageAlignment;
                return Remainder == 0 ? Offset : Offset + CoverageAlignment - Remainder;
            };
            constexpr VkDeviceSize CoveragePageSize = 2U * 1024U * 1024U;
            std::size_t            CoveragePage = CoveragePages.empty() ? 0U : CoveragePages.size() - 1U;
            VkDeviceSize           CoverageOffset =
                CoveragePages.empty() ? 0U : AlignCoverageOffset(CoveragePageUsed[CoveragePage]);
            if (CoveragePages.empty() || CoverageOffset > CoveragePages[CoveragePage]->GetSize() ||
                CoverageBytes > CoveragePages[CoveragePage]->GetSize() - CoverageOffset)
            {
                const VkDeviceSize PageCapacity = std::max(CoveragePageSize, AlignCoverageOffset(CoverageBytes));
                CoveragePages.push_back(std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                                          Device,
                                                                          PageCapacity,
                                                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
                CoveragePageUsed.push_back(0U);
                CoveragePage = CoveragePages.size() - 1U;
                CoverageOffset = 0U;
            }
            CoveragePageUsed[CoveragePage] = CoverageOffset + CoverageBytes;

            TOutput Output;
            Output.Segments = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice, Device, Bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Output.DrawCommands = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice,
                Device,
                DrawBytes,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Output.TopDrawCommands = std::make_unique<GPU::TGPUBuffer>(
                PhysicalDevice,
                Device,
                TopDrawBytes,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            // Indirect command topology is immutable for this overlay mesh. Upload those static
            // fields once; per-frame VertexCount/InstanceCount values are reset by the coverage
            // compute pass before the boundary pass runs.
            std::vector<VkDrawIndirectCommand> InitialDrawCommands(SurfaceCount + 2U);
            for (std::uint32_t Surface = 0; Surface < SurfaceCount; ++Surface)
            {
                const auto& Range = Mesh.Ranges[Surface];
                InitialDrawCommands[Surface] = {0U, 1U, Range.FirstIndex * 2U, 0U};
            }
            InitialDrawCommands[SurfaceCount] = {0U, 1U, Triangles * 6U, 0U};

            std::vector<VkDrawIndexedIndirectCommand> InitialTopDrawCommands(SurfaceCount);
            for (std::uint32_t Surface = 0; Surface < SurfaceCount; ++Surface)
            {
                const auto& Range = Mesh.Ranges[Surface];
                // OverlayTop.vert does not use gl_InstanceIndex; zero also supports devices
                // without indirect firstInstance.
                InitialTopDrawCommands[Surface] = {Range.IndexCount, 0U, Range.FirstIndex, 0, 0U};
            }

            constexpr VkDeviceSize MaxUpdateBytes = 65536U;
            for (VkDeviceSize Offset = 0; Offset < DrawBytes; Offset += MaxUpdateBytes)
                vkCmdUpdateBuffer(Command,
                                  Output.DrawCommands->GetHandle(),
                                  Offset,
                                  std::min(MaxUpdateBytes, DrawBytes - Offset),
                                  reinterpret_cast<const std::uint8_t*>(InitialDrawCommands.data()) + Offset);
            for (VkDeviceSize Offset = 0; Offset < TopDrawBytes; Offset += MaxUpdateBytes)
                vkCmdUpdateBuffer(Command,
                                  Output.TopDrawCommands->GetHandle(),
                                  Offset,
                                  std::min(MaxUpdateBytes, TopDrawBytes - Offset),
                                  reinterpret_cast<const std::uint8_t*>(InitialTopDrawCommands.data()) + Offset);
            for (auto& Readback : Output.ActivityReadbacks)
                Readback = std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                             Device,
                                                             sizeof(std::uint32_t),
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            Output.CoveragePage = CoveragePage;
            Output.CoverageOffset = CoverageOffset;
            VkDescriptorSetAllocateInfo Allocate{};
            Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            Allocate.descriptorPool = Pool;
            Allocate.descriptorSetCount = 1;
            Allocate.pSetLayouts = &OutputLayout;
            RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Output.Set));
            const std::array<VkDescriptorBufferInfo, 7> Buffers{
                {{Mesh.VertexBuffer->GetHandle(), 0, VK_WHOLE_SIZE},
                 {Mesh.IndexBuffer->GetHandle(), 0, VK_WHOLE_SIZE},
                 {Mesh.BoundaryBuffer->GetHandle(), 0, VK_WHOLE_SIZE},
                 {Output.Segments->GetHandle(), 0, VK_WHOLE_SIZE},
                 {CoveragePages[CoveragePage]->GetHandle(), CoverageOffset, CoverageBytes},
                 {Output.DrawCommands->GetHandle(), 0, DrawBytes},
                 {Output.TopDrawCommands->GetHandle(), 0, TopDrawBytes}}};
            std::array<VkWriteDescriptorSet, 7> Writes{};
            for (std::uint32_t I = 0; I < Writes.size(); ++I)
            {
                Writes[I].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Writes[I].dstSet = Output.Set;
                Writes[I].dstBinding = I;
                Writes[I].descriptorCount = 1;
                Writes[I].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Writes[I].pBufferInfo = &Buffers[I];
            }
            vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
            Output.VertexCount = Vertices;
            Output.TriangleCount = Triangles;
            Output.BoundaryCount = Boundaries;
            Output.SurfaceCount = SurfaceCount;
            It = Outputs.emplace(Key, std::move(Output)).first;
        }
        if (It->second.Segments->GetSize() != Bytes || It->second.TopDrawCommands->GetSize() != TopDrawBytes ||
            It->second.VertexCount != Vertices || It->second.TriangleCount != Triangles ||
            It->second.BoundaryCount != Boundaries || It->second.SurfaceCount != SurfaceCount)
            throw std::logic_error("Overlay topology changed without rebuilding Scene resources.");

        std::array<VkBufferMemoryBarrier, 4> Barriers{};
        for (auto& Barrier : Barriers)
        {
            Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Barrier.size = VK_WHOLE_SIZE;
        }
        Barriers[0].buffer = It->second.Segments->GetHandle();
        Barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[1].buffer = CoveragePages[It->second.CoveragePage]->GetHandle();
        Barriers[1].offset = It->second.CoverageOffset;
        Barriers[1].size = CoverageBytes;
        Barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[2].buffer = It->second.DrawCommands->GetHandle();
        Barriers[2].srcAccessMask =
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[2].size = DrawBytes;
        Barriers[3].buffer = It->second.TopDrawCommands->GetHandle();
        Barriers[3].srcAccessMask =
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[3].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[3].size = TopDrawBytes;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             static_cast<std::uint32_t>(Barriers.size()),
                             Barriers.data(),
                             0,
                             nullptr);
        WriteStageTimestamp(1U);

        const std::array<VkDescriptorSet, 3> Sets{
            bStateAB ? StateDescriptors.GetABSet() : StateDescriptors.GetBASet(), ComputedSet, It->second.Set};
        vkCmdBindDescriptorSets(Command,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                PipelineLayout,
                                0,
                                static_cast<std::uint32_t>(Sets.size()),
                                Sets.data(),
                                0,
                                nullptr);
        const std::array<std::uint32_t, 6> CoveragePush{
            Vertices, 0U, Channel, Channels, static_cast<std::uint32_t>(Geometry.GetTexelCount()), OccupancyTileSize};
        vkCmdPushConstants(
            Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CoveragePush), CoveragePush.data());
        const auto Dispatch = [&](std::uint32_t Count)
        {
            const std::uint32_t GroupCount = (Count - 1U) / 64U + 1U;
            const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
            const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
            if (GroupsY > Limits.maxComputeWorkGroupCount[1])
                throw std::overflow_error("Overlay side dispatch exceeds Vulkan workgroup limits.");
            vkCmdDispatch(Command, GroupsX, GroupsY, 1);
        };
        vkCmdBindPipeline(
            Command, VK_PIPELINE_BIND_POINT_COMPUTE, bSmoothCoverage ? CoverageSmoothingPipeline : CoveragePipeline);
        Dispatch(std::max(Vertices, SurfaceCount + 2U));
        WriteStageTimestamp(2U);

        std::array<VkBufferMemoryBarrier, 3> CoverageToBoundaryBarriers{};
        CoverageToBoundaryBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        CoverageToBoundaryBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        CoverageToBoundaryBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        CoverageToBoundaryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        CoverageToBoundaryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        CoverageToBoundaryBarriers[0].buffer = CoveragePages[It->second.CoveragePage]->GetHandle();
        CoverageToBoundaryBarriers[0].offset = It->second.CoverageOffset;
        CoverageToBoundaryBarriers[0].size = CoverageBytes;
        CoverageToBoundaryBarriers[1] = CoverageToBoundaryBarriers[0];
        CoverageToBoundaryBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        CoverageToBoundaryBarriers[1].buffer = It->second.DrawCommands->GetHandle();
        CoverageToBoundaryBarriers[1].offset = 0;
        CoverageToBoundaryBarriers[1].size = DrawBytes;
        CoverageToBoundaryBarriers[2] = CoverageToBoundaryBarriers[1];
        CoverageToBoundaryBarriers[2].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        CoverageToBoundaryBarriers[2].buffer = It->second.TopDrawCommands->GetHandle();
        CoverageToBoundaryBarriers[2].size = TopDrawBytes;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             static_cast<std::uint32_t>(CoverageToBoundaryBarriers.size()),
                             CoverageToBoundaryBarriers.data(),
                             0,
                             nullptr);
        WriteStageTimestamp(3U);

        const std::array<std::uint32_t, 6> Push{Triangles,
                                                Boundaries,
                                                Channel,
                                                Channels,
                                                static_cast<std::uint32_t>(Geometry.GetTexelCount()),
                                                OccupancyTileSize};
        vkCmdPushConstants(Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), Push.data());
        const std::uint32_t GroupCount = (SegmentCount - 1U) / 64U + 1U;
        const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
        if (GroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Overlay side dispatch exceeds Vulkan workgroup limits.");

        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, BoundaryPipeline);
        vkCmdDispatch(Command, GroupsX, GroupsY, 1);
        WriteStageTimestamp(4U);

        VkBufferMemoryBarrier CounterBarrier{};
        CounterBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        CounterBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        CounterBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        CounterBarrier.srcQueueFamilyIndex = CounterBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        CounterBarrier.buffer = It->second.DrawCommands->GetHandle();
        CounterBarrier.offset = static_cast<VkDeviceSize>(SurfaceCount + 1U) * sizeof(VkDrawIndirectCommand);
        CounterBarrier.size = sizeof(std::uint32_t);
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &CounterBarrier,
                             0,
                             nullptr);
        WriteStageTimestamp(5U);
        // Surface-specific top commands are now activated directly by OverlaySideBoundary.comp.
        // Keep the legacy profiling slot so existing UI/query indexing remains stable.
        WriteStageTimestamp(6U);
        VkBufferCopy CounterCopy{};
        CounterCopy.srcOffset = CounterBarrier.offset;
        CounterCopy.size = sizeof(std::uint32_t);
        vkCmdCopyBuffer(Command,
                        It->second.DrawCommands->GetHandle(),
                        It->second.ActivityReadbacks[FrameIndex]->GetHandle(),
                        1,
                        &CounterCopy);
        VkBufferMemoryBarrier ReadbackBarrier{};
        ReadbackBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        ReadbackBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        ReadbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        ReadbackBarrier.srcQueueFamilyIndex = ReadbackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ReadbackBarrier.buffer = It->second.ActivityReadbacks[FrameIndex]->GetHandle();
        ReadbackBarrier.size = sizeof(std::uint32_t);
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &ReadbackBarrier,
                             0,
                             nullptr);
        It->second.ActivityPending[FrameIndex] = true;

        Barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        Barriers[0].offset = 0;
        Barriers[0].size = VK_WHOLE_SIZE;
        Barriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[2].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        Barriers[1].buffer = CoveragePages[It->second.CoveragePage]->GetHandle();
        Barriers[1].offset = It->second.CoverageOffset;
        Barriers[1].size = CoverageBytes;
        Barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        Barriers[3].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barriers[3].dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        const std::array<VkBufferMemoryBarrier, 4> DrawBarriers{Barriers[0], Barriers[1], Barriers[2], Barriers[3]};
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                             0,
                             0,
                             nullptr,
                             static_cast<std::uint32_t>(DrawBarriers.size()),
                             DrawBarriers.data(),
                             0,
                             nullptr);
        WriteStageTimestamp(7U);
    }
}
