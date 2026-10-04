/**
 * @file SurfaceStateSolver.cpp
 * @brief 선택적 동적 형상 갱신과 2-pass GPU Surface State solver를 기록한다.
 */

#include "SurfaceState/State/SurfaceStateSolver.h"

#include "SurfaceState/Types/SurfaceSolverRates.h"

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
                Device, PipelineLayout, (ShaderRoot + "/Simulation/SurfaceAccumulationHeight.comp.spv").c_str(), false);
            DirtyDispatchPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicUpdateDispatch.comp.spv").c_str(),
                false);
            AccumulationGeometryPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicGeometryUpdate.comp.spv").c_str(),
                false);
            DynamicTransferWeightPipeline = CreateComputePipeline(
                Device,
                PipelineLayout,
                (ShaderRoot + "/Simulation/SurfaceDynamicWeightsUpdate.comp.spv").c_str(),
                false);
            // Specialize both modes so the cached shader does not retain the recomputation path.
            for (std::size_t Mode = 0; Mode < Pass1Pipelines.size(); ++Mode)
            {
                Pass1Pipelines[Mode] = CreateComputePipeline(
                    Device,
                    PipelineLayout,
                    (ShaderRoot + "/Simulation/SurfaceSolver/SurfaceSolverPass1.comp.spv").c_str(),
                    Mode == 0);
                Pass2Pipelines[Mode] = CreateComputePipeline(
                    Device,
                    PipelineLayout,
                    (ShaderRoot + "/Simulation/SurfaceSolver/SurfaceSolverPass2.comp.spv").c_str(),
                    Mode == 0);
            }
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
            for (const auto Pipeline : Pass1Pipelines)
                if (Pipeline != VK_NULL_HANDLE)
                    vkDestroyPipeline(Device, Pipeline, nullptr);
            for (const auto Pipeline : Pass2Pipelines)
                if (Pipeline != VK_NULL_HANDLE)
                    vkDestroyPipeline(Device, Pipeline, nullptr);
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
        for (const auto Pipeline : Pass1Pipelines)
            if (Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Pipeline, nullptr);
        for (const auto Pipeline : Pass2Pipelines)
            if (Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Pipeline, nullptr);
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
        if (CommandBuffer == VK_NULL_HANDLE || TexelCount == 0 || ChannelCount == 0 ||
            TexelCount > std::numeric_limits<std::uint32_t>::max() ||
            ChannelCount > std::numeric_limits<std::uint32_t>::max() || !std::isfinite(DeltaTime) || DeltaTime < 0.0F)
        {
            throw std::invalid_argument("Surface State solver step received invalid dimensions or DeltaTime.");
        }
        if (TexelCount > static_cast<std::size_t>(65535U) * 64U)
        {
            throw std::length_error("Surface State solver dispatch exceeds Vulkan's minimum X workgroup limit.");
        }

        const VkDescriptorSet       DescriptorSet = bCurrentStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet();
        TSurfaceSolverPushConstants Constants{};
        Constants.DeltaTime = DeltaTime;
        Constants.StateChannelCount = static_cast<std::uint32_t>(ChannelCount);
        Constants.LocalTexelCount = static_cast<std::uint32_t>(TexelCount);
        Constants.Flags = SolverFlags;
        Constants.GravityWorld = {GravityWorld.x, GravityWorld.y, GravityWorld.z, 0.0F};
        // All texels in this dispatch share these values. Stay within Vulkan's minimum 128-byte budget.
        constexpr float GeometryEpsilon = 1.0e-6F;
        const glm::mat3 ModelLinear(ModelMatrix);
        glm::mat3       NormalMatrix(0.0F);
        glm::vec3       Up(0.0F);
        const float     Determinant = glm::determinant(ModelLinear);
        if (std::isfinite(Determinant) && std::abs(Determinant) > GeometryEpsilon)
            NormalMatrix = glm::transpose(glm::inverse(ModelLinear));
        const float GravityLength = glm::length(GravityWorld);
        if ((SolverFlags & 1U) == 0U && std::isfinite(GravityLength) && GravityLength > GeometryEpsilon)
        {
            Up = -GravityWorld / GravityLength;
        }
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

        // Previous frames can still read either ping-pong buffer in debug vertex/fragment shaders.
        // Order those reads before this step reuses the buffers as compute outputs.
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

        const std::size_t   CacheMode = (SolverFlags & SurfaceSolverDisableRawFluxCacheFlag) == 0U ? 0U : 1U;
        const std::uint32_t WorkgroupCount = static_cast<std::uint32_t>((TexelCount + 63U) / 64U);
        vkCmdBindDescriptorSets(
            CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0, 1, &DescriptorSet, 0, nullptr);
        vkCmdPushConstants(
            CommandBuffer, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Constants), &Constants);

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery);
        }
        if ((SolverFlags & SurfaceSolverAccumulationGeometryUpdateFlag) != 0U)
        {
            const VkBuffer HeightBuffer =
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::AccumulationHeights, bCurrentStateAB);
            const VkDeviceSize IndirectOffset =
                static_cast<VkDeviceSize>((3U * TexelCount + WorkgroupCount) * sizeof(float));
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, AccumulationHeightPipeline);
            vkCmdDispatch(CommandBuffer, WorkgroupCount, 1, 1);
            const VkBufferMemoryBarrier HeightBarrier =
                MakeComputeBufferBarrier(HeightBuffer, VK_ACCESS_SHADER_READ_BIT);
            vkCmdPipelineBarrier(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &HeightBarrier,
                                 0,
                                 nullptr);

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DirtyDispatchPipeline);
            vkCmdDispatch(CommandBuffer, 1, 1, 1);
            const VkBufferMemoryBarrier IndirectBarrier =
                MakeComputeBufferBarrier(HeightBuffer, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
            vkCmdPipelineBarrier(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &IndirectBarrier,
                                 0,
                                 nullptr);

            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, AccumulationGeometryPipeline);
            vkCmdDispatchIndirect(CommandBuffer, HeightBuffer, IndirectOffset);
            const std::array<VkBufferMemoryBarrier, 2> GeometryBarriers = {
                MakeComputeBufferBarrier(
                    Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::DynamicGeometry, bCurrentStateAB),
                    VK_ACCESS_SHADER_READ_BIT),
                MakeComputeBufferBarrier(Descriptors.GetBoundBufferHandle(
                                             TSurfaceGPUDescriptorBinding::AccumulationHeights, bCurrentStateAB),
                                         VK_ACCESS_SHADER_READ_BIT)};
            vkCmdPipelineBarrier(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(GeometryBarriers.size()),
                                 GeometryBarriers.data(),
                                 0,
                                 nullptr);

            if (TimestampQueryPool != VK_NULL_HANDLE)
            {
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 1U);
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 2U);
            }
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DynamicTransferWeightPipeline);
            vkCmdDispatchIndirect(CommandBuffer, HeightBuffer, IndirectOffset);
            const std::array<VkBufferMemoryBarrier, 2> GeometryUpdateBarriers = {
                MakeComputeBufferBarrier(
                    Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::TransferWeights, bCurrentStateAB),
                    VK_ACCESS_SHADER_READ_BIT),
                MakeComputeBufferBarrier(Descriptors.GetBoundBufferHandle(
                                             TSurfaceGPUDescriptorBinding::DynamicConcavityWeights, bCurrentStateAB),
                                         VK_ACCESS_SHADER_READ_BIT)};
            vkCmdPipelineBarrier(CommandBuffer,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(GeometryUpdateBarriers.size()),
                                 GeometryUpdateBarriers.data(),
                                 0,
                                 nullptr);
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            if ((SolverFlags & SurfaceSolverAccumulationGeometryUpdateFlag) == 0U)
            {
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 1U);
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 2U);
            }
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 3U);
        }

        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass1Pipelines[CacheMode]);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 4U);
        }
        vkCmdDispatch(CommandBuffer, WorkgroupCount, 1, 1);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 5U);
        }

        const std::array<VkBufferMemoryBarrier, 3> Pass1Barriers = {
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::OutgoingFluxScale, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawOutgoing, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawFlux, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT)};
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             (SolverFlags & SurfaceSolverDisableRawFluxCacheFlag) == 0U ? 3U : 2U,
                             Pass1Barriers.data(),
                             0,
                             nullptr);

        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass2Pipelines[CacheMode]);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 6U);
        }
        vkCmdDispatch(CommandBuffer, WorkgroupCount, 1, 1);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, TimestampQueryPool, FirstPassQuery + 7U);
        }

        const std::array<VkBufferMemoryBarrier, 5> NextStepBarriers = {
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::NextState, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::InputDelta, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawOutgoing, bCurrentStateAB),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::OutgoingFluxScale, bCurrentStateAB),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::RawFlux, bCurrentStateAB),
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT)};
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             (SolverFlags & SurfaceSolverDisableRawFluxCacheFlag) == 0U ? 5U : 4U,
                             NextStepBarriers.data(),
                             0,
                             nullptr);
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
                                                          const char*      ShaderPath,
                                                          bool             bRawFluxCacheEnabled)
    {
        const VkShaderModule            Module = CreateShaderModule(Device, ShaderPath);
        VkPipelineShaderStageCreateInfo Stage{};
        Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        Stage.module = Module;
        Stage.pName = "main";
        const VkBool32                 CacheEnabled = bRawFluxCacheEnabled ? VK_TRUE : VK_FALSE;
        const VkSpecializationMapEntry CacheEntry{0U, 0U, sizeof(CacheEnabled)};
        const VkSpecializationInfo     Specialization{1U, &CacheEntry, sizeof(CacheEnabled), &CacheEnabled};
        Stage.pSpecializationInfo = &Specialization;

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
