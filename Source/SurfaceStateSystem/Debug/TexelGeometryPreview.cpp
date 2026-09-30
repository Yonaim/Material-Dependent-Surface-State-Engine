#include "SurfaceStateSystem/Debug/TexelGeometryPreview.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS
{
    namespace
    {
        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS) throw std::runtime_error("Failed to create texel geometry preview resources.");
        }
    }

    TTexelGeometryPreview::TTexelGeometryPreview(VkPhysicalDevice PhysicalDevice, VkDevice Device,
                                                VkDescriptorSetLayout SurfaceLayout, std::size_t MaxInstances)
        : PhysicalDevice(PhysicalDevice), Device(Device)
    {
        if (!SurfaceLayout || MaxInstances == 0 || MaxInstances > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Texel geometry preview requires Surface resources and instance slots.");
        VkPhysicalDeviceProperties Properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
        Limits = Properties.limits;
        constexpr auto DescriptorCount = static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count) + 1U;
        if (Limits.maxPerStageDescriptorStorageBuffers < DescriptorCount ||
            Limits.maxDescriptorSetStorageBuffers < DescriptorCount || Limits.maxComputeWorkGroupSize[0] < 64 ||
            Limits.maxComputeWorkGroupInvocations < 64)
            throw std::runtime_error("Vulkan device does not support texel geometry preview limits.");
        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            const VkDescriptorSetLayoutBinding Binding{
                0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo OutputInfo{};
            OutputInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            OutputInfo.bindingCount = 1;
            OutputInfo.pBindings = &Binding;
            RequireVk(vkCreateDescriptorSetLayout(Device, &OutputInfo, nullptr, &OutputLayout));
            const std::array<VkDescriptorSetLayout, 2> Layouts{SurfaceLayout, OutputLayout};
            const VkPushConstantRange Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 80};
            VkPipelineLayoutCreateInfo Info{};
            Info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            Info.setLayoutCount = Layouts.size();
            Info.pSetLayouts = Layouts.data();
            Info.pushConstantRangeCount = 1;
            Info.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &Info, nullptr, &Layout));
            std::ifstream File(std::string(MDSS_SHADER_DIR) + "/Debug/TexelGeometry.comp.spv", std::ios::binary | std::ios::ate);
            if (!File) throw std::runtime_error("Cannot open texel geometry preview shader.");
            const auto Size = File.tellg();
            if (Size <= 0 || Size % 4 != 0) throw std::runtime_error("Invalid texel geometry preview SPIR-V.");
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File) throw std::runtime_error("Cannot read texel geometry preview shader.");
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
            RequireVk(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline));
            vkDestroyShaderModule(Device, Module, nullptr);
            Module = VK_NULL_HANDLE;
            const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(MaxInstances)};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = static_cast<std::uint32_t>(MaxInstances);
            PoolInfo.poolSizeCount = 1;
            PoolInfo.pPoolSizes = &PoolSize;
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));
        }
        catch (...)
        {
            if (Module) vkDestroyShaderModule(Device, Module, nullptr);
            Destroy();
            throw;
        }
    }

    TTexelGeometryPreview::~TTexelGeometryPreview() { Destroy(); }
    void TTexelGeometryPreview::Destroy() noexcept
    {
        if (Pipeline) vkDestroyPipeline(Device, Pipeline, nullptr);
        if (Layout) vkDestroyPipelineLayout(Device, Layout, nullptr);
        if (Pool) vkDestroyDescriptorPool(Device, Pool, nullptr);
        Outputs.clear();
        if (OutputLayout) vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
    }
    VkDescriptorSet TTexelGeometryPreview::GetOutputSet(std::size_t Instance) const { return Outputs.at(Instance).Set; }
    const TGPUBuffer& TTexelGeometryPreview::GetOutputBuffer(std::size_t Instance) const { return *Outputs.at(Instance).Buffer; }

    void TTexelGeometryPreview::Record(VkCommandBuffer Command, std::size_t Instance,
                                      const TSurfaceStateDescriptorResources& Descriptors, std::uint32_t TexelCount,
                                      std::uint32_t Channel, std::uint32_t Channels, float AccumulationDisplayScale,
                                      float GeometryDisplayScale, const glm::mat4& ModelMatrix,
                                      bool bStateAB, bool bAccumulation)
    {
        const auto Bytes = GetSurfaceGPUBufferByteSize(TexelCount, sizeof(TTexelGeometryVertex), Limits.maxStorageBufferRange);
        auto It = Outputs.find(Instance);
        if (It == Outputs.end())
        {
            TOutput Output;
            Output.Buffer = std::make_unique<TGPUBuffer>(PhysicalDevice, Device, Bytes,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VkDescriptorSetAllocateInfo Allocate{};
            Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            Allocate.descriptorPool = Pool;
            Allocate.descriptorSetCount = 1;
            Allocate.pSetLayouts = &OutputLayout;
            RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Output.Set));
            const VkDescriptorBufferInfo Buffer{Output.Buffer->GetHandle(), 0, Bytes};
            VkWriteDescriptorSet Write{};
            Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            Write.dstSet = Output.Set;
            Write.dstBinding = 0;
            Write.descriptorCount = 1;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            Write.pBufferInfo = &Buffer;
            vkUpdateDescriptorSets(Device, 1, &Write, 0, nullptr);
            It = Outputs.emplace(Instance, std::move(Output)).first;
        }
        if (It->second.Buffer->GetSize() != Bytes)
            throw std::logic_error("Texel geometry size changed without rebuilding Scene resources.");
        const std::uint32_t GroupCount = (TexelCount - 1U) / 64U + 1U;
        const std::uint32_t GroupsX = std::min(GroupCount, Limits.maxComputeWorkGroupCount[0]);
        const std::uint32_t GroupsY = (GroupCount - 1U) / GroupsX + 1U;
        if (GroupsY > Limits.maxComputeWorkGroupCount[1])
            throw std::overflow_error("Texel geometry dispatch exceeds Vulkan workgroup limits.");
        // One GPU-only buffer per Scene instance. A queue barrier protects reuse across frames.
        VkBufferMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        Barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.buffer = It->second.Buffer->GetHandle();
        Barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &Barrier, 0, nullptr);
        struct TPush
        {
            std::uint32_t Texels, Channel, Channels, Accumulation;
            float AccumulationDisplayScale, GeometryDisplayScale;
            std::uint32_t Padding[2];
            std::array<glm::vec4, 3> NormalMatrixColumns;
        };
        static_assert(offsetof(TPush, NormalMatrixColumns) == 32);
        static_assert(sizeof(TPush) == 80);
        glm::mat3 NormalMatrix(0.0F);
        const glm::mat3 ModelLinear(ModelMatrix);
        const float Determinant = glm::determinant(ModelLinear);
        if (std::isfinite(Determinant) && std::abs(Determinant) > 1e-6F)
            NormalMatrix = glm::transpose(glm::inverse(ModelLinear));
        TPush Push{TexelCount, Channel, Channels, bAccumulation ? 1U : 0U,
                   AccumulationDisplayScale, GeometryDisplayScale, {}, {}};
        for (int Column = 0; Column < 3; ++Column)
            Push.NormalMatrixColumns[Column] = glm::vec4(NormalMatrix[Column], 0.0F);
        const std::array<VkDescriptorSet, 2> Sets{bStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet(), It->second.Set};
        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        vkCmdBindDescriptorSets(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 0, Sets.size(), Sets.data(), 0, nullptr);
        vkCmdPushConstants(Command, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);
        vkCmdDispatch(Command, GroupsX, GroupsY, 1);
        Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 1, &Barrier, 0, nullptr);
    }
}
