/**
 * @file RenderStateTexture.cpp
 * @brief Surface마다 한 layer를 가진 RGBA16F State texture와 선택적 precomputed smoothing을 갱신한다.
 */
#include "Rendering/RenderStateTexture.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

        std::vector<std::uint32_t> ReadShader(const char* RelativePath)
        {
            std::ifstream File(std::string(MDSS_SHADER_DIR) + RelativePath, std::ios::binary | std::ios::ate);
            if (!File)
                throw std::runtime_error(std::string("Cannot open rendering State texture shader: ") + RelativePath);
            const auto Size = File.tellg();
            if (Size <= 0 || Size % 4 != 0)
                throw std::runtime_error(std::string("Invalid rendering State texture shader: ") + RelativePath);
            std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / 4);
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Code.data()), Size);
            if (!File)
                throw std::runtime_error(std::string("Cannot read rendering State texture shader: ") + RelativePath);
            return Code;
        }

        VkPipeline CreateComputePipeline(VkDevice Device, VkPipelineLayout Layout, const char* RelativePath)
        {
            const auto Code = ReadShader(RelativePath);
            VkShaderModuleCreateInfo ModuleInfo{};
            ModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            ModuleInfo.codeSize = Code.size() * sizeof(std::uint32_t);
            ModuleInfo.pCode = Code.data();
            VkShaderModule Module = VK_NULL_HANDLE;
            RequireVk(vkCreateShaderModule(Device, &ModuleInfo, nullptr, &Module));

            VkComputePipelineCreateInfo PipelineInfo{};
            PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            PipelineInfo.layout = Layout;
            PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            PipelineInfo.stage.module = Module;
            PipelineInfo.stage.pName = "main";
            VkPipeline Result = VK_NULL_HANDLE;
            const VkResult PipelineResult =
                vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Result);
            vkDestroyShaderModule(Device, Module, nullptr);
            RequireVk(PipelineResult);
            return Result;
        }

        VkImageMemoryBarrier MakeImageBarrier(VkImage Image,
                                              std::uint32_t Layers,
                                              VkImageLayout OldLayout,
                                              VkAccessFlags SrcAccess,
                                              VkAccessFlags DstAccess)
        {
            VkImageMemoryBarrier Barrier{};
            Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            Barrier.srcAccessMask = SrcAccess;
            Barrier.dstAccessMask = DstAccess;
            Barrier.oldLayout = OldLayout;
            Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Barrier.image = Image;
            Barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, Layers};
            return Barrier;
        }
    }

#pragma region Render_State_Texture_Lifecycle

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

            const std::array<VkDescriptorSetLayoutBinding, 3> SmoothBindings{
                {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                 {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                 {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo SmoothLayoutInfo{};
            SmoothLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            SmoothLayoutInfo.bindingCount = static_cast<std::uint32_t>(SmoothBindings.size());
            SmoothLayoutInfo.pBindings = SmoothBindings.data();
            RequireVk(vkCreateDescriptorSetLayout(Device, &SmoothLayoutInfo, nullptr, &SmoothingLayout));

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
                {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, ActiveCount * 5U},
                 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, ActiveCount * 2U},
                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ActiveCount * 2U}}};
            VkDescriptorPoolCreateInfo PoolInfo{};
            PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            PoolInfo.maxSets = ActiveCount * 3U;
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

            const std::array<VkDescriptorSetLayout, 2> SmoothingPipelineLayouts{SurfaceLayout, SmoothingLayout};
            const VkPushConstantRange SmoothPushRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(std::uint32_t)};
            VkPipelineLayoutCreateInfo SmoothPipelineLayoutInfo{};
            SmoothPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            SmoothPipelineLayoutInfo.setLayoutCount = static_cast<std::uint32_t>(SmoothingPipelineLayouts.size());
            SmoothPipelineLayoutInfo.pSetLayouts = SmoothingPipelineLayouts.data();
            SmoothPipelineLayoutInfo.pushConstantRangeCount = 1;
            SmoothPipelineLayoutInfo.pPushConstantRanges = &SmoothPushRange;
            RequireVk(vkCreatePipelineLayout(Device, &SmoothPipelineLayoutInfo, nullptr, &SmoothingPipelineLayout));

            Pipeline = CreateComputePipeline(Device, PipelineLayout, "/Rendering/Surface/RenderStateTexture.comp.spv");
            constexpr std::array<const char*, 5> SmoothVariants{
                "direct", "a1h0", "a1h1", "a2h0", "a2h1"};
            for (std::size_t Variant = 0; Variant < SmoothVariants.size(); ++Variant)
                SmoothingPipelines[Variant] = CreateComputePipeline(
                    Device, SmoothingPipelineLayout,
                    (std::string("/Rendering/Surface/RenderStateTextureSmoothing.comp.") +
                     SmoothVariants[Variant] + ".spv").c_str());

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

                const auto MakeImage = [&](VkImageUsageFlags Usage)
                {
                    return std::make_unique<GPU::TGPUImage>(PhysicalDevice,
                                                            Device,
                                                            Instance.Extent,
                                                            RenderFormat,
                                                            VK_IMAGE_TILING_OPTIMAL,
                                                            Usage,
                                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                            Instance.SurfaceCount);
                };
                const auto MakeView = [&](const GPU::TGPUImage& Image)
                {
                    return std::make_unique<GPU::TGPUImageView>(Device,
                                                                Image.GetHandle(),
                                                                RenderFormat,
                                                                VK_IMAGE_ASPECT_COLOR_BIT,
                                                                Instance.SurfaceCount,
                                                                VK_IMAGE_VIEW_TYPE_2D_ARRAY);
                };
                Instance.Image = MakeImage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
                Instance.View = MakeView(*Instance.Image);
                Instance.SmoothingTempImage = MakeImage(VK_IMAGE_USAGE_STORAGE_BIT);
                Instance.SmoothingTempView = MakeView(*Instance.SmoothingTempImage);
                Instance.SmoothedImage = MakeImage(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
                Instance.SmoothedView = MakeView(*Instance.SmoothedImage);

                const auto AllocateSet = [&](VkDescriptorSetLayout SetLayout, VkDescriptorSet& Set)
                {
                    VkDescriptorSetAllocateInfo AllocateInfo{};
                    AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    AllocateInfo.descriptorPool = Pool;
                    AllocateInfo.descriptorSetCount = 1;
                    AllocateInfo.pSetLayouts = &SetLayout;
                    RequireVk(vkAllocateDescriptorSets(Device, &AllocateInfo, &Set));
                };
                AllocateSet(Layout, Instance.Set);
                AllocateSet(Layout, Instance.SmoothedSet);
                AllocateSet(SmoothingLayout, Instance.SmoothingSet);

                const auto& FlagBuffer = Geometry->GetRenderSamplingBoundaryFlagBuffer();
                const VkDescriptorBufferInfo FlagInfo{FlagBuffer.GetHandle(), 0, FlagBuffer.GetSize()};
                const auto WriteRenderSet = [&](VkDescriptorSet Set, VkImageView StorageView, VkImageView SampleView)
                {
                    const VkDescriptorImageInfo StorageInfo{VK_NULL_HANDLE, StorageView, VK_IMAGE_LAYOUT_GENERAL};
                    const VkDescriptorImageInfo SampleInfo{Sampler, SampleView, VK_IMAGE_LAYOUT_GENERAL};
                    std::array<VkWriteDescriptorSet, 3> Writes{};
                    Writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    Writes[0].dstSet = Set;
                    Writes[0].dstBinding = 0;
                    Writes[0].descriptorCount = 1;
                    Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    Writes[0].pImageInfo = &StorageInfo;
                    Writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    Writes[1].dstSet = Set;
                    Writes[1].dstBinding = 1;
                    Writes[1].descriptorCount = 1;
                    Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Writes[1].pImageInfo = &SampleInfo;
                    Writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    Writes[2].dstSet = Set;
                    Writes[2].dstBinding = 2;
                    Writes[2].descriptorCount = 1;
                    Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    Writes[2].pBufferInfo = &FlagInfo;
                    vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
                };
                WriteRenderSet(Instance.Set, Instance.View->GetHandle(), Instance.View->GetHandle());
                WriteRenderSet(
                    Instance.SmoothedSet, Instance.SmoothedView->GetHandle(), Instance.SmoothedView->GetHandle());

                const std::array<VkDescriptorImageInfo, 3> SmoothInfos{
                    {{VK_NULL_HANDLE, Instance.View->GetHandle(), VK_IMAGE_LAYOUT_GENERAL},
                     {VK_NULL_HANDLE, Instance.SmoothingTempView->GetHandle(), VK_IMAGE_LAYOUT_GENERAL},
                     {VK_NULL_HANDLE, Instance.SmoothedView->GetHandle(), VK_IMAGE_LAYOUT_GENERAL}}};
                std::array<VkWriteDescriptorSet, 3> SmoothWrites{};
                for (std::uint32_t Binding = 0; Binding < SmoothWrites.size(); ++Binding)
                {
                    SmoothWrites[Binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    SmoothWrites[Binding].dstSet = Instance.SmoothingSet;
                    SmoothWrites[Binding].dstBinding = Binding;
                    SmoothWrites[Binding].descriptorCount = 1;
                    SmoothWrites[Binding].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    SmoothWrites[Binding].pImageInfo = &SmoothInfos[Binding];
                }
                vkUpdateDescriptorSets(
                    Device, static_cast<std::uint32_t>(SmoothWrites.size()), SmoothWrites.data(), 0, nullptr);
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
        for (VkPipeline Variant : SmoothingPipelines)
            if (Variant) vkDestroyPipeline(Device, Variant, nullptr);
        if (Pipeline)
            vkDestroyPipeline(Device, Pipeline, nullptr);
        if (SmoothingPipelineLayout)
            vkDestroyPipelineLayout(Device, SmoothingPipelineLayout, nullptr);
        if (PipelineLayout)
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        if (Pool)
            vkDestroyDescriptorPool(Device, Pool, nullptr);
        if (Sampler)
            vkDestroySampler(Device, Sampler, nullptr);
        if (SmoothingLayout)
            vkDestroyDescriptorSetLayout(Device, SmoothingLayout, nullptr);
        if (Layout)
            vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
        SmoothingPipelines.fill(VK_NULL_HANDLE);
        Pipeline = VK_NULL_HANDLE;
        SmoothingPipelineLayout = VK_NULL_HANDLE;
        PipelineLayout = VK_NULL_HANDLE;
        Pool = VK_NULL_HANDLE;
        Sampler = VK_NULL_HANDLE;
        SmoothingLayout = VK_NULL_HANDLE;
        Layout = VK_NULL_HANDLE;
    }

#pragma endregion

#pragma region Descriptor_Access

    VkDescriptorSet TRenderStateTexture::GetSet(std::size_t Instance, bool bSmoothed) const noexcept
    {
        if (Instance >= Instances.size()) return VK_NULL_HANDLE;
        return bSmoothed ? Instances[Instance].SmoothedSet : Instances[Instance].Set;
    }

#pragma endregion

#pragma region Texture_Command_Recording

    void TRenderStateTexture::Record(VkCommandBuffer                                 Command,
                                     const SurfaceState::TSurfaceGPUResourceManager& Resources,
                                     std::array<std::uint32_t, 4>                    Channels,
                                     std::uint32_t                                   ChannelCount,
                                     bool                                            bUpdateStates,
                                     bool                                            bPrecomputeSmoothing,
                                     bool                                            bSeparableSmoothing,
                                     bool                                            bSharedHalo)
    {
        if (!Command || !Pipeline || ChannelCount == 0)
            return;
        const std::array<std::uint32_t, 5> Push{Channels[0], Channels[1], Channels[2], Channels[3], ChannelCount};
        bool bMainPipelineReady = false;
        for (std::size_t Index = 0; Index < Instances.size(); ++Index)
        {
            TInstance& Instance = Instances[Index];
            if (!Instance.Image)
                continue;
            const auto* SurfaceDescriptors = Resources.GetInstanceDescriptors(Index);
            if (!SurfaceDescriptors)
                continue;

            // Even the legacy SSBO A/B pipeline statically declares the render-state sampler.
            // Keep the descriptor image in the declared GENERAL layout without paying the
            // state-conversion dispatch when texture sampling is disabled.
            if (!bUpdateStates)
            {
                if (!Instance.bInitialized)
                {
                    VkImageMemoryBarrier Initialize = MakeImageBarrier(Instance.Image->GetHandle(),
                                                                        Instance.SurfaceCount,
                                                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                                                        0,
                                                                        VK_ACCESS_SHADER_READ_BIT);
                    vkCmdPipelineBarrier(Command,
                                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         0,
                                         0,
                                         nullptr,
                                         0,
                                         nullptr,
                                         1,
                                         &Initialize);
                    Instance.bInitialized = true;
                }
                continue;
            }

            VkImageMemoryBarrier Before = MakeImageBarrier(
                Instance.Image->GetHandle(),
                Instance.SurfaceCount,
                Instance.bInitialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                Instance.bInitialized ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : 0,
                VK_ACCESS_SHADER_WRITE_BIT);
            vkCmdPipelineBarrier(Command,
                                 Instance.bInitialized
                                     ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                     : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &Before);

            const VkDescriptorSet StateSet = Resources.IsCurrentStateAB(Index) ? SurfaceDescriptors->GetABSet()
                                                                                : SurfaceDescriptors->GetBASet();
            // Smoothing uses a different pipeline layout (4-byte push constants vs 20 bytes here).
            // With multiple instances, the previous iteration may have left a smoothing variant bound.
            // Re-establish the main pipeline and its full push-constant payload before the next state dispatch.
            if (!bMainPipelineReady)
            {
                vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
                vkCmdPushConstants(
                    Command, PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), Push.data());
                bMainPipelineReady = true;
            }
            const std::array<VkDescriptorSet, 2> Sets{StateSet, Instance.Set};
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
            Instance.bInitialized = true;

            if (!bPrecomputeSmoothing)
            {
                VkImageMemoryBarrier After = MakeImageBarrier(Instance.Image->GetHandle(),
                                                               Instance.SurfaceCount,
                                                               VK_IMAGE_LAYOUT_GENERAL,
                                                               VK_ACCESS_SHADER_WRITE_BIT,
                                                               VK_ACCESS_SHADER_READ_BIT);
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
                continue;
            }

            std::array<VkImageMemoryBarrier, 3> SmoothBefore{
                MakeImageBarrier(Instance.Image->GetHandle(),
                                 Instance.SurfaceCount,
                                 VK_IMAGE_LAYOUT_GENERAL,
                                 VK_ACCESS_SHADER_WRITE_BIT,
                                 VK_ACCESS_SHADER_READ_BIT),
                MakeImageBarrier(Instance.SmoothingTempImage->GetHandle(),
                                 Instance.SurfaceCount,
                                 Instance.bSmoothingInitialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                                 Instance.bSmoothingInitialized ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : 0,
                                 VK_ACCESS_SHADER_WRITE_BIT),
                MakeImageBarrier(Instance.SmoothedImage->GetHandle(),
                                 Instance.SurfaceCount,
                                 Instance.bSmoothingInitialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
                                 Instance.bSmoothingInitialized ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : 0,
                                 VK_ACCESS_SHADER_WRITE_BIT)};
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(SmoothBefore.size()),
                                 SmoothBefore.data());

            const std::array<VkDescriptorSet, 2> SmoothSets{StateSet, Instance.SmoothingSet};
            bMainPipelineReady = false;
            vkCmdBindDescriptorSets(Command,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    SmoothingPipelineLayout,
                                    0,
                                    static_cast<std::uint32_t>(SmoothSets.size()),
                                    SmoothSets.data(),
                                    0,
                                    nullptr);
            const auto Dispatch = [&]
            {
                vkCmdDispatch(Command,
                              (Instance.Extent.width + 7U) / 8U,
                              (Instance.Extent.height + 7U) / 8U,
                              Instance.SurfaceCount);
            };
            if (bSeparableSmoothing)
            {
                std::uint32_t Mode = 1U;
                vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  SmoothingPipelines[1U + (bSharedHalo ? 1U : 0U)]);
                vkCmdPushConstants(
                    Command, SmoothingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Mode), &Mode);
                Dispatch();
                VkImageMemoryBarrier TempBarrier = MakeImageBarrier(Instance.SmoothingTempImage->GetHandle(),
                                                                     Instance.SurfaceCount,
                                                                     VK_IMAGE_LAYOUT_GENERAL,
                                                                     VK_ACCESS_SHADER_WRITE_BIT,
                                                                     VK_ACCESS_SHADER_READ_BIT);
                vkCmdPipelineBarrier(Command,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     1,
                                     &TempBarrier);
                Mode = 2U;
                vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  SmoothingPipelines[3U + (bSharedHalo ? 1U : 0U)]);
                vkCmdPushConstants(
                    Command, SmoothingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Mode), &Mode);
                Dispatch();
            }
            else
            {
                const std::uint32_t Mode = 0U;
                vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_COMPUTE, SmoothingPipelines[0]);
                vkCmdPushConstants(
                    Command, SmoothingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Mode), &Mode);
                Dispatch();
            }

            VkImageMemoryBarrier AfterSmooth = MakeImageBarrier(Instance.SmoothedImage->GetHandle(),
                                                                 Instance.SurfaceCount,
                                                                 VK_IMAGE_LAYOUT_GENERAL,
                                                                 VK_ACCESS_SHADER_WRITE_BIT,
                                                                 VK_ACCESS_SHADER_READ_BIT);
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &AfterSmooth);
            Instance.bSmoothingInitialized = true;
        }
    }
#pragma endregion
}
