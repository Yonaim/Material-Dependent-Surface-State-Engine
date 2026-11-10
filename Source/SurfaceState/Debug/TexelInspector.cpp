/**
 * @file TexelInspector.cpp
 * @brief 선택한 texel의 진단 snapshot을 생성하고 조회한다.
 */
#include "SurfaceState/Debug/TexelInspector.h"

#include "ShaderCatalog.h"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace MDSS::SurfaceState
{
    namespace
    {
        constexpr VkDeviceSize SnapshotBytes = sizeof(std::array<glm::vec4, 7>);
        static_assert(SnapshotBytes == 112);
        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS)
                throw std::runtime_error("Failed to create Texel Inspector GPU resources.");
        }
    }

// ============================================================
// 00. Inspector Resource Lifecycle
// ============================================================
#pragma region 00_Inspector_Resource_Lifecycle
// 선택 texel의 GPU 진단과 readback 자원을 관리한다.

    FTexelInspector::FTexelInspector(VkPhysicalDevice      PhysicalDevice,
                                     VkDevice              Device,
                                     VkDescriptorSetLayout SurfaceLayout,
                                     std::size_t           FrameCount)
        : Device(Device)
    {
        if (!SurfaceLayout || FrameCount == 0)
            throw std::invalid_argument("Texel Inspector requires Surface resources and frame slots.");
        VkShaderModule Module = VK_NULL_HANDLE;
        try
        {
            VkDescriptorSetLayoutBinding Binding{
                0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
            VkDescriptorSetLayoutCreateInfo OutputInfo{};
            OutputInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            OutputInfo.bindingCount = 1;
            OutputInfo.pBindings = &Binding;
            RequireVk(vkCreateDescriptorSetLayout(Device, &OutputInfo, nullptr, &OutputLayout));
            const std::array<VkDescriptorSetLayout, 2> Layouts{SurfaceLayout, OutputLayout};
            const VkPushConstantRange                  Push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 64};
            VkPipelineLayoutCreateInfo                 Info{};
            Info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            Info.setLayoutCount = 2;
            Info.pSetLayouts = Layouts.data();
            Info.pushConstantRangeCount = 1;
            Info.pPushConstantRanges = &Push;
            RequireVk(vkCreatePipelineLayout(Device, &Info, nullptr, &Layout));
            std::ifstream File(Shaders::Path(Shaders::EShader::Debug_TexelInspector_comp),
                               std::ios::binary | std::ios::ate);
            if (!File)
                throw std::runtime_error("Cannot open Texel Inspector shader.");
            const auto Size = File.tellg();

            if (Size <= 0 || Size % 4 != 0)
                throw std::runtime_error("Invalid Texel Inspector SPIR-V.");
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);

            if (!File)
                throw std::runtime_error("Cannot read Texel Inspector shader.");
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
            VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<std::uint32_t>(FrameCount)};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = static_cast<std::uint32_t>(FrameCount);
            PoolInfo.poolSizeCount = 1;
            PoolInfo.pPoolSizes = &PoolSize;
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));
            Frames.resize(FrameCount);

            for (auto& Frame : Frames)
            {
                Frame.Buffer = std::make_unique<GPU::FGPUBuffer>(PhysicalDevice,
                                                                 Device,
                                                                 SnapshotBytes,
                                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                 GPU::EGPUBufferMemoryCategory::Debug);
                VkDescriptorSetAllocateInfo Allocate{};
                Allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                Allocate.descriptorPool = Pool;
                Allocate.descriptorSetCount = 1;
                Allocate.pSetLayouts = &OutputLayout;
                RequireVk(vkAllocateDescriptorSets(Device, &Allocate, &Frame.Set));
                VkDescriptorBufferInfo Buffer{Frame.Buffer->GetHandle(), 0, SnapshotBytes};
                VkWriteDescriptorSet   Write{};
                Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Write.dstSet = Frame.Set;
                Write.dstBinding = 0;
                Write.descriptorCount = 1;
                Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Write.pBufferInfo = &Buffer;
                vkUpdateDescriptorSets(Device, 1, &Write, 0, nullptr);
            }
        }
        catch (...)
        {
            if (Module)
                vkDestroyShaderModule(Device, Module, nullptr);
            Destroy();
            throw;
        }
    }

    FTexelInspector::~FTexelInspector()
    {
        Destroy();
    }

    void FTexelInspector::Destroy() noexcept
    {
        if (Pipeline)
            vkDestroyPipeline(Device, Pipeline, nullptr);
        if (Layout)
            vkDestroyPipelineLayout(Device, Layout, nullptr);
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        Frames.clear();

        if (OutputLayout)
            vkDestroyDescriptorSetLayout(Device, OutputLayout, nullptr);
    }
#pragma endregion

// ============================================================
// 01. Inspector Command Recording
// ============================================================
#pragma region 01_Inspector_Command_Recording
// 선택 texel의 State·형상·반응 진단을 읽을 compute 명령을 기록한다.

    void FTexelInspector::Record(VkCommandBuffer                         Command,
                                 std::size_t                             FrameIndex,
                                 const FSurfaceStateDescriptorResources& Descriptors,
                                 const FSurfaceTexelSelection&           Selection,
                                 FStateId                                Channel,
                                 std::uint32_t                           Channels,
                                 float                                   AccumulationDisplayScale,
                                 const glm::mat4&                        ModelMatrix,
                                 bool                                    bStateAB,
                                 std::uint64_t                           Step)
    {
        auto& Frame = Frames.at(FrameIndex);
        struct FPush
        {
            std::uint32_t            Texel, Channel, Channels;
            float                    AccumulationDisplayScale;
            std::array<glm::vec4, 3> NormalMatrixColumns;
        };
        static_assert(sizeof(FPush) == 64);
        glm::mat3       NormalMatrix(0.0F);
        const glm::mat3 ModelLinear(ModelMatrix);
        const float     Determinant = glm::determinant(ModelLinear);

        if (std::isfinite(Determinant) && std::abs(Determinant) > 1e-6F)
            NormalMatrix = glm::transpose(glm::inverse(ModelLinear));
        FPush Push{Selection.Texel, Channel, Channels, AccumulationDisplayScale, {}};

        for (int Column = 0; Column < 3; ++Column)
            Push.NormalMatrixColumns[Column] = glm::vec4(NormalMatrix[Column], 0.0F);
        const std::array<VkDescriptorSet, 2> Sets{bStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet(),
                                                  Frame.Set};
        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        vkCmdBindDescriptorSets(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 0, 2, Sets.data(), 0, nullptr);
        vkCmdPushConstants(Command, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);
        vkCmdDispatch(Command, 1, 1, 1);
        VkBufferMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.buffer = Frame.Buffer->GetHandle();
        Barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &Barrier,
                             0,
                             nullptr);
        Frame.Pending = FSurfaceTexelSnapshot{Selection, Channel, Step, ++Serial, bStateAB, {}};
        Frame.Generation = Generation;
    }
#pragma endregion

// ============================================================
// 02. Inspector Readback
// ============================================================
#pragma region 02_Inspector_Readback
// GPU 완료 후 캡처한 texel 진단을 CPU snapshot으로 읽는다.

    void FTexelInspector::CompleteFrame(std::size_t FrameIndex)
    {
        auto& Frame = Frames.at(FrameIndex);

        if (Frame.Pending && Frame.Generation == Generation && (!Snapshot || Frame.Pending->Serial > Snapshot->Serial))
        {
            Frame.Buffer->Download(Frame.Pending->Values.data(), SnapshotBytes);
            Snapshot = std::move(Frame.Pending);
        }

        Frame.Pending.reset();
    }

    void FTexelInspector::Invalidate() noexcept
    {
        ++Generation;
        Snapshot.reset();
    }
#pragma endregion
}
