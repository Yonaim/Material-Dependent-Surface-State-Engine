/**
 * @file TexelGeometryPreview.cpp
 * @brief 진단용 texel geometry cache와 높이 미리보기 계산을 관리한다.
 */
#include "SurfaceState/Debug/TexelGeometryPreview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS::SurfaceState
{
    namespace
    {
        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS)
                throw std::runtime_error("Failed to create texel geometry preview resources.");
        }
    }

#pragma region Preview_Resource_Lifecycle

    TTexelGeometryPreview::TTexelGeometryPreview(VkPhysicalDevice      PhysicalDevice,
                                                 VkDevice              Device,
                                                 VkDescriptorSetLayout SurfaceLayout,
                                                 std::size_t           MaxInstances,
                                                 bool                  bEnableOccupancyScan)
        : PhysicalDevice(PhysicalDevice), Device(Device), bEnableOccupancyScan(bEnableOccupancyScan)
    {
        if (!SurfaceLayout || MaxInstances == 0 || MaxInstances > std::numeric_limits<std::uint32_t>::max() / 2U)
            throw std::invalid_argument("Texel geometry preview requires Surface resources and instance slots.");
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        Limits = Properties.limits;
        constexpr auto DescriptorCount = static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count) + 2U;
        if (Limits.maxPerStageDescriptorStorageBuffers < DescriptorCount ||
            Limits.maxDescriptorSetStorageBuffers < DescriptorCount || Limits.maxComputeWorkGroupSize[0] < 64 ||
            Limits.maxComputeWorkGroupInvocations < 64)
            throw std::runtime_error("Vulkan device does not support texel geometry preview limits.");
        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            const std::array<VkDescriptorSetLayoutBinding, 2> Bindings{
                {{0,
                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                  1,
                  VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                  nullptr},
                 {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo OutputInfo{};
            OutputInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            OutputInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
            OutputInfo.pBindings = Bindings.data();
            RequireVk(vkCreateDescriptorSetLayout(Device, &OutputInfo, nullptr, &OutputLayout));
            const std::array<VkDescriptorSetLayout, 2> Layouts{SurfaceLayout, OutputLayout};
            const VkPushConstantRange                  Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 96};
            VkPipelineLayoutCreateInfo                 Info{};
            Info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            Info.setLayoutCount = Layouts.size();
            Info.pSetLayouts = Layouts.data();
            Info.pushConstantRangeCount = 1;
            Info.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &Info, nullptr, &Layout));
            const auto CreatePipeline = [&](const char* Directory, const char* ShaderName, VkPipeline& Destination)
            {
                std::ifstream File(std::string(MDSS_SHADER_DIR) + "/" + Directory + "/" + ShaderName + ".spv",
                                   std::ios::binary | std::ios::ate);
                if (!File)
                    throw std::runtime_error("Cannot open texel geometry preview shader.");
                const auto Size = File.tellg();
                if (Size <= 0 || Size % 4 != 0)
                    throw std::runtime_error("Invalid texel geometry preview SPIR-V.");
                std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
                File.seekg(0);
                File.read(reinterpret_cast<char*>(Code.data()), Size);
                if (!File)
                    throw std::runtime_error("Cannot read texel geometry preview shader.");
                VkShaderModuleCreateInfo ModuleInfo{};
                ModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                ModuleInfo.codeSize = static_cast<std::size_t>(Size);
                ModuleInfo.pCode = Code.data();
                RequireVk(vkCreateShaderModule(Device, &ModuleInfo, nullptr, &Module));
                VkComputePipelineCreateInfo PipelineInfo{};
                PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                PipelineInfo.layout = Layout;
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
            CreatePipeline("Debug", "TexelGeometryHeight.comp", HeightPipeline);
            CreatePipeline("Debug", "TexelGeometryBaseline.comp", BaselinePipeline);
            CreatePipeline("Debug", "TexelGeometry.comp", Pipeline);
            if (bEnableOccupancyScan)
                CreatePipeline("Rendering/StateOverlay", "OverlayOccupancyScan.comp", OccupancyPipeline);
            const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                static_cast<std::uint32_t>(MaxInstances * 2U)};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = static_cast<std::uint32_t>(MaxInstances);
            PoolInfo.poolSizeCount = 1;
            PoolInfo.pPoolSizes = &PoolSize;
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));
        }
        catch (...)
        {
            if (Module)
                vkDestroyShaderModule(Device, Module, nullptr);
            Destroy();
            throw;
        }
    }

    TTexelGeometryPreview::~TTexelGeometryPreview()
    {
        Destroy();
    }
    void TTexelGeometryPreview::Destroy() noexcept
    {
        if (Pipeline)
            vkDestroyPipeline(Device, Pipeline, nullptr);
        if (HeightPipeline)
            vkDestroyPipeline(Device, HeightPipeline, nullptr);
        if (BaselinePipeline)
            vkDestroyPipeline(Device, BaselinePipeline, nullptr);
        if (OccupancyPipeline)
            vkDestroyPipeline(Device, OccupancyPipeline, nullptr);
        if (Layout)
            vkDestroyPipelineLayout(Device, Layout, nullptr);
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        Outputs.clear();
        if (OutputLayout)
            vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
    }
#pragma endregion

#pragma region Preview_Accessors_and_Settings

    VkDescriptorSet TTexelGeometryPreview::GetOutputSet(std::size_t Instance) const
    {
        return Outputs.at(Instance).Set;
    }
    const GPU::TGPUBuffer& TTexelGeometryPreview::GetOutputBuffer(std::size_t Instance) const
    {
        return *Outputs.at(Instance).Buffer;
    }
    const GPU::TGPUBuffer& TTexelGeometryPreview::GetGeometryCacheBuffer(std::size_t Instance) const
    {
        return *Outputs.at(Instance).GeometryCache;
    }

    TTexelGeometryPreview::TTileActivity TTexelGeometryPreview::CompleteOccupancyFrame(std::size_t FrameIndex)
    {
        if (FrameIndex >= Rendering::TRenderContext::MaxFramesInFlight)
            throw std::out_of_range("Occupancy frame slot is invalid.");
        TTileActivity Activity;
        for (auto& [Instance, Output] : Outputs)
        {
            (void)Instance;
            if (!Output.OccupancyPending[FrameIndex])
                continue;
            std::uint32_t Active = 0;
            Output.OccupancyReadbacks[FrameIndex]->Download(&Active, sizeof(Active));
            Activity.Active += Active;
            Activity.Total += Output.OccupancyTotalTiles;
            Output.OccupancyPending[FrameIndex] = false;
        }
        return Activity;
    }
    void TTexelGeometryPreview::SetOccupancyTileSize(std::uint32_t TileSize)
    {
        if (TileSize != 8U && TileSize != 16U && TileSize != 32U)
            throw std::invalid_argument("Overlay occupancy tile size must be 8, 16, or 32.");
        if (OccupancyTileSize == TileSize)
            return;
        OccupancyTileSize = TileSize;
        // Packed tile flags are indexed by the selected tile size, so changing the size invalidates
        // both the history bits and the compact dispatch list. The next Record performs a full sparse init.
        for (auto& [Instance, Output] : Outputs)
        {
            (void)Instance;
            Output.bOccupancyInitialized = false;
            Output.bSparseReady = false;
        }
    }
#pragma endregion

#pragma region Preview_Command_Recording

    void TTexelGeometryPreview::Record(VkCommandBuffer                         Command,
                                       std::size_t                             Instance,
                                       const TSurfaceStateDescriptorResources& Descriptors,
                                       std::uint32_t                           TexelCount,
                                       std::uint32_t                           Channel,
                                       std::uint32_t                           Channels,
                                       float                                   AccumulationDisplayScale,
                                       float                                   GeometryDisplayScale,
                                       const glm::mat4&                        ModelMatrix,
                                       bool                                    bStateAB,
                                       bool                                    bAccumulation,
                                       VkQueryPool                             TimestampQueryPool,
                                       std::uint32_t                           HeightCompleteQuery,
                                       bool                                    bTotalHeight,
                                       std::size_t                             FrameIndex,
                                       std::array<std::uint32_t, 3>           MaterialChannels,
                                       std::uint32_t                           ActiveMaterialMask)
    {
        if (FrameIndex >= Rendering::TRenderContext::MaxFramesInFlight)
            throw std::out_of_range("Occupancy frame slot is invalid.");
        const auto Bytes =
            GetSurfaceGPUBufferByteSize(TexelCount, sizeof(TTexelGeometryVertex), Limits.maxStorageBufferRange);
        const auto BaseCacheBytes =
            GetSurfaceGPUBufferByteSize(TexelCount, sizeof(glm::vec4), Limits.maxStorageBufferRange);

        // Sparse overlay metadata is appended after the 4 uint planes used by the height/normal cache.
        // Active occupancy and geometry dirtiness are intentionally separate. Active flags drive
        // coverage/draw culling, while input deltas against the last built input drive geometry work.
        // Layout after the base cache (uint words):
        //   activeFlags[N]                                  persistent
        //   builtState[TexelCount], builtCoverage[TexelCount] persistent
        //   height/normal/coverageScheduled[N]              dynamic
        //   heightCount, heightDispatch[3], heightList[N]   dynamic
        //   normalCount, normalDispatch[3], normalList[N]   dynamic
        //   coverageCount, coverageDispatch[3], coverageList[N] dynamic
        //   globalAny, activeTileCount                      dynamic (last two words)
        // N includes padding at every surface edge for the smallest supported tile (8x8).
        const auto& SurfaceRanges = Descriptors.GetSharedGeometry().GetSurfaceRanges();
        const auto CountTiles = [&SurfaceRanges](std::uint32_t TileSize)
        {
            std::size_t Count = 0;
            for (const auto& Range : SurfaceRanges)
                Count += ((static_cast<std::size_t>(Range.Width) + TileSize - 1U) / TileSize) *
                         ((static_cast<std::size_t>(Range.Height) + TileSize - 1U) / TileSize);
            return Count;
        };
        const auto MaxTileCount = CountTiles(8U);
        const auto SelectedTileCount = CountTiles(OccupancyTileSize);
        if (MaxTileCount == 0 || MaxTileCount > Limits.maxComputeWorkGroupCount[0] ||
            MaxTileCount > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("Overlay tile capacity exceeds Vulkan workgroup limits.");
        const auto SparseWordCount = static_cast<std::size_t>(TexelCount) * 2U +
                                     static_cast<std::size_t>(MaxTileCount) * 7U + 14U;
        const auto SparseBytes = SparseWordCount * sizeof(std::uint32_t);
        const auto CacheBytes = BaseCacheBytes + SparseBytes;
        const VkDeviceSize PersistentSparseBytes =
            (static_cast<VkDeviceSize>(MaxTileCount) + static_cast<VkDeviceSize>(TexelCount) * 2U) *
            sizeof(std::uint32_t);
        const VkDeviceSize DynamicSparseOffset = BaseCacheBytes + PersistentSparseBytes;
        const VkDeviceSize DynamicSparseBytes = SparseBytes - PersistentSparseBytes;
        const VkDeviceSize HeightDispatchIndirectOffset =
            BaseCacheBytes +
            (static_cast<VkDeviceSize>(TexelCount) * 2U + static_cast<VkDeviceSize>(MaxTileCount) * 4U + 1U) *
                sizeof(std::uint32_t);
        const VkDeviceSize NormalDispatchIndirectOffset =
            BaseCacheBytes +
            (static_cast<VkDeviceSize>(TexelCount) * 2U + static_cast<VkDeviceSize>(MaxTileCount) * 5U + 5U) *
                sizeof(std::uint32_t);

        auto It = Outputs.find(Instance);
        if (It == Outputs.end())
        {
            TOutput Output;
            Output.Buffer =
                std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                  Device,
                                                  Bytes,
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                  GPU::TGPUBufferMemoryCategory::Debug);
            Output.GeometryCache =
                std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                  Device,
                                                  CacheBytes,
                                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                                      VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                  GPU::TGPUBufferMemoryCategory::Debug);
            for (auto& Readback : Output.OccupancyReadbacks)
                Readback = std::make_unique<GPU::TGPUBuffer>(PhysicalDevice,
                                                             Device,
                                                             sizeof(std::uint32_t),
                                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                             GPU::TGPUBufferMemoryCategory::Debug);
            VkDescriptorSetAllocateInfo Allocate{};
            Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            Allocate.descriptorPool = Pool;
            Allocate.descriptorSetCount = 1;
            Allocate.pSetLayouts = &OutputLayout;
            RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Output.Set));
            const std::array<VkDescriptorBufferInfo, 2> DescriptorBuffers{
                {{Output.Buffer->GetHandle(), 0, Bytes}, {Output.GeometryCache->GetHandle(), 0, CacheBytes}}};
            std::array<VkWriteDescriptorSet, 2> Writes{};
            for (std::uint32_t Binding = 0; Binding < Writes.size(); ++Binding)
            {
                Writes[Binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Writes[Binding].dstSet = Output.Set;
                Writes[Binding].dstBinding = Binding;
                Writes[Binding].descriptorCount = 1;
                Writes[Binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Writes[Binding].pBufferInfo = &DescriptorBuffers[Binding];
            }
            vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
            It = Outputs.emplace(Instance, std::move(Output)).first;
        }
        if (It->second.Buffer->GetSize() != Bytes || It->second.GeometryCache->GetSize() != CacheBytes)
            throw std::logic_error("Texel geometry size changed without rebuilding Scene resources.");

        const std::uint32_t GroupCount = (TexelCount - 1U) / 64U + 1U;
        const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
        if (GroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Texel geometry dispatch exceeds Vulkan workgroup limits.");

        const std::uint32_t TileGroupCount = static_cast<std::uint32_t>(SelectedTileCount);
        const std::uint32_t TileGroupsX = std::min(TileGroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t TileGroupsY = (TileGroupCount - 1U) / TileGroupsX + 1U;
        if (TileGroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Overlay occupancy dispatch exceeds Vulkan workgroup limits.");

        VkBufferMemoryBarrier SparseBarrier{};
        SparseBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        SparseBarrier.srcQueueFamilyIndex = SparseBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        SparseBarrier.buffer = It->second.GeometryCache->GetHandle();

        // Active flags and the per-texel state captured at the last geometry build survive across
        // frames. Only schedules/counters/indirect args/lists are reset every frame. On creation or
        // tile-size changes the complete sparse area is initialized.
        if (!It->second.bOccupancyInitialized)
        {
            SparseBarrier.offset = BaseCacheBytes;
            SparseBarrier.size = SparseBytes;
            SparseBarrier.srcAccessMask =
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            SparseBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &SparseBarrier,
                                 0,
                                 nullptr);
            vkCmdFillBuffer(Command,
                            It->second.GeometryCache->GetHandle(),
                            BaseCacheBytes,
                            SparseBytes,
                            bEnableOccupancyScan ? 0U : 0xffffffffU);
            SparseBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            SparseBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &SparseBarrier,
                                 0,
                                 nullptr);
            It->second.bOccupancyInitialized = true;
            It->second.bSparseReady = false;
        }
        else if (bEnableOccupancyScan)
        {
            SparseBarrier.offset = DynamicSparseOffset;
            SparseBarrier.size = DynamicSparseBytes;
            SparseBarrier.srcAccessMask =
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
            SparseBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &SparseBarrier,
                                 0,
                                 nullptr);
            vkCmdFillBuffer(Command,
                            It->second.GeometryCache->GetHandle(),
                            DynamicSparseOffset,
                            DynamicSparseBytes,
                            0U);
            // The transfer reset touches only the dynamic tail, but the occupancy pass also reads
            // builtState written by the previous frame. Cover the complete sparse region here so
            // both the transfer reset and the persistent compute writes are visible to this scan.
            SparseBarrier.offset = BaseCacheBytes;
            SparseBarrier.size = SparseBytes;
            SparseBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            SparseBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &SparseBarrier,
                                 0,
                                 nullptr);
        }

        if (bEnableOccupancyScan)
        {
            It->second.OccupancyTotalTiles = TileGroupCount;
            const std::array<VkDescriptorSet, 2> OccupancySets{
                bStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet(), It->second.Set};
            std::array<std::uint32_t, 24> OccupancyPush{};
            OccupancyPush[0] = TexelCount;
            OccupancyPush[1] = Channel;
            OccupancyPush[2] = Channels;
            OccupancyPush[3] = OccupancyTileSize;
            OccupancyPush[4] = It->second.bSparseReady ? 0U : 1U;
            OccupancyPush[5] = MaterialChannels[0];
            OccupancyPush[6] = MaterialChannels[1];
            OccupancyPush[7] = MaterialChannels[2];
            OccupancyPush[8] = ActiveMaterialMask & 0x7U;
            vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, OccupancyPipeline);
            vkCmdBindDescriptorSets(Command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    Layout,
                                    0,
                                    static_cast<std::uint32_t>(OccupancySets.size()),
                                    OccupancySets.data(),
                                    0,
                                    nullptr);
            vkCmdPushConstants(
                Command, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(OccupancyPush), OccupancyPush.data());
            vkCmdDispatch(Command, TileGroupsX, TileGroupsY, 1);

            SparseBarrier.offset = BaseCacheBytes;
            SparseBarrier.size = SparseBytes;
            SparseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            SparseBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
                                          VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &SparseBarrier,
                                 0,
                                 nullptr);
            VkBufferCopy OccupancyCopy{};
            OccupancyCopy.srcOffset = It->second.GeometryCache->GetSize() - sizeof(std::uint32_t);
            OccupancyCopy.size = sizeof(std::uint32_t);
            vkCmdCopyBuffer(Command,
                            It->second.GeometryCache->GetHandle(),
                            It->second.OccupancyReadbacks[FrameIndex]->GetHandle(),
                            1,
                            &OccupancyCopy);
            VkBufferMemoryBarrier OccupancyReadbackBarrier{};
            OccupancyReadbackBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            OccupancyReadbackBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            OccupancyReadbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            OccupancyReadbackBarrier.srcQueueFamilyIndex = OccupancyReadbackBarrier.dstQueueFamilyIndex =
                VK_QUEUE_FAMILY_IGNORED;
            OccupancyReadbackBarrier.buffer = It->second.OccupancyReadbacks[FrameIndex]->GetHandle();
            OccupancyReadbackBarrier.size = sizeof(std::uint32_t);
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &OccupancyReadbackBarrier,
                                 0,
                                 nullptr);
            It->second.OccupancyPending[FrameIndex] = true;
            It->second.bSparseReady = true;
        }

        // One GPU-only output buffer per Scene instance. A queue barrier protects reuse across frames.
        VkBufferMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        Barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.buffer = It->second.Buffer->GetHandle();
        Barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &Barrier,
                             0,
                             nullptr);

        struct TPush
        {
            std::uint32_t            Texels, Channel, Channels, Accumulation;
            float                    AccumulationDisplayScale, GeometryDisplayScale;
            std::uint32_t            TotalHeight, UseBaseline, TileSize, UseActiveTileList, Padding[2];
            std::array<glm::vec4, 3> NormalMatrixColumns;
        };
        static_assert(offsetof(TPush, NormalMatrixColumns) == 48);
        static_assert(sizeof(TPush) == 96);
        glm::mat3       NormalMatrix(0.0F);
        const glm::mat3 ModelLinear(ModelMatrix);
        const float     Determinant = glm::determinant(ModelLinear);
        if (std::isfinite(Determinant) && std::abs(Determinant) > 1e-6F)
            NormalMatrix = glm::transpose(glm::inverse(ModelLinear));

        bool bBuildBaseline = false;
        if (bAccumulation && !bTotalHeight)
        {
            const auto Revisions = Descriptors.GetGeometryInputRevisions();
            if (!It->second.bInputsObserved || It->second.ObservedInputRevisions != Revisions ||
                It->second.BaselineScale != GeometryDisplayScale)
            {
                It->second.ObservedInputRevisions = Revisions;
                It->second.BaselineScale = GeometryDisplayScale;
                It->second.bInputsObserved = true;
                It->second.bBaselineReady = false;
            }
            else if (!It->second.bBaselineReady)
            {
                bBuildBaseline = true;
                It->second.bBaselineReady = true;
            }
        }

        TPush Push{TexelCount,
                   Channel,
                   Channels,
                   bAccumulation ? 1U : 0U,
                   AccumulationDisplayScale,
                   GeometryDisplayScale,
                   bTotalHeight ? 1U : 0U,
                   bAccumulation && !bTotalHeight && It->second.bBaselineReady ? 1U : 0U,
                   OccupancyTileSize,
                   bEnableOccupancyScan ? 1U : 0U,
                   {0U, 0U},
                   {}};
        for (int Column = 0; Column < 3; ++Column)
            Push.NormalMatrixColumns[Column] = glm::vec4(NormalMatrix[Column], 0.0F);
        const std::array<VkDescriptorSet, 2> Sets{bStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet(),
                                                  It->second.Set};
        vkCmdBindDescriptorSets(
            Command, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 0, Sets.size(), Sets.data(), 0, nullptr);
        vkCmdPushConstants(Command, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);

        if (bBuildBaseline)
        {
            VkBufferMemoryBarrier BaselineBarrier{};
            BaselineBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            BaselineBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            BaselineBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            BaselineBarrier.srcQueueFamilyIndex = BaselineBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            BaselineBarrier.buffer = It->second.GeometryCache->GetHandle();
            BaselineBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &BaselineBarrier,
                                 0,
                                 nullptr);
            vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, BaselinePipeline);
            vkCmdDispatch(Command, GroupsX, GroupsY, 1);
            BaselineBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            BaselineBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &BaselineBarrier,
                                 0,
                                 nullptr);
        }

        if (bAccumulation && !bTotalHeight)
        {
            VkBufferMemoryBarrier HeightBarrier{};
            HeightBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            HeightBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            HeightBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            HeightBarrier.srcQueueFamilyIndex = HeightBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            HeightBarrier.buffer = It->second.GeometryCache->GetHandle();
            HeightBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &HeightBarrier,
                                 0,
                                 nullptr);
            vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, HeightPipeline);
            if (bEnableOccupancyScan)
                vkCmdDispatchIndirect(Command, It->second.GeometryCache->GetHandle(), HeightDispatchIndirectOffset);
            else
                vkCmdDispatch(Command, GroupsX, GroupsY, 1);
            HeightBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            HeightBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &HeightBarrier,
                                 0,
                                 nullptr);
        }

        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(Command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, HeightCompleteQuery);

        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        if (bEnableOccupancyScan)
            vkCmdDispatchIndirect(Command, It->second.GeometryCache->GetHandle(), NormalDispatchIndirectOffset);
        else
            vkCmdDispatch(Command, GroupsX, GroupsY, 1);

        Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &Barrier,
                             0,
                             nullptr);
    }
#pragma endregion
}
