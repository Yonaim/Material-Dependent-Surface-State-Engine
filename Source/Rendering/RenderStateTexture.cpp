/**
 * @file RenderStateTexture.cpp
 * @brief Surface마다 한 layer를 가진 RGBA16F texture를 compute에서 갱신한다.
 */
#include "Rendering/RenderStateTexture.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS::Rendering
{
    namespace
    {
        constexpr VkFormat RenderFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        void RequireVk(VkResult Result)
        {
            if (Result != VK_SUCCESS)
                throw std::runtime_error("Failed to create rendering State texture resources.");
        }

        std::vector<std::uint32_t> ReadShader()
        {
            std::ifstream File(std::string(MDSS_SHADER_DIR) + "/Rendering/Surface/RenderStateTexture.comp.spv",
                               std::ios::binary | std::ios::ate);
            if (!File)
                throw std::runtime_error("Cannot open rendering State texture shader.");
            const auto Size = File.tellg();
            if (Size <= 0 || Size % 4 != 0)
                throw std::runtime_error("Invalid rendering State texture shader.");
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File)
                throw std::runtime_error("Cannot read rendering State texture shader.");
            return Code;
        }
    }

    TRenderStateTexture::TRenderStateTexture(VkPhysicalDevice                                PhysicalDevice,
                                             VkDevice                                        Device,
                                             VkDescriptorSetLayout                           SurfaceLayout,
                                             const SurfaceState::TSurfaceGPUResourceManager& Resources)
        : Device(Device)
    {
        if (!PhysicalDevice || !Device || !SurfaceLayout || Resources.GetManagedInstanceCount() == 0)
            throw std::invalid_argument("Rendering State texture requires Surface instances and a descriptor layout.");

        VkPhysicalDeviceProperties Limits{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &Limits);
        VkFormatProperties FormatProperties{};
        vkGetPhysicalDeviceFormatProperties(PhysicalDevice, RenderFormat, &FormatProperties);
        constexpr VkFormatFeatureFlags RequiredFeatures = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((FormatProperties.optimalTilingFeatures & RequiredFeatures) != RequiredFeatures ||
            Limits.limits.maxComputeWorkGroupInvocations < 64 || Limits.limits.maxComputeWorkGroupSize[0] < 8 ||
            Limits.limits.maxComputeWorkGroupSize[1] < 8)
            throw std::runtime_error("Device cannot write and linearly sample RGBA16F State textures.");

        Instances.resize(Resources.GetSceneInstanceCount());
        try
        {
            const std::array<VkDescriptorSetLayoutBinding, 3> Bindings{
                {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                 {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                 {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo LayoutInfo{};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            LayoutInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
            LayoutInfo.pBindings = Bindings.data();
            RequireVk(vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &Layout));

            VkSamplerCreateInfo SamplerInfo{};
            SamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            SamplerInfo.magFilter = VK_FILTER_LINEAR;
            SamplerInfo.minFilter = VK_FILTER_LINEAR;
            SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            SamplerInfo.maxLod = 0.0F;
            RequireVk(vkCreateSampler(Device, &SamplerInfo, nullptr, &Sampler));

            const auto ActiveCount = static_cast<std::uint32_t>(Resources.GetManagedInstanceCount());
            const std::array<VkDescriptorPoolSize, 3> PoolSizes{
                {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ActiveCount},
                 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ActiveCount},
                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ActiveCount}}};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = ActiveCount;
            PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
            PoolInfo.pPoolSizes = PoolSizes.data();
            RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool));

            const std::array<VkDescriptorSetLayout, 2> PipelineLayouts{SurfaceLayout, Layout};
            const VkPushConstantRange                  PushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, 20};
            VkPipelineLayoutCreateInfo                 PipelineLayoutInfo{};
            PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            PipelineLayoutInfo.setLayoutCount = static_cast<std::uint32_t>(PipelineLayouts.size());
            PipelineLayoutInfo.pSetLayouts = PipelineLayouts.data();
            PipelineLayoutInfo.pushConstantRangeCount = 1;
            PipelineLayoutInfo.pPushConstantRanges = &PushRange;
            RequireVk(vkCreatePipelineLayout(Device, &PipelineLayoutInfo, nullptr, &PipelineLayout));

            const auto               Code = ReadShader();
            VkShaderModuleCreateInfo ModuleInfo{};
            ModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ModuleInfo.codeSize = Code.size() * sizeof(std::uint32_t);
            ModuleInfo.pCode = Code.data();
            VkShaderModule Module = VK_NULL_HANDLE;
            RequireVk(vkCreateShaderModule(Device, &ModuleInfo, nullptr, &Module));
            VkComputePipelineCreateInfo PipelineInfo{};
            PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            PipelineInfo.layout = PipelineLayout;
            PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            PipelineInfo.stage.module = Module;
            PipelineInfo.stage.pName = "main";
            const VkResult PipelineResult =
                vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline);
            vkDestroyShaderModule(Device, Module, nullptr);
            RequireVk(PipelineResult);

            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
            {
                if (!Resources.GetInstanceDescriptors(Index))
                    continue;
                const auto* Geometry = Resources.GetInstanceSharedGeometry(Index);
                const auto& Ranges = Geometry->GetSurfaceRanges();
                TInstance&  Instance = Instances[Index];
                if (Ranges.empty() || Ranges.size() > Limits.limits.maxImageArrayLayers ||
                    Ranges.size() > Limits.limits.maxComputeWorkGroupCount[2])
                    throw std::runtime_error("Surface count exceeds State texture array or dispatch limits.");
                Instance.SurfaceCount = static_cast<std::uint32_t>(Ranges.size());
                for (const auto& Range : Ranges)
                {
                    Instance.Extent.width = std::max(Instance.Extent.width, Range.Width);
                    Instance.Extent.height = std::max(Instance.Extent.height, Range.Height);
                }
                if (Instance.Extent.width > Limits.limits.maxImageDimension2D ||
                    Instance.Extent.height > Limits.limits.maxImageDimension2D ||
                    (Instance.Extent.width + 7U) / 8U > Limits.limits.maxComputeWorkGroupCount[0] ||
                    (Instance.Extent.height + 7U) / 8U > Limits.limits.maxComputeWorkGroupCount[1])
                    throw std::runtime_error("Surface resolution exceeds State texture or dispatch limits.");

                Instance.Image =
                    std::make_unique<GPU::TGPUImage>(PhysicalDevice,
                                                     Device,
                                                     Instance.Extent,
                                                     RenderFormat,
                                                     VK_IMAGE_TILING_OPTIMAL,
                                                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                     Instance.SurfaceCount);
                Instance.View = std::make_unique<GPU::TGPUImageView>(Device,
                                                                     Instance.Image->GetHandle(),
                                                                     RenderFormat,
                                                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                                                     Instance.SurfaceCount,
                                                                     VK_IMAGE_VIEW_TYPE_2D_ARRAY);
                VkDescriptorSetAllocateInfo AllocateInfo{};
                AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                AllocateInfo.descriptorPool = Pool;
                AllocateInfo.descriptorSetCount = 1;
                AllocateInfo.pSetLayouts = &Layout;
                RequireVk(vkAllocateDescriptorSets(Device, &AllocateInfo, &Instance.Set));

                const VkDescriptorImageInfo StorageInfo{
                    VK_NULL_HANDLE, Instance.View->GetHandle(), VK_IMAGE_LAYOUT_GENERAL};
                const VkDescriptorImageInfo SampleInfo{Sampler, Instance.View->GetHandle(), VK_IMAGE_LAYOUT_GENERAL};
                const auto& FlagBuffer = Geometry->GetRenderSamplingBoundaryFlagBuffer();
                const VkDescriptorBufferInfo FlagInfo{FlagBuffer.GetHandle(), 0, FlagBuffer.GetSize()};
                std::array<VkWriteDescriptorSet, 3> Writes{};
                Writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Writes[0].dstSet = Instance.Set;
                Writes[0].dstBinding = 0;
                Writes[0].descriptorCount = 1;
                Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                Writes[0].pImageInfo = &StorageInfo;
                Writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Writes[1].dstSet = Instance.Set;
                Writes[1].dstBinding = 1;
                Writes[1].descriptorCount = 1;
                Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                Writes[1].pImageInfo = &SampleInfo;
                Writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Writes[2].dstSet = Instance.Set;
                Writes[2].dstBinding = 2;
                Writes[2].descriptorCount = 1;
                Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Writes[2].pBufferInfo = &FlagInfo;
                vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
            }
        }
        catch (...)
        {
            Destroy();
            throw;
        }
    }

    TRenderStateTexture::~TRenderStateTexture()
    {
        Destroy();
    }

    void TRenderStateTexture::Destroy() noexcept
    {
        if (Pipeline)
            vkDestroyPipeline(Device, Pipeline, nullptr);
        if (PipelineLayout)
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        if (Sampler)
            vkDestroySampler(Device, Sampler, nullptr);
        if (Layout)
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
        Pipeline = VK_NULL_HANDLE;
        PipelineLayout = VK_NULL_HANDLE;
        Pool = VK_NULL_HANDLE;
        Sampler = VK_NULL_HANDLE;
        Layout = VK_NULL_HANDLE;
    }

    VkDescriptorSet TRenderStateTexture::GetSet(std::size_t Instance) const noexcept
    {
        return Instance < Instances.size() ? Instances[Instance].Set : VK_NULL_HANDLE;
    }

    void TRenderStateTexture::Record(VkCommandBuffer                                 Command,
                                     const SurfaceState::TSurfaceGPUResourceManager& Resources,
                                     std::array<std::uint32_t, 4>                    Channels,
                                     std::uint32_t                                   ChannelCount)
    {
        if (!Command || !Pipeline || ChannelCount == 0)
            return;
        const std::array<std::uint32_t, 5> Push{Channels[0], Channels[1], Channels[2], Channels[3], ChannelCount};
        vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
        vkCmdPushConstants(Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), Push.data());
        for (std::size_t Index = 0; Index < Instances.size(); ++Index)
        {
            TInstance& Instance = Instances[Index];
            if (!Instance.Image)
                continue;
            const auto* SurfaceDescriptors = Resources.GetInstanceDescriptors(Index);
            if (!SurfaceDescriptors)
                continue;

            VkImageMemoryBarrier Before{};
            Before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            Before.srcAccessMask = Instance.bInitialized ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : 0;
            Before.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            Before.oldLayout = Instance.bInitialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
            Before.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            Before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Before.image = Instance.Image->GetHandle();
            Before.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, Instance.SurfaceCount};
            vkCmdPipelineBarrier(Command,
                                 Instance.bInitialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                       : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &Before);

            const std::array<VkDescriptorSet, 2> Sets{
                Resources.IsCurrentStateAB(Index) ? SurfaceDescriptors->GetABSet() : SurfaceDescriptors->GetBASet(),
                Instance.Set};
            vkCmdBindDescriptorSets(Command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    PipelineLayout,
                                    0,
                                    static_cast<std::uint32_t>(Sets.size()),
                                    Sets.data(),
                                    0,
                                    nullptr);
            vkCmdDispatch(
                Command, (Instance.Extent.width + 7U) / 8U, (Instance.Extent.height + 7U) / 8U, Instance.SurfaceCount);

            VkImageMemoryBarrier After = Before;
            After.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            After.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            After.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &After);
            Instance.bInitialized = true;
        }
    }
}
