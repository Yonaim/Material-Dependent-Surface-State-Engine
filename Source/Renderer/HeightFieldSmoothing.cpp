/**
 * @file HeightFieldSmoothing.cpp
 * @brief 표시용 height field smoothing compute pass를 기록한다.
 */
#include "Renderer/HeightFieldSmoothing.h"

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
        constexpr std::uint32_t MaxOverlayLayersPerInstance = 3;

        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS)
                throw std::runtime_error("Failed to create height-field smoothing resources.");
        }
    }

    THeightFieldSmoothing::THeightFieldSmoothing(VkPhysicalDevice      PhysicalDevice,
                                                 VkDevice              Device,
                                                 VkDescriptorSetLayout SurfaceLayout,
                                                 VkDescriptorSetLayout HeightLayout,
                                                 std::size_t           MaxInstances)
        : PhysicalDevice(PhysicalDevice), Device(Device), HeightLayout(HeightLayout)
    {
        if (!SurfaceLayout || !HeightLayout || MaxInstances == 0 ||
            MaxInstances > std::numeric_limits<std::uint32_t>::max() / MaxOverlayLayersPerInstance)
            throw std::invalid_argument("Height-field smoothing requires valid layouts and instance slots.");
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        Limits = Properties.limits;
        const auto StorageBindings = static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count) + 2U;
        if (Limits.maxPerStageDescriptorStorageBuffers < StorageBindings ||
            Limits.maxDescriptorSetStorageBuffers < StorageBindings || Limits.maxComputeWorkGroupInvocations < 64 ||
            Limits.maxComputeWorkGroupSize[0] < 64)
            throw std::runtime_error("Vulkan device lacks height-field smoothing limits.");

        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            const std::array<VkDescriptorSetLayout, 3> Layouts{SurfaceLayout, HeightLayout, HeightLayout};
            const VkPushConstantRange                  Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 20};
            VkPipelineLayoutCreateInfo                 LayoutInfo{};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            LayoutInfo.setLayoutCount = static_cast<std::uint32_t>(Layouts.size());
            LayoutInfo.pSetLayouts = Layouts.data();
            LayoutInfo.pushConstantRangeCount = 1;
            LayoutInfo.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout));

            std::ifstream File(std::string(MDSS_SHADER_DIR) + "/Rendering/Surface/HeightFieldSmoothing.comp.spv",
                               std::ios::binary | std::ios::ate);
            if (!File)
                throw std::runtime_error("Cannot open height-field smoothing compute shader.");
            const auto Size = File.tellg();
            if (Size <= 0 || Size % 4 != 0)
                throw std::runtime_error("Invalid height-field smoothing SPIR-V.");
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File)
                throw std::runtime_error("Cannot read height-field smoothing compute shader.");
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

            // Smoothing output sets are cached per instance and overlay State channel.
            const auto SetCount = static_cast<std::uint32_t>(MaxInstances * MaxOverlayLayersPerInstance);
            const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SetCount * 2U};
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
            if (Pipeline)
                vkDestroyPipeline(Device, Pipeline, nullptr);
            if (PipelineLayout)
                vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            throw;
        }
    }

    THeightFieldSmoothing::~THeightFieldSmoothing()
    {
        Outputs.clear();
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        if (Pipeline)
            vkDestroyPipeline(Device, Pipeline, nullptr);
        if (PipelineLayout)
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
    }

    VkDescriptorSet THeightFieldSmoothing::GetOutputSet(std::size_t Instance, std::uint32_t Channel) const
    {
        return Outputs.at({Instance, Channel}).Set;
    }

    const TGPUBuffer& THeightFieldSmoothing::GetOutputBuffer(std::size_t Instance, std::uint32_t Channel) const
    {
        return *Outputs.at({Instance, Channel}).Buffer;
    }

    void THeightFieldSmoothing::Record(VkCommandBuffer                         Command,
                                       std::size_t                             Instance,
                                       std::uint32_t                           Channel,
                                       std::uint32_t                           Channels,
                                       std::uint32_t                           TexelCount,
                                       const TSurfaceStateDescriptorResources& StateDescriptors,
                                       VkDescriptorSet                         InputSet,
                                       const TGPUBuffer&                       InputBuffer,
                                       bool                                    bStateAB,
                                       float                                   AccumulationDisplayScale)
    {
        const auto Bytes = GetSurfaceGPUBufferByteSize(TexelCount, sizeof(glm::vec4), Limits.maxStorageBufferRange);
        if (!InputSet || InputBuffer.GetSize() != Bytes)
            throw std::invalid_argument("Height-field smoothing input does not match the texel geometry.");
        const auto Key = std::make_pair(Instance, Channel);
        auto       It = Outputs.find(Key);
        if (It == Outputs.end())
        {
            TOutput Output;
            Output.Buffer = std::make_unique<TGPUBuffer>(
                PhysicalDevice, Device, Bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VkDescriptorSetAllocateInfo Allocate{};
            Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            Allocate.descriptorPool = Pool;
            Allocate.descriptorSetCount = 1;
            Allocate.pSetLayouts = &HeightLayout;
            RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Output.Set));
            const VkDescriptorBufferInfo Buffer{Output.Buffer->GetHandle(), 0, Bytes};
            VkWriteDescriptorSet         Write{};
            Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            Write.dstSet = Output.Set;
            Write.dstBinding = 0;
            Write.descriptorCount = 1;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Write.pBufferInfo = &Buffer;
            // The smoothed geometry replaces binding 0, while downstream overlay
            // passes still read the input preview's occupancy GeometryCache at binding 1.
            VkCopyDescriptorSet CopyGeometryCache{};
            CopyGeometryCache.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            CopyGeometryCache.srcSet = InputSet;
            CopyGeometryCache.srcBinding = 1;
            CopyGeometryCache.dstSet = Output.Set;
            CopyGeometryCache.dstBinding = 1;
            CopyGeometryCache.descriptorCount = 1;
            vkUpdateDescriptorSets(Device, 1, &Write, 1, &CopyGeometryCache);
            It = Outputs.emplace(Key, std::move(Output)).first;
        }
        if (It->second.Buffer->GetSize() != Bytes)
            throw std::logic_error("Height-field smoothing size changed without rebuilding Scene resources.");

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

        const std::array<VkDescriptorSet, 3> Sets{
            bStateAB ? StateDescriptors.GetABSet() : StateDescriptors.GetBASet(), InputSet, It->second.Set};
        struct TPush
        {
            std::uint32_t Texels, Channel, Channels, Padding;
            float         DisplayScale;
        };
        static_assert(sizeof(TPush) == 20);
        const TPush Push{TexelCount, Channel, Channels, 0U, AccumulationDisplayScale};
        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        vkCmdBindDescriptorSets(Command,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                PipelineLayout,
                                0,
                                static_cast<std::uint32_t>(Sets.size()),
                                Sets.data(),
                                0,
                                nullptr);
        vkCmdPushConstants(Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);
        const std::uint32_t GroupCount = (TexelCount - 1U) / 64U + 1U;
        const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
        if (GroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Height-field smoothing dispatch exceeds Vulkan workgroup limits.");
        vkCmdDispatch(Command, GroupsX, GroupsY, 1);

        Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &Barrier,
                             0,
                             nullptr);
    }
}
