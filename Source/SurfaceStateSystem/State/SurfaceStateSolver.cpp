/**
 * @file SurfaceStateSolver.cpp
 * @brief Two-pass GPU Surface State update and its storage-buffer dependencies.
 */

#include "SurfaceStateSystem/State/SurfaceStateSolver.h"

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

namespace MDSS
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

        VkBufferMemoryBarrier MakeComputeBufferBarrier(VkBuffer Buffer, VkAccessFlags DestinationAccess)
        {
            VkBufferMemoryBarrier Barrier{};
            Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
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
            Pass1Pipeline = CreateComputePipeline(
                Device, PipelineLayout, (ShaderRoot + "/SurfaceSolverPass1.comp.spv").c_str());
            Pass2Pipeline = CreateComputePipeline(
                Device, PipelineLayout, (ShaderRoot + "/SurfaceSolverPass2.comp.spv").c_str());
        }
        catch (...)
        {
            if (Pass1Pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(Device, Pass1Pipeline, nullptr);
                Pass1Pipeline = VK_NULL_HANDLE;
            }
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            PipelineLayout = VK_NULL_HANDLE;
            throw;
        }
    }

    TSurfaceStateSolver::~TSurfaceStateSolver()
    {
        if (Pass2Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(Device, Pass2Pipeline, nullptr);
        }
        if (Pass1Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(Device, Pass1Pipeline, nullptr);
        }
        if (PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        }
    }

    void TSurfaceStateSolver::RecordStep(VkCommandBuffer CommandBuffer,
                                         const TSurfaceStateDescriptorResources& Descriptors,
                                         bool bCurrentStateAB,
                                         std::size_t TexelCount,
                                         std::size_t ChannelCount,
                                         float DeltaTime) const
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

        const VkDescriptorSet DescriptorSet = bCurrentStateAB ? Descriptors.GetABSet() : Descriptors.GetBASet();
        TSurfaceSolverPushConstants Constants{};
        Constants.DeltaTime = DeltaTime;
        Constants.StateChannelCount = static_cast<std::uint32_t>(ChannelCount);
        Constants.LocalTexelCount = static_cast<std::uint32_t>(TexelCount);

        const std::uint32_t WorkgroupCount = static_cast<std::uint32_t>((TexelCount + 63U) / 64U);
        vkCmdBindDescriptorSets(CommandBuffer,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                PipelineLayout,
                                0,
                                1,
                                &DescriptorSet,
                                0,
                                nullptr);
        vkCmdPushConstants(CommandBuffer,
                           PipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(Constants),
                           &Constants);

        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass1Pipeline);
        vkCmdDispatch(CommandBuffer, WorkgroupCount, 1, 1);

        VkBufferMemoryBarrier AlphaBarrier = MakeComputeBufferBarrier(
            Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::OutgoingFluxScale, bCurrentStateAB),
            VK_ACCESS_SHADER_READ_BIT);
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &AlphaBarrier,
                             0,
                             nullptr);

        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Pass2Pipeline);
        vkCmdDispatch(CommandBuffer, WorkgroupCount, 1, 1);

        const std::array<VkBufferMemoryBarrier, 2> NextStepBarriers = {
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::NextState, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT),
            MakeComputeBufferBarrier(
                Descriptors.GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::InputDelta, bCurrentStateAB),
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)};
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             static_cast<std::uint32_t>(NextStepBarriers.size()),
                             NextStepBarriers.data(),
                             0,
                             nullptr);
    }

    VkShaderModule TSurfaceStateSolver::CreateShaderModule(VkDevice Device, const char* Path)
    {
        const std::vector<std::uint32_t> Code = ReadSpirvFile(Path);
        VkShaderModuleCreateInfo CreateInfo{};
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

    VkPipeline TSurfaceStateSolver::CreateComputePipeline(VkDevice Device,
                                                           VkPipelineLayout Layout,
                                                           const char* ShaderPath)
    {
        const VkShaderModule Module = CreateShaderModule(Device, ShaderPath);
        VkPipelineShaderStageCreateInfo Stage{};
        Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        Stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        Stage.module = Module;
        Stage.pName = "main";

        VkComputePipelineCreateInfo PipelineInfo{};
        PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        PipelineInfo.stage = Stage;
        PipelineInfo.layout = Layout;

        VkPipeline Pipeline = VK_NULL_HANDLE;
        const VkResult Result = vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline);
        vkDestroyShaderModule(Device, Module, nullptr);
        if (Result != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Failed to create Surface State compute pipeline: ") + ShaderPath);
        }
        return Pipeline;
    }
} // namespace MDSS
