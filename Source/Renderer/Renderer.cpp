#include "Renderer/Renderer.h"

#include "Application/Window.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/VulkanContext.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#ifndef MDSSP_SHADER_DIR
#define MDSSP_SHADER_DIR "Shaders"
#endif

namespace MDSS
{
    namespace
    {
        struct CubeVertex
        {
            float Position[3];
            float Normal[3];
            float UV[2];
        };

        struct StaticMeshPushConstants
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };

        static_assert(sizeof(StaticMeshPushConstants) == 128,
                      "Static mesh push constants are expected to use Vulkan's guaranteed 128-byte minimum.");

        constexpr std::array<CubeVertex, 24> CubeVertices = {{
            // Front (+Z)
            {{-0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F}},
            {{0.5F, -0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F}},
            {{0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F}},
            {{-0.5F, 0.5F, 0.5F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F}},

            // Back (-Z)
            {{0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}, {0.0F, 0.0F}},
            {{-0.5F, -0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}, {1.0F, 0.0F}},
            {{-0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}, {1.0F, 1.0F}},
            {{0.5F, 0.5F, -0.5F}, {0.0F, 0.0F, -1.0F}, {0.0F, 1.0F}},

            // Right (+X)
            {{0.5F, -0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}, {0.0F, 0.0F}},
            {{0.5F, -0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}, {1.0F, 0.0F}},
            {{0.5F, 0.5F, -0.5F}, {1.0F, 0.0F, 0.0F}, {1.0F, 1.0F}},
            {{0.5F, 0.5F, 0.5F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F}},

            // Left (-X)
            {{-0.5F, -0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 0.0F}},
            {{-0.5F, -0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}, {1.0F, 0.0F}},
            {{-0.5F, 0.5F, 0.5F}, {-1.0F, 0.0F, 0.0F}, {1.0F, 1.0F}},
            {{-0.5F, 0.5F, -0.5F}, {-1.0F, 0.0F, 0.0F}, {0.0F, 1.0F}},

            // Top (+Y)
            {{-0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}},
            {{0.5F, 0.5F, 0.5F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F}},
            {{0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}, {1.0F, 1.0F}},
            {{-0.5F, 0.5F, -0.5F}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F}},

            // Bottom (-Y)
            {{-0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}, {0.0F, 0.0F}},
            {{0.5F, -0.5F, -0.5F}, {0.0F, -1.0F, 0.0F}, {1.0F, 0.0F}},
            {{0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}, {1.0F, 1.0F}},
            {{-0.5F, -0.5F, 0.5F}, {0.0F, -1.0F, 0.0F}, {0.0F, 1.0F}},
        }};

        constexpr std::array<std::uint32_t, 36> CubeIndices = {{
            0,  1,  2,  2,  3,  0,  // Front
            4,  5,  6,  6,  7,  4,  // Back
            8,  9,  10, 10, 11, 8,  // Right
            12, 13, 14, 14, 15, 12, // Left
            16, 17, 18, 18, 19, 16, // Top
            20, 21, 22, 22, 23, 20  // Bottom
        }};

        GraphicsPipelineConfig BuildStaticMeshPipelineConfig()
        {
            static const std::string VertexShaderPath = std::string(MDSSP_SHADER_DIR) + "/StaticMesh.vert.spv";
            static const std::string FragmentShaderPath = std::string(MDSSP_SHADER_DIR) + "/StaticMesh.frag.spv";

            GraphicsPipelineConfig Config{};
            Config.VertexShaderPath = VertexShaderPath.c_str();
            Config.FragmentShaderPath = FragmentShaderPath.c_str();
            Config.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            Config.CullMode = VK_CULL_MODE_BACK_BIT;
            Config.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            Config.bDepthTestEnabled = true;
            Config.bDepthWriteEnabled = true;
            Config.DepthCompareOp = VK_COMPARE_OP_LESS;
            Config.bBlendingEnabled = false;

            VkVertexInputBindingDescription Binding{};
            Binding.binding = 0;
            Binding.stride = sizeof(CubeVertex);
            Binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            Config.VertexBindings.push_back(Binding);

            VkVertexInputAttributeDescription PositionAttribute{};
            PositionAttribute.location = 0;
            PositionAttribute.binding = 0;
            PositionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
            PositionAttribute.offset = static_cast<std::uint32_t>(offsetof(CubeVertex, Position));
            Config.VertexAttributes.push_back(PositionAttribute);

            VkVertexInputAttributeDescription NormalAttribute{};
            NormalAttribute.location = 1;
            NormalAttribute.binding = 0;
            NormalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
            NormalAttribute.offset = static_cast<std::uint32_t>(offsetof(CubeVertex, Normal));
            Config.VertexAttributes.push_back(NormalAttribute);

            VkVertexInputAttributeDescription UVAttribute{};
            UVAttribute.location = 2;
            UVAttribute.binding = 0;
            UVAttribute.format = VK_FORMAT_R32G32_SFLOAT;
            UVAttribute.offset = static_cast<std::uint32_t>(offsetof(CubeVertex, UV));
            Config.VertexAttributes.push_back(UVAttribute);

            VkPushConstantRange PushConstantRange{};
            PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            PushConstantRange.offset = 0;
            PushConstantRange.size = sizeof(StaticMeshPushConstants);
            Config.PushConstantRanges.push_back(PushConstantRange);

            return Config;
        }
    } // namespace

    Renderer::Renderer(const VulkanContext& Context, const Window& Window)
        : Context(Context), SwapchainData(Context, Window), DepthFormat(FindDepthFormat(Context.GetPhysicalDevice())),
          DepthImage(Context.GetPhysicalDevice(),
                     Context.GetDevice(),
                     SwapchainData.GetExtent(),
                     DepthFormat,
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          DepthImageView(Context.GetDevice(), DepthImage.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT),
          MainRenderPass(Context.GetDevice(), SwapchainData.GetImageFormat(), DepthFormat),
          StaticMeshPipeline(Context.GetDevice(), MainRenderPass.GetHandle(), BuildStaticMeshPipelineConfig()),
          MainFramebuffers(Context.GetDevice(),
                           MainRenderPass.GetHandle(),
                           SwapchainData.GetExtent(),
                           SwapchainData.GetImageViews(),
                           DepthImageView.GetHandle()),
          FrameContext(Context),
          CubeVertexBuffer(Context.GetPhysicalDevice(),
                           Context.GetDevice(),
                           sizeof(CubeVertices),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
          CubeIndexBuffer(Context.GetPhysicalDevice(),
                          Context.GetDevice(),
                          sizeof(CubeIndices),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
          CubeIndexCount(static_cast<std::uint32_t>(CubeIndices.size()))
    {
        CubeVertexBuffer.Upload(CubeVertices.data(), sizeof(CubeVertices));
        CubeIndexBuffer.Upload(CubeIndices.data(), sizeof(CubeIndices));

        std::cout << "[Renderer] Depth buffer, camera-ready static mesh pipeline, and cube buffers created.\n";
    }

    Renderer::~Renderer()
    {
        if (Context.GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Context.GetDevice());
        }
    }

    void Renderer::RenderFrame(const Scene& SceneData)
    {
        FrameContext.WaitForCurrentFrame();

        std::uint32_t  ImageIndex = 0;
        const VkResult AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);

        if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Swapchain recreation is intentionally deferred to a later renderer milestone.
            // Returning here is safe because the current in-flight fence has not been reset yet.
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

        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData);

        const VkSemaphore          WaitSemaphores[] = {FrameContext.GetImageAvailableSemaphore()};
        const VkPipelineStageFlags WaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSemaphore          SignalSemaphores[] = {FrameContext.GetRenderFinishedSemaphore()};

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = WaitSemaphores;
        SubmitInfo.pWaitDstStageMask = WaitStages;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = SignalSemaphores;

        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }

        const VkSwapchainKHR Swapchains[] = {SwapchainData.GetHandle()};

        VkPresentInfoKHR PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = SignalSemaphores;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = Swapchains;
        PresentInfo.pImageIndices = &ImageIndex;

        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
        if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR &&
            PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present Vulkan swapchain image.");
        }

        FrameContext.AdvanceFrame();
    }

    const Swapchain& Renderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
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
                (Tiling == VK_IMAGE_TILING_LINEAR) ? Properties.linearTilingFeatures : Properties.optimalTilingFeatures;

            if ((AvailableFeatures & Features) == Features)
            {
                return Candidates[Index];
            }
        }

        throw std::runtime_error("Failed to find a supported Vulkan depth format.");
    }

    void
    Renderer::RecordCommandBuffer(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex, const Scene& SceneData) const
    {
        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin recording Vulkan command buffer.");
        }

        std::array<VkClearValue, 2> ClearValues{};
        ClearValues[0].color.float32[0] = 0.03F;
        ClearValues[0].color.float32[1] = 0.03F;
        ClearValues[0].color.float32[2] = 0.05F;
        ClearValues[0].color.float32[3] = 1.0F;
        ClearValues[1].depthStencil.depth = 1.0F;
        ClearValues[1].depthStencil.stencil = 0;

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

        VkViewport Viewport{};
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

        const VkBuffer         VertexBuffers[] = {CubeVertexBuffer.GetHandle()};
        constexpr VkDeviceSize VertexOffsets[] = {0};
        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, VertexBuffers, VertexOffsets);
        vkCmdBindIndexBuffer(CommandBuffer, CubeIndexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);

        const float     AspectRatio = static_cast<float>(Extent.width) / static_cast<float>(Extent.height);
        const glm::mat4 ViewProjection = SceneData.GetMainCamera().GetViewProjectionMatrix(AspectRatio);

        for (const StaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
        {
            StaticMeshPushConstants PushConstants{};
            PushConstants.Model = Instance.GetTransform().GetMatrix();
            PushConstants.ViewProjection = ViewProjection;

            vkCmdPushConstants(CommandBuffer,
                               StaticMeshPipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(StaticMeshPushConstants),
                               &PushConstants);

            vkCmdDrawIndexed(CommandBuffer, CubeIndexCount, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(CommandBuffer);

        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record Vulkan command buffer.");
        }
    }
} // namespace MDSS
