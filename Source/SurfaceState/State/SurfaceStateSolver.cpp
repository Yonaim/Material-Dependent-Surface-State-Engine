/**
 * @file SurfaceStateSolver.cpp
 * @brief 선택적 동적 형상 갱신과 2-pass GPU Surface State solver를 기록한다.
 */

#include "SurfaceState/State/SurfaceStateSolver.h"

#include "SurfaceState/Types/SurfaceSolverRates.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS::SurfaceState
{
    namespace
    {
        std::vector<std::uint32_t> ReadSpirvFile(const char* Path)
        {
            std::ifstream File(Path, std::ios::ate | std::ios::binary);
            if (!File.is_open())
            {
                throw std::runtime_error(std::string("Failed to open compute shader: ") + Path);
            }

            const std::streamsize Size = File.tellg();
            if (Size <= 0 || (Size % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0)
            {
                throw std::runtime_error(std::string("Invalid SPIR-V file size: ") + Path);
            }

            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / sizeof(std::uint32_t));
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File)
            {
                throw std::runtime_error(std::string("Failed to read compute shader: ") + Path);
            }
            return Code;
        }

        VkBufferMemoryBarrier MakeComputeBufferBarrier(VkBuffer      Buffer,
                                                       VkAccessFlags DestinationAccess,
                                                       VkAccessFlags SourceAccess = VK_ACCESS_SHADER_WRITE_BIT)
        {
            VkBufferMemoryBarrier Barrier{};
            Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            Barrier.srcAccessMask = SourceAccess;
            Barrier.dstAccessMask = DestinationAccess;
            Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Barrier.buffer = Buffer;
            Barrier.offset = 0;
            Barrier.size = VK_WHOLE_SIZE;
            return Barrier;
        }

        TSurfaceSolverPushConstants MakePushConstants(const TSurfaceSolverInstanceStep& Step)
        {
            TSurfaceSolverPushConstants Constants{};
            Constants.DeltaTime = Step.DeltaTime;
            Constants.StateChannelCount = static_cast<std::uint32_t>(Step.ChannelCount);
            Constants.LocalTexelCount = static_cast<std::uint32_t>(Step.TexelCount);
            Constants.Flags = Step.SolverFlags;
            Constants.GravityWorld = {Step.GravityWorld.x, Step.GravityWorld.y, Step.GravityWorld.z, 0.0F};

            constexpr float GeometryEpsilon = 1.0e-6F;
            const glm::mat3  ModelLinear(Step.ModelMatrix);
            glm::mat3        NormalMatrix(0.0F);
            glm::vec3        Up(0.0F);
            const float      Determinant = glm::determinant(ModelLinear);
            if (std::isfinite(Determinant) && std::abs(Determinant) > GeometryEpsilon)
                NormalMatrix = glm::transpose(glm::inverse(ModelLinear));
            const float GravityLength = glm::length(Step.GravityWorld);
            if ((Step.SolverFlags & 1U) == 0U && std::isfinite(GravityLength) && GravityLength > GeometryEpsilon)
                Up = -Step.GravityWorld / GravityLength;

            for (std::size_t Column = 0; Column < 3; ++Column)
            {
                for (std::size_t Row = 0; Row < 3; ++Row)
                {
                    Constants.ModelLinearColumns[Column][Row] =
                        ModelLinear[static_cast<glm::length_t>(Column)][static_cast<glm::length_t>(Row)];
                    Constants.NormalMatrixAndUpColumns[Column][Row] =
                        NormalMatrix[static_cast<glm::length_t>(Column)][static_cast<glm::length_t>(Row)];
                }
                Constants.NormalMatrixAndUpColumns[Column][3] = Up[static_cast<glm::length_t>(Column)];
            }
            return Constants;
        }

        void RecordBufferBarriers(VkCommandBuffer CommandBuffer,
                                  VkPipelineStageFlags SourceStages,
                                  VkPipelineStageFlags DestinationStages,
                                  const std::vector<VkBufferMemoryBarrier>& Barriers)
        {
            if (Barriers.empty())
                return;
            vkCmdPipelineBarrier(CommandBuffer,
                                 SourceStages,
                                 DestinationStages,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(Barriers.size()),
                                 Barriers.data(),
                                 0,
                                 nullptr);
        }
    } // namespace

    TSurfaceStateSolver::TSurfaceStateSolver(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout)
        : Device(Device)
    {
        if (Device == VK_NULL_HANDLE || DescriptorSetLayout == VK_NULL_HANDLE)
        {
            throw std::invalid_argument("Surface State solver requires a device and descriptor set layout.");
        }

        VkPushConstantRange PushConstants{};
        PushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        PushConstants.offset = 0;
        PushConstants.size = sizeof(TSurfaceSolverPushConstants);

        VkPipelineLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        LayoutInfo.setLayoutCount = 1;
        LayoutInfo.pSetLayouts = &DescriptorSetLayout;
        LayoutInfo.pushConstantRangeCount = 1;
        LayoutInfo.pPushConstantRanges = &PushConstants;
        if (vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Surface State compute pipeline layout.");
        }

        try
        {
            const std::string ShaderRoot = MDSS_SHADER_DIR;
            AccumulationHeightPipeline = CreateComputePipeline(
                Device, PipelineLayout, (ShaderRoot + "/Simulation/SurfaceAccumulationHeight.comp.spv").c_str());
            DirtyDispatchPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicUpdateDispatch.comp.spv").c_str());
            AccumulationGeometryPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicGeometryUpdate.comp.spv").c_str());
            DynamicTransferWeightPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicWeightsUpdate.comp.spv").c_str());
            Pass1Pipeline = CreateComputePipeline(
                Device, PipelineLayout, (ShaderRoot + "/Simulation/SurfaceSolver/SurfaceSolverPass1.comp.spv").c_str());
            Pass2Pipeline = CreateComputePipeline(
                Device, PipelineLayout, (ShaderRoot + "/Simulation/SurfaceSolver/SurfaceSolverPass2.comp.spv").c_str());
        }
        catch (...)
        {
            if (AccumulationHeightPipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, AccumulationHeightPipeline, nullptr);
            if (DirtyDispatchPipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, DirtyDispatchPipeline, nullptr);
            if (AccumulationGeometryPipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, AccumulationGeometryPipeline, nullptr);
            if (DynamicTransferWeightPipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, DynamicTransferWeightPipeline, nullptr);
            if (Pass1Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Pass1Pipeline, nullptr);
            if (Pass2Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Pass2Pipeline, nullptr);
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            PipelineLayout = VK_NULL_HANDLE;
            throw;
        }
    }

    TSurfaceStateSolver::~TSurfaceStateSolver()
    {
        if (AccumulationHeightPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, AccumulationHeightPipeline, nullptr);
        if (DirtyDispatchPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, DirtyDispatchPipeline, nullptr);
        if (AccumulationGeometryPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, AccumulationGeometryPipeline, nullptr);
        if (DynamicTransferWeightPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, DynamicTransferWeightPipeline, nullptr);
        if (Pass1Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, Pass1Pipeline, nullptr);
        if (Pass2Pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(Device, Pass2Pipeline, nullptr);
        if (PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        }
    }

    void TSurfaceStateSolver::RecordStep(VkCommandBuffer                         CommandBuffer,
                                         const TSurfaceStateDescriptorResources& Descriptors,
                                         bool                                    bCurrentStateAB,
                                         std::size_t                             TexelCount,
                                         std::size_t                             ChannelCount,
                                         float                                   DeltaTime,
                                         const glm::mat4&                        ModelMatrix,
                                         const glm::vec3&                        GravityWorld,
                                         std::uint32_t                           SolverFlags,
                                         VkQueryPool                             TimestampQueryPool,
                                         std::uint32_t                           FirstPassQuery) const
    {
        const TSurfaceSolverInstanceStep Step{
            &Descriptors,
            bCurrentStateAB,
            TexelCount,
            ChannelCount,
            DeltaTime,
            ModelMatrix,
            GravityWorld,
            SolverFlags};
        RecordSteps(CommandBuffer,
                    std::span<const TSurfaceSolverInstanceStep>(&Step, 1U),
                    TimestampQueryPool,
                    FirstPassQuery);
    }

    void TSurfaceStateSolver::RecordSteps(VkCommandBuffer CommandBuffer,
                                          std::span<const TSurfaceSolverInstanceStep> InstanceSteps,
                                          VkQueryPool   TimestampQueryPool,
                                          std::uint32_t FirstStepQuery) const
    {
        if (CommandBuffer == VK_NULL_HANDLE)
            throw std::invalid_argument("Surface State solver requires a command buffer.");
        if (InstanceSteps.empty())
            return;

        struct TPreparedStep
        {
            const TSurfaceSolverInstanceStep* Step = nullptr;
            VkDescriptorSet                   DescriptorSet = VK_NULL_HANDLE;
            TSurfaceSolverPushConstants       Constants{};
            std::uint32_t                     WorkgroupCount = 0U;
            VkBuffer                          HeightBuffer = VK_NULL_HANDLE;
            VkDeviceSize                      IndirectOffset = 0U;
            bool                              bUpdateGeometry = false;
        };

        std::vector<TPreparedStep> Prepared;
        Prepared.reserve(InstanceSteps.size());
        for (const TSurfaceSolverInstanceStep& Step : InstanceSteps)
        {
            if (Step.Descriptors == nullptr || Step.TexelCount == 0 || Step.ChannelCount == 0 ||
                Step.TexelCount > std::numeric_limits<std::uint32_t>::max() ||
                Step.ChannelCount > std::numeric_limits<std::uint32_t>::max() || !std::isfinite(Step.DeltaTime) ||
                Step.DeltaTime < 0.0F)
                throw std::invalid_argument("Surface State solver step received invalid dimensions or DeltaTime.");
            if (Step.TexelCount > static_cast<std::size_t>(65535U) * 64U)
                throw std::length_error("Surface State solver dispatch exceeds Vulkan's minimum X workgroup limit.");

            TPreparedStep Item;
            Item.Step = &Step;
            Item.DescriptorSet = Step.bCurrentStateAB ? Step.Descriptors->GetABSet() : Step.Descriptors->GetBASet();
            Item.Constants = MakePushConstants(Step);
            Item.WorkgroupCount = static_cast<std::uint32_t>((Step.TexelCount + 63U) / 64U);
            Item.HeightBuffer = Step.Descriptors->GetBoundBufferHandle(
                TSurfaceGPUDescriptorBinding::AccumulationHeights, Step.bCurrentStateAB);
            Item.IndirectOffset = static_cast<VkDeviceSize>(
                (3U * Step.TexelCount + Item.WorkgroupCount) * sizeof(std::uint32_t));
            Item.bUpdateGeometry = (Step.SolverFlags & SurfaceSolverAccumulationGeometryUpdateFlag) != 0U;
            Prepared.push_back(Item);
        }

        // Wait once for prior rendering/compute reads before any instance reuses its state buffers.
        VkMemoryBarrier ReadCompletion{};
        ReadCompletion.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        ReadCompletion.srcAccessMask =
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        ReadCompletion.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             1,
                             &ReadCompletion,
                             0,
                             nullptr,
                             0,
                             nullptr);

        const auto BindStep = [&](const TPreparedStep& Item)
        {
            vkCmdBindDescriptorSets(CommandBuffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    PipelineLayout,
                                    0,
                                    1,
                                    &Item.DescriptorSet,
                                    0,
                                    nullptr);
            vkCmdPushConstants(CommandBuffer,
                               PipelineLayout,
                               VK_SHADER_STAGE_COMPUTE_BIT,
                               0,
                               sizeof(Item.Constants),
                               &Item.Constants);
        };
        const auto WriteTimestamp = [&](std::uint32_t Offset)
        {
            if (TimestampQueryPool != VK_NULL_HANDLE)
                vkCmdWriteTimestamp(CommandBuffer,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    TimestampQueryPool,
                                    FirstStepQuery + Offset);
        };
        const auto AppendGeometryBarriers = [&](std::vector<VkBufferMemoryBarrier>& Barriers,
                                                const TPreparedStep&                Item)
        {
            if (Item.bUpdateGeometry)
            {
                const auto& Descriptors = *Item.Step->Descriptors;
                const bool  CurrentAB = Item.Step->bCurrentStateAB;
                Barriers.push_back(MakeComputeBufferBarrier(
                    Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::DynamicGeometry, CurrentAB),
                    VK_ACCESS_SHADER_READ_BIT));
                Barriers.push_back(MakeComputeBufferBarrier(Item.HeightBuffer, VK_ACCESS_SHADER_READ_BIT));
            }
        };

        const bool bAnyGeometryUpdate = std::any_of(Prepared.begin(), Prepared.end(), [](const TPreparedStep& Item)
        {
            return Item.bUpdateGeometry;
        });
        WriteTimestamp(0U);
        if (bAnyGeometryUpdate)
        {
            std::vector<VkBufferMemoryBarrier> Barriers;
            Barriers.reserve(Prepared.size());
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, AccumulationHeightPipeline);
            for (const TPreparedStep& Item : Prepared)
            {
                if (!Item.bUpdateGeometry)
                    continue;
                BindStep(Item);
                vkCmdDispatch(CommandBuffer, Item.WorkgroupCount, 1, 1);
                Barriers.push_back(MakeComputeBufferBarrier(Item.HeightBuffer, VK_ACCESS_SHADER_READ_BIT));
            }
            RecordBufferBarriers(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 Barriers);

            Barriers.clear();
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DirtyDispatchPipeline);
            for (const TPreparedStep& Item : Prepared)
            {
                if (!Item.bUpdateGeometry)
                    continue;
                BindStep(Item);
                vkCmdDispatch(CommandBuffer, 1, 1, 1);
                Barriers.push_back(MakeComputeBufferBarrier(Item.HeightBuffer, VK_ACCESS_INDIRECT_COMMAND_READ_BIT));
            }
            RecordBufferBarriers(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 Barriers);

            Barriers.clear();
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, AccumulationGeometryPipeline);
            for (const TPreparedStep& Item : Prepared)
            {
                if (!Item.bUpdateGeometry)
                    continue;
                BindStep(Item);
                vkCmdDispatchIndirect(CommandBuffer, Item.HeightBuffer, Item.IndirectOffset);
                AppendGeometryBarriers(Barriers, Item);
            }
            RecordBufferBarriers(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 Barriers);
            WriteTimestamp(1U);
            WriteTimestamp(2U);

            Barriers.clear();
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DynamicTransferWeightPipeline);
            for (const TPreparedStep& Item : Prepared)
            {
                if (!Item.bUpdateGeometry)
                    continue;
                BindStep(Item);
                vkCmdDispatchIndirect(CommandBuffer, Item.HeightBuffer, Item.IndirectOffset);
                const auto& Descriptors = *Item.Step->Descriptors;
                const bool  CurrentAB = Item.Step->bCurrentStateAB;
                Barriers.push_back(MakeComputeBufferBarrier(
                    Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::TransferWeights, CurrentAB),
                    VK_ACCESS_SHADER_READ_BIT));
                Barriers.push_back(MakeComputeBufferBarrier(
                    Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::DynamicConcavityWeights, CurrentAB),
                    VK_ACCESS_SHADER_READ_BIT));
            }
            RecordBufferBarriers(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 Barriers);
            WriteTimestamp(3U);
        }
        else
        {
            WriteTimestamp(1U);
            WriteTimestamp(2U);
            WriteTimestamp(3U);
        }

        WriteTimestamp(4U);
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass1Pipeline);
        std::vector<VkBufferMemoryBarrier> PassBarriers;
        PassBarriers.reserve(Prepared.size() * 2U);
        for (const TPreparedStep& Item : Prepared)
        {
            BindStep(Item);
            vkCmdDispatch(CommandBuffer, Item.WorkgroupCount, 1, 1);
            const auto& Descriptors = *Item.Step->Descriptors;
            const bool  CurrentAB = Item.Step->bCurrentStateAB;
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::OutgoingFluxScale, CurrentAB),
                VK_ACCESS_SHADER_READ_BIT));
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawOutgoing, CurrentAB),
                VK_ACCESS_SHADER_READ_BIT));
        }
        WriteTimestamp(5U);
        RecordBufferBarriers(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             PassBarriers);

        WriteTimestamp(6U);
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass2Pipeline);
        PassBarriers.clear();
        PassBarriers.reserve(Prepared.size() * 4U);
        for (const TPreparedStep& Item : Prepared)
        {
            BindStep(Item);
            vkCmdDispatch(CommandBuffer, Item.WorkgroupCount, 1, 1);
            const auto& Descriptors = *Item.Step->Descriptors;
            const bool  CurrentAB = Item.Step->bCurrentStateAB;
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::NextState, CurrentAB),
                VK_ACCESS_SHADER_READ_BIT));
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::InputDelta, CurrentAB),
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawOutgoing, CurrentAB),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT));
            PassBarriers.push_back(MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::OutgoingFluxScale, CurrentAB),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT));
        }
        WriteTimestamp(7U);
        RecordBufferBarriers(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                             PassBarriers);
    }

    void TSurfaceStateSolver::RecordCurrentAccumulationHeight(VkCommandBuffer                         CommandBuffer,
                                                              const TSurfaceStateDescriptorResources& Descriptors,
                                                              bool                                    bCurrentStateAB,
                                                              std::size_t                             TexelCount,
                                                              std::size_t                             ChannelCount,
                                                              const glm::mat4&                        ModelMatrix) const
    {
        if (CommandBuffer == VK_NULL_HANDLE || TexelCount == 0 || ChannelCount == 0 ||
            TexelCount > static_cast<std::size_t>(65535U) * 64U ||
            TexelCount > std::numeric_limits<std::uint32_t>::max() ||
            ChannelCount > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Current accumulation height received invalid dimensions.");

        TSurfaceSolverPushConstants Constants{};
        Constants.LocalTexelCount = static_cast<std::uint32_t>(TexelCount);
        Constants.StateChannelCount = static_cast<std::uint32_t>(ChannelCount);
        const glm::mat3 ModelLinear(ModelMatrix);
        const float     Determinant = glm::determinant(ModelLinear);
        glm::mat3       NormalMatrix(0.0F);
        if (std::isfinite(Determinant) && std::abs(Determinant) > 1.0e-6F)
            NormalMatrix = glm::transpose(glm::inverse(ModelLinear));
        for (std::size_t Column = 0; Column < 3; ++Column)
            for (std::size_t Row = 0; Row < 3; ++Row)
                Constants.NormalMatrixAndUpColumns[Column][Row] =
                    NormalMatrix[static_cast<glm::length_t>(Column)][static_cast<glm::length_t>(Row)];

        VkMemoryBarrier Before{};
        Before.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        Before.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        Before.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             1,
                             &Before,
                             0,
                             nullptr,
                             0,
                             nullptr);

        const VkDescriptorSet Set = bCurrentStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet();
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0, 1, &Set, 0, nullptr);
        vkCmdPushConstants(
            CommandBuffer, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Constants), &Constants);
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, AccumulationHeightPipeline);
        vkCmdDispatch(CommandBuffer, static_cast<std::uint32_t>((TexelCount + 63U) / 64U), 1, 1);

        const VkBufferMemoryBarrier HeightBarrier = MakeComputeBufferBarrier(
            Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::AccumulationHeights, bCurrentStateAB),
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &HeightBarrier,
                             0,
                             nullptr);
    }

    VkShaderModule TSurfaceStateSolver::CreateShaderModule(VkDevice Device, const char* Path)
    {
        const std::vector<std::uint32_t> Code = ReadSpirvFile(Path);
        VkShaderModuleCreateInfo         CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = Code.size() * sizeof(std::uint32_t);
        CreateInfo.pCode = Code.data();

        VkShaderModule Module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(Device, &CreateInfo, nullptr, &Module) != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Failed to create compute shader module: ") + Path);
        }
        return Module;
    }

    VkPipeline TSurfaceStateSolver::CreateComputePipeline(VkDevice         Device,
                                                          VkPipelineLayout Layout,
                                                          const char*      ShaderPath)
    {
        const VkShaderModule            Module = CreateShaderModule(Device, ShaderPath);
        VkPipelineShaderStageCreateInfo Stage{};
        Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        Stage.module = Module;
        Stage.pName = "main";
        VkComputePipelineCreateInfo PipelineInfo{};
        PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        PipelineInfo.stage = Stage;
        PipelineInfo.layout = Layout;

        VkPipeline     Pipeline = VK_NULL_HANDLE;
        const VkResult Result = vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline);
        vkDestroyShaderModule(Device, Module, nullptr);
        if (Result != VK_SUCCESS)
        {
            if (Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Pipeline, nullptr);
            throw std::runtime_error(std::string("Failed to create Surface State compute pipeline: ") + ShaderPath);
        }
        return Pipeline;
    }
} // namespace MDSS::SurfaceState
