#include "Renderer/AccumulationOverlaySides.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS
{
    namespace
    {
        struct alignas(16) TSideSegment
        {
            glm::vec4 BaseA, TopA, BaseB, TopB, Inside;
        };
        static_assert(sizeof(TSideSegment) == 80);

        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS) throw std::runtime_error("Failed to create accumulation overlay side resources.");
        }
    }

    TAccumulationOverlaySides::TAccumulationOverlaySides(VkPhysicalDevice PhysicalDevice, VkDevice Device,
        VkDescriptorSetLayout SurfaceLayout, VkDescriptorSetLayout ComputedLayout, std::size_t MaxInstances)
        : PhysicalDevice(PhysicalDevice), Device(Device)
    {
        if (!SurfaceLayout || !ComputedLayout || MaxInstances == 0 ||
            MaxInstances > std::numeric_limits<std::uint32_t>::max() / 2U)
            throw std::invalid_argument("Accumulation overlay sides require valid layouts and instance slots.");
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        Limits = Properties.limits;
        const auto StorageBindings = static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count) + 5U;
        if (Limits.maxPerStageDescriptorStorageBuffers < StorageBindings ||
            Limits.maxDescriptorSetStorageBuffers < StorageBindings ||
            Limits.maxComputeWorkGroupInvocations < 64 || Limits.maxComputeWorkGroupSize[0] < 64)
            throw std::runtime_error("Vulkan device lacks accumulation overlay side limits.");

        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            std::array<VkDescriptorSetLayoutBinding, 4> Bindings{};
            for (std::uint32_t I = 0; I < Bindings.size(); ++I)
            {
                Bindings[I].binding = I;
                Bindings[I].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Bindings[I].descriptorCount = 1;
                Bindings[I].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT |
                    (I == 3 ? VK_SHADER_STAGE_VERTEX_BIT : 0);
            }
            VkDescriptorSetLayoutCreateInfo OutputInfo{};
            OutputInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            OutputInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
            OutputInfo.pBindings = Bindings.data();
            RequireVk(vkCreateDescriptorSetLayout(Device, &OutputInfo, nullptr, &OutputLayout));

            const std::array<VkDescriptorSetLayout, 3> Layouts{SurfaceLayout, ComputedLayout, OutputLayout};
            const VkPushConstantRange Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
            VkPipelineLayoutCreateInfo LayoutInfo{};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            LayoutInfo.setLayoutCount = static_cast<std::uint32_t>(Layouts.size());
            LayoutInfo.pSetLayouts = Layouts.data();
            LayoutInfo.pushConstantRangeCount = 1;
            LayoutInfo.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout));

            std::ifstream File(std::string(MDSS_SHADER_DIR) + "/Rendering/OverlaySides.comp.spv",
                               std::ios::binary | std::ios::ate);
            if (!File) throw std::runtime_error("Cannot open overlay side compute shader.");
            const auto Size = File.tellg();
            if (Size <= 0 || Size % 4 != 0) throw std::runtime_error("Invalid overlay side SPIR-V.");
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File) throw std::runtime_error("Cannot read overlay side compute shader.");
            VkShaderModuleCreateInfo ModuleInfo{};
            ModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ModuleInfo.codeSize = static_cast<std::size_t>(Size);
            ModuleInfo.pCode = Code.data();
            RequireVk(vkCreateShaderModule(Device, &ModuleInfo, nullptr, &Module));
            VkComputePipelineCreateInfo PipelineInfo{};
            PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            PipelineInfo.layout = PipelineLayout;
            PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            PipelineInfo.stage.module = Module;
            PipelineInfo.stage.pName = "main";
            RequireVk(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline));
            vkDestroyShaderModule(Device, Module, nullptr);
            Module = VK_NULL_HANDLE;

            const std::uint32_t SetCount = static_cast<std::uint32_t>(MaxInstances * 2U);
            const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SetCount * 4U};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = SetCount;
            PoolInfo.poolSizeCount = 1;
            PoolInfo.pPoolSizes = &PoolSize;
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));
        }
        catch (...)
        {
            if (Module) vkDestroyShaderModule(Device, Module, nullptr);
            if (Pool) vkDestroyDescriptorPool(Device, Pool, nullptr);
            if (Pipeline) vkDestroyPipeline(Device, Pipeline, nullptr);
            if (PipelineLayout) vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            if (OutputLayout) vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
            throw;
        }
    }

    TAccumulationOverlaySides::~TAccumulationOverlaySides()
    {
        Outputs.clear();
        if (Pool) vkDestroyDescriptorPool(Device, Pool, nullptr);
        if (Pipeline) vkDestroyPipeline(Device, Pipeline, nullptr);
        if (PipelineLayout) vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        if (OutputLayout) vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
    }

    VkDescriptorSet TAccumulationOverlaySides::GetSet(std::size_t Instance, std::uint32_t Channel) const
    {
        return Outputs.at({Instance, Channel}).Set;
    }
    std::uint32_t TAccumulationOverlaySides::GetTriangleCount(std::size_t Instance, std::uint32_t Channel) const
    {
        return Outputs.at({Instance, Channel}).TriangleCount;
    }
    std::uint32_t TAccumulationOverlaySides::GetBoundaryCount(std::size_t Instance, std::uint32_t Channel) const
    {
        return Outputs.at({Instance, Channel}).BoundaryCount;
    }

    void TAccumulationOverlaySides::Record(VkCommandBuffer Command, std::size_t Instance, std::uint32_t Channel,
        std::uint32_t Channels, const TSurfaceSharedGeometryGPUResources& Geometry,
        const TSurfaceStateDescriptorResources& StateDescriptors, VkDescriptorSet ComputedSet, bool bStateAB)
    {
        if (!Geometry.GetTexelMeshIndexBuffer() || !Geometry.GetTexelMeshVertexBuffer() ||
            !Geometry.GetTexelMeshBoundaryBuffer()) return;
        const auto Triangles = static_cast<std::uint32_t>(Geometry.GetTexelMeshIndexBuffer()->GetSize() /
                                                           (3U * sizeof(std::uint32_t)));
        const auto Boundaries = Geometry.GetTexelMeshBoundaryCount();
        const std::uint32_t SegmentCount = Triangles + Boundaries;
        if (SegmentCount == 0) return;
        const auto Bytes = GetSurfaceGPUBufferByteSize(SegmentCount, sizeof(TSideSegment), Limits.maxStorageBufferRange);
        const auto Key = std::make_pair(Instance, Channel);
        auto It = Outputs.find(Key);
        if (It == Outputs.end())
        {
            TOutput Output;
            Output.Segments = std::make_unique<TGPUBuffer>(PhysicalDevice, Device, Bytes,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VkDescriptorSetAllocateInfo Allocate{};
            Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            Allocate.descriptorPool = Pool;
            Allocate.descriptorSetCount = 1;
            Allocate.pSetLayouts = &OutputLayout;
            RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Output.Set));
            const std::array<VkDescriptorBufferInfo, 4> Buffers{{
                {Geometry.GetTexelMeshVertexBuffer()->GetHandle(), 0, VK_WHOLE_SIZE},
                {Geometry.GetTexelMeshIndexBuffer()->GetHandle(), 0, VK_WHOLE_SIZE},
                {Geometry.GetTexelMeshBoundaryBuffer()->GetHandle(), 0, VK_WHOLE_SIZE},
                {Output.Segments->GetHandle(), 0, VK_WHOLE_SIZE}}};
            std::array<VkWriteDescriptorSet, 4> Writes{};
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
            Output.TriangleCount = Triangles;
            Output.BoundaryCount = Boundaries;
            It = Outputs.emplace(Key, std::move(Output)).first;
        }
        if (It->second.Segments->GetSize() != Bytes || It->second.TriangleCount != Triangles ||
            It->second.BoundaryCount != Boundaries)
            throw std::logic_error("Overlay topology changed without rebuilding Scene resources.");

        VkBufferMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        Barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.buffer = It->second.Segments->GetHandle();
        Barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);

        const std::array<VkDescriptorSet, 3> Sets{
            bStateAB ? StateDescriptors.GetABSet() : StateDescriptors.GetBASet(), ComputedSet, It->second.Set};
        const std::array<std::uint32_t, 4> Push{Triangles, Boundaries, Channel, Channels};
        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        vkCmdBindDescriptorSets(Command, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0,
                                static_cast<std::uint32_t>(Sets.size()), Sets.data(), 0, nullptr);
        vkCmdPushConstants(Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), Push.data());
        const std::uint32_t GroupCount = (SegmentCount - 1U) / 64U + 1U;
        const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
        if (GroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Overlay side dispatch exceeds Vulkan workgroup limits.");
        vkCmdDispatch(Command, GroupsX, GroupsY, 1);
        Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0, 0, nullptr, 1, &Barrier, 0, nullptr);
    }
}
