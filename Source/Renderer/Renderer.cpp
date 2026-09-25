/**
 * @file Renderer.cpp
 * @brief swapchain 기반 장면 렌더링과 재생성 흐름.
 */

#include "Renderer/Renderer.h"

#include "Application/Window.h"
#include "AssetManager/AssetManager.h"
#include "AssetManager/MaterialAsset.h"
#include "AssetManager/MeshAsset.h"
#include "AssetManager/TextureAsset.h"
#include "DebugUI/DebugUI.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/VulkanContext.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <stdexcept>
#include <string>

#ifndef MDSS_SHADER_DIR
#define MDSS_SHADER_DIR "Shaders"
#endif

namespace MDSS
{
    namespace
    {
        struct StaticMeshPushConstants
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };

        struct alignas(16) MaterialUniform
        {
            glm::vec4     BaseColor{1.0F};
            std::uint32_t RenderMode = 0;
            std::uint32_t FlipNormalY = 1;
            float         NormalStrength = 1.0F;
            float         AmbientLight = 0.25F;
        };

        const char* GetRenderViewModeName(RenderViewMode Mode)
        {
            switch (Mode)
            {
                case RenderViewMode::Lit:
                    return "Lit";
                case RenderViewMode::Unlit:
                    return "Unlit";
                case RenderViewMode::VertexNormalWS:
                    return "Vertex Normal (World Space)";
                case RenderViewMode::NormalTextureTS:
                    return "Normal Texture (Tangent Space)";
                case RenderViewMode::MappedNormalWS:
                    return "Mapped Normal (World Space)";
            }
            return "Unknown";
        }

        static_assert(sizeof(StaticMeshPushConstants) == 128,
                      "Static mesh push constants are expected to use Vulkan's guaranteed 128-byte minimum.");
        static_assert(sizeof(MaterialUniform) == 32, "MaterialUniform must match the std140 shader block layout.");

        GraphicsPipelineConfig BuildStaticMeshPipelineConfig(VkDescriptorSetLayout MaterialLayout)
        {
            GraphicsPipelineConfig Config{};
            Config.ShaderStages = {
                {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/StaticMesh.vert.spv", "main"},
                {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/StaticMesh.frag.spv", "main"},
            };
            Config.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            Config.CullMode = VK_CULL_MODE_BACK_BIT;
            Config.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            Config.bDepthTestEnabled = true;
            Config.bDepthWriteEnabled = true;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS;
            Config.bBlendingEnabled = false;
            Config.DescriptorSetLayouts.push_back(MaterialLayout);

            VkVertexInputBindingDescription Binding{};
            Binding.binding = 0;
            Binding.stride = sizeof(Vertex);
            Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            Config.VertexBindings.push_back(Binding);

            auto AddAttribute = [&](std::uint32_t Location, VkFormat Format, std::uint32_t Offset)
            {
                VkVertexInputAttributeDescription Attribute{};
                Attribute.location = Location;
                Attribute.binding = 0;
                Attribute.format = Format;
                Attribute.offset = Offset;
                Config.VertexAttributes.push_back(Attribute);
            };

            AddAttribute(0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, Position)));
            AddAttribute(1, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, Normal)));
            AddAttribute(2, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, UV)));
            AddAttribute(3, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<std::uint32_t>(offsetof(Vertex, Tangent)));

            VkPushConstantRange PushConstantRange{};
            PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            PushConstantRange.offset = 0;
            PushConstantRange.size = sizeof(StaticMeshPushConstants);
            Config.PushConstantRanges.push_back(PushConstantRange);

            return Config;
        }
    } // namespace

    Renderer::Renderer(const VulkanContext& Context, Window& Window, const AssetManager& Assets)
        : Context(Context), TargetWindow(Window), Assets(Assets), SwapchainData(Context, Window),
          DepthFormat(FindDepthFormat(Context.GetPhysicalDevice())),
          DepthImage(Context.GetPhysicalDevice(),
                     Context.GetDevice(),
                     SwapchainData.GetExtent(),
                     DepthFormat,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          DepthImageView(Context.GetDevice(), DepthImage.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT),
          MainRenderPass(Context.GetDevice(), SwapchainData.GetImageFormat(), DepthFormat),
          MaterialDescriptorSetLayout(CreateMaterialDescriptorSetLayout(Context.GetDevice())),
          StaticMeshPipeline(Context.GetDevice(),
                             MainRenderPass.GetHandle(),
                             BuildStaticMeshPipelineConfig(MaterialDescriptorSetLayout)),
          MainFramebuffers(Context.GetDevice(),
                           MainRenderPass.GetHandle(),
                           SwapchainData.GetExtent(),
                           SwapchainData.GetImageViews(),
                           DepthImageView.GetHandle()),
          FrameContext(Context)
    {
        CreateMaterialDescriptorResources();
        Logger::Info("Renderer", "Static mesh pipeline ready with MTL base-color and tangent-space normal mapping.");
        Logger::Debug("Renderer",
                      "Depth format=" + std::to_string(static_cast<int>(DepthFormat)) +
                          ", material descriptor count=" + std::to_string(MaterialResources.size()) + ".");
    }

    Renderer::~Renderer()
    {
        if (Context.GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Context.GetDevice());
        }

        if (MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), MaterialDescriptorPool, nullptr);
            MaterialDescriptorPool = VK_NULL_HANDLE;
        }
        MaterialResources.clear();
        if (MaterialDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(Context.GetDevice(), MaterialDescriptorSetLayout, nullptr);
            MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    void Renderer::RenderFrame(const Scene& SceneData, DebugUI& DebugInterface)
    {
        FrameContext.WaitForCurrentFrame();

        if (TargetWindow.WasFramebufferResized())
        {
            RecreateSwapchain(DebugInterface);
            return;
        }

        std::uint32_t  ImageIndex = 0;
        const VkResult AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);

        if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Logger::Debug("Renderer", "Swapchain became out of date while acquiring; recreating it.");
            RecreateSwapchain(DebugInterface);
            return;
        }
        if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire a Vulkan swapchain image.");
        }

        FrameContext.ResetCurrentFence();

        const VkCommandBuffer CommandBuffer = FrameContext.GetCurrentCommandBuffer();
        if (vkResetCommandBuffer(CommandBuffer, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset Vulkan command buffer.");
        }

        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData, DebugInterface);

        const VkSemaphore          WaitSemaphore = FrameContext.GetImageAvailableSemaphore();
        const VkSemaphore          SignalSemaphore = FrameContext.GetRenderFinishedSemaphore();
        const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = &WaitSemaphore;
        SubmitInfo.pWaitDstStageMask = &WaitStage;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = &SignalSemaphore;

        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }

        const VkSwapchainKHR SwapchainHandle = SwapchainData.GetHandle();
        VkPresentInfoKHR     PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = &SignalSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &SwapchainHandle;
        PresentInfo.pImageIndices = &ImageIndex;

        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
        const bool     bSwapchainNeedsRecreation =
            AcquireResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
            PresentResult == VK_SUBOPTIMAL_KHR || TargetWindow.WasFramebufferResized();

        if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR &&
            PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present Vulkan swapchain image.");
        }

        FrameContext.AdvanceFrame();

        if (bSwapchainNeedsRecreation)
        {
            Logger::Debug("Renderer", "Presentation requires swapchain recreation.");
            RecreateSwapchain(DebugInterface);
        }
    }

    void Renderer::RecreateSwapchain(DebugUI& DebugInterface)
    {
        TargetWindow.WaitForNonZeroFramebuffer();
        if (TargetWindow.ShouldClose())
        {
            return;
        }

        std::uint32_t FramebufferWidth = 0;
        std::uint32_t FramebufferHeight = 0;
        TargetWindow.GetFramebufferSize(FramebufferWidth, FramebufferHeight);

        Logger::Info("Renderer",
                     "Recreating swapchain for framebuffer " + std::to_string(FramebufferWidth) + "x" +
                         std::to_string(FramebufferHeight) + ".");

        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for Vulkan device before swapchain recreation.");
        }

        // Framebuffers reference both swapchain image views and the depth image view,
        // so they must be destroyed before either dependency is recreated.
        MainFramebuffers.Reset();
        DepthImageView.Reset();
        DepthImage.Reset();

        const VkFormat PreviousColorFormat = SwapchainData.GetImageFormat();
        SwapchainData.Recreate(Context, TargetWindow);

        if (PreviousColorFormat != SwapchainData.GetImageFormat())
        {
            throw std::runtime_error(
                "Swapchain color format changed during resize. RenderPass/Pipeline recreation is required.");
        }

        DepthImage.Recreate(Context.GetPhysicalDevice(),
                            SwapchainData.GetExtent(),
                            DepthFormat,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        DepthImageView.Recreate(DepthImage.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        MainFramebuffers.Recreate(MainRenderPass.GetHandle(),
                                  SwapchainData.GetExtent(),
                                  SwapchainData.GetImageViews(),
                                  DepthImageView.GetHandle());

        TargetWindow.ResetFramebufferResized();
        DebugInterface.OnSwapchainRecreated(Context, *this);

        const VkExtent2D NewExtent = SwapchainData.GetExtent();
        Logger::Info("Renderer",
                     "Swapchain recreation complete: " + std::to_string(NewExtent.width) + "x" +
                         std::to_string(NewExtent.height) + ".");
    }

    const Swapchain& Renderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
    }

    VkRenderPass Renderer::GetRenderPassHandle() const noexcept
    {
        return MainRenderPass.GetHandle();
    }

    RenderViewMode Renderer::GetRenderViewMode() const noexcept
    {
        return ViewMode;
    }

    void Renderer::SetRenderViewMode(RenderViewMode Mode)
    {
        if (ViewMode == Mode)
        {
            return;
        }

        ViewMode = Mode;
        UpdateMaterialUniforms();
        Logger::Info("Renderer", std::string("Render view mode changed to ") + GetRenderViewModeName(ViewMode) + ".");
    }

    bool Renderer::GetFlipNormalY() const noexcept
    {
        return bFlipNormalY;
    }

    void Renderer::SetFlipNormalY(bool bEnabled)
    {
        if (bFlipNormalY == bEnabled)
        {
            return;
        }

        bFlipNormalY = bEnabled;
        UpdateMaterialUniforms();
        Logger::Info("Renderer", std::string("Normal-map Y flip ") + (bFlipNormalY ? "enabled." : "disabled."));
    }

    float Renderer::GetNormalStrength() const noexcept
    {
        return NormalStrength;
    }

    void Renderer::SetNormalStrength(float Strength)
    {
        NormalStrength = std::clamp(Strength, 0.0F, 4.0F);
        UpdateMaterialUniforms();
    }

    float Renderer::GetAmbientLight() const noexcept
    {
        return AmbientLight;
    }

    void Renderer::SetAmbientLight(float Intensity)
    {
        AmbientLight = std::clamp(Intensity, 0.0F, 1.0F);
        UpdateMaterialUniforms();
    }

    VkDescriptorSetLayout Renderer::CreateMaterialDescriptorSetLayout(VkDevice Device)
    {
        std::array<VkDescriptorSetLayoutBinding, 3> Bindings{};

        Bindings[0].binding = 0;
        Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Bindings[0].descriptorCount = 1;
        Bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        Bindings[1].binding = 1;
        Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        Bindings[1].descriptorCount = 1;
        Bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        Bindings[2].binding = 2;
        Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        Bindings[2].descriptorCount = 1;
        Bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo Info{};
        Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        Info.bindingCount = static_cast<std::uint32_t>(Bindings.size());
        Info.pBindings = Bindings.data();

        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(Device, &Info, nullptr, &Layout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create static mesh material descriptor set layout.");
        }
        return Layout;
    }

    void Renderer::CreateMaterialDescriptorResources()
    {
        const std::size_t MaterialCount = Assets.GetMaterialCount();
        if (MaterialCount == 0)
        {
            Logger::Warning("Renderer", "No materials are registered; material descriptor resources were not created.");
            return;
        }

        Logger::Debug("Renderer",
                      "Creating descriptor resources for " + std::to_string(MaterialCount) + " material(s).");

        std::array<VkDescriptorPoolSize, 2> PoolSizes{};
        PoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        PoolSizes[0].descriptorCount = static_cast<std::uint32_t>(MaterialCount * 2U);
        PoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        PoolSizes[1].descriptorCount = static_cast<std::uint32_t>(MaterialCount);

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = static_cast<std::uint32_t>(MaterialCount);

        if (vkCreateDescriptorPool(Context.GetDevice(), &PoolInfo, nullptr, &MaterialDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create material descriptor pool.");
        }

        std::vector<VkDescriptorSetLayout> Layouts(MaterialCount, MaterialDescriptorSetLayout);
        std::vector<VkDescriptorSet>       Sets(MaterialCount, VK_NULL_HANDLE);

        VkDescriptorSetAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocateInfo.descriptorPool = MaterialDescriptorPool;
        AllocateInfo.descriptorSetCount = static_cast<std::uint32_t>(MaterialCount);
        AllocateInfo.pSetLayouts = Layouts.data();

        if (vkAllocateDescriptorSets(Context.GetDevice(), &AllocateInfo, Sets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate material descriptor sets.");
        }

        MaterialResources.resize(MaterialCount);
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            const MaterialAssetHandle Handle = static_cast<MaterialAssetHandle>(Index);
            const MaterialAsset&      Material = Assets.GetMaterial(Handle);
            const TextureAsset&       BaseTexture = Assets.GetTexture(Material.GetBaseColorTexture());
            const TextureAsset&       NormalTexture = Assets.GetTexture(Material.GetNormalTexture());

            MaterialResources[Index].UniformBuffer =
                std::make_unique<GPUBuffer>(Context.GetPhysicalDevice(),
                                            Context.GetDevice(),
                                            sizeof(MaterialUniform),
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            MaterialResources[Index].DescriptorSet = Sets[Index];

            const MaterialUniform Uniform{Material.GetBaseColor(),
                                          static_cast<std::uint32_t>(ViewMode),
                                          bFlipNormalY ? 1U : 0U,
                                          NormalStrength,
                                          AmbientLight};
            MaterialResources[Index].UniformBuffer->Upload(&Uniform, sizeof(Uniform));

            VkDescriptorImageInfo BaseImage{};
            BaseImage.sampler = BaseTexture.GetSampler();
            BaseImage.imageView = BaseTexture.GetImageView();
            BaseImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo NormalImage{};
            NormalImage.sampler = NormalTexture.GetSampler();
            NormalImage.imageView = NormalTexture.GetImageView();
            NormalImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorBufferInfo MaterialBuffer{};
            MaterialBuffer.buffer = MaterialResources[Index].UniformBuffer->GetHandle();
            MaterialBuffer.offset = 0;
            MaterialBuffer.range = sizeof(MaterialUniform);

            std::array<VkWriteDescriptorSet, 3> Writes{};
            for (VkWriteDescriptorSet& Write : Writes)
            {
                Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                Write.dstSet = Sets[Index];
                Write.descriptorCount = 1;
            }
            Writes[0].dstBinding = 0;
            Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Writes[0].pImageInfo = &BaseImage;
            Writes[1].dstBinding = 1;
            Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Writes[1].pImageInfo = &NormalImage;
            Writes[2].dstBinding = 2;
            Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            Writes[2].pBufferInfo = &MaterialBuffer;

            vkUpdateDescriptorSets(
                Context.GetDevice(), static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
        }

        Logger::Info("Renderer", "Material descriptor sets created: " + std::to_string(MaterialResources.size()) + ".");
    }

    void Renderer::UpdateMaterialUniforms()
    {
        const std::size_t MaterialCount = std::min(Assets.GetMaterialCount(), MaterialResources.size());
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            if (!MaterialResources[Index].UniformBuffer)
            {
                continue;
            }

            const MaterialAsset&  Material = Assets.GetMaterial(static_cast<MaterialAssetHandle>(Index));
            const MaterialUniform Uniform{Material.GetBaseColor(),
                                          static_cast<std::uint32_t>(ViewMode),
                                          bFlipNormalY ? 1U : 0U,
                                          NormalStrength,
                                          AmbientLight};
            MaterialResources[Index].UniformBuffer->Upload(&Uniform, sizeof(Uniform));
        }
    }

    void Renderer::RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                       std::uint32_t   ImageIndex,
                                       const Scene&    SceneData,
                                       const DebugUI&  DebugInterface) const
    {
        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin Vulkan command buffer.");
        }

        std::array<VkClearValue, 2> ClearValues{};
        ClearValues[0].color = {{0.03F, 0.04F, 0.06F, 1.0F}};
        ClearValues[1].depthStencil = {1.0F, 0};

        VkRenderPassBeginInfo RenderPassInfo{};
        RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassInfo.renderPass = MainRenderPass.GetHandle();
        RenderPassInfo.framebuffer = MainFramebuffers.Get(ImageIndex);
        RenderPassInfo.renderArea.offset = {0, 0};
        RenderPassInfo.renderArea.extent = SwapchainData.GetExtent();
        RenderPassInfo.clearValueCount = static_cast<std::uint32_t>(ClearValues.size());
        RenderPassInfo.pClearValues = ClearValues.data();

        vkCmdBeginRenderPass(CommandBuffer, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, StaticMeshPipeline.GetHandle());

        const VkExtent2D Extent = SwapchainData.GetExtent();
        VkViewport       Viewport{};
        Viewport.x = 0.0F;
        Viewport.y = 0.0F;
        Viewport.width = static_cast<float>(Extent.width);
        Viewport.height = static_cast<float>(Extent.height);
        Viewport.minDepth = 0.0F;
        Viewport.maxDepth = 1.0F;
        vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

        VkRect2D Scissor{};
        Scissor.offset = {0, 0};
        Scissor.extent = Extent;
        vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

        const float AspectRatio =
            Extent.height == 0 ? 1.0F : static_cast<float>(Extent.width) / static_cast<float>(Extent.height);
        const glm::mat4 ViewProjection = SceneData.GetMainCamera().GetViewProjectionMatrix(AspectRatio);

        for (const StaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
        {
            if (Instance.GetMesh() == InvalidAssetHandle)
            {
                continue;
            }

            const MeshAsset&   Mesh = Assets.GetMesh(Instance.GetMesh());
            const VkBuffer     VertexBuffer = Mesh.GetVertexBuffer().GetHandle();
            const VkDeviceSize VertexOffset = 0;
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &VertexOffset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh.GetIndexBuffer().GetHandle(), 0, VK_INDEX_TYPE_UINT32);

            const StaticMeshPushConstants PushConstants{Instance.GetTransform().GetMatrix(), ViewProjection};
            vkCmdPushConstants(CommandBuffer,
                               StaticMeshPipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(PushConstants),
                               &PushConstants);

            for (const MeshSection& Section : Mesh.GetSections())
            {
                if (Section.Material >= MaterialResources.size())
                {
                    continue;
                }

                const VkDescriptorSet DescriptorSet = MaterialResources[Section.Material].DescriptorSet;
                vkCmdBindDescriptorSets(CommandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        StaticMeshPipeline.GetLayout(),
                                        0,
                                        1,
                                        &DescriptorSet,
                                        0,
                                        nullptr);

                vkCmdDrawIndexed(CommandBuffer, Section.IndexCount, 1, Section.FirstIndex, 0, 0);
            }
        }

        DebugInterface.Render(CommandBuffer);

        vkCmdEndRenderPass(CommandBuffer);
        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record Vulkan command buffer.");
        }
    }

    VkFormat Renderer::FindDepthFormat(VkPhysicalDevice PhysicalDevice)
    {
        constexpr std::array<VkFormat, 3> Candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };
        return FindSupportedFormat(PhysicalDevice,
                                   Candidates.data(),
                                   static_cast<std::uint32_t>(Candidates.size()),
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat Renderer::FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
                                           const VkFormat*      Candidates,
                                           std::uint32_t        CandidateCount,
                                           VkImageTiling        Tiling,
                                           VkFormatFeatureFlags Features)
    {
        for (std::uint32_t Index = 0; Index < CandidateCount; ++Index)
        {
            VkFormatProperties Properties{};
            vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Candidates[Index], &Properties);

            const VkFormatFeatureFlags AvailableFeatures =
                Tiling == VK_IMAGE_TILING_LINEAR ? Properties.linearTilingFeatures : Properties.optimalTilingFeatures;
            if ((AvailableFeatures & Features) == Features)
            {
                return Candidates[Index];
            }
        }
        throw std::runtime_error("Failed to find a supported Vulkan format.");
    }
} // namespace MDSS
