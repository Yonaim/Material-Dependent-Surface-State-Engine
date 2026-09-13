#include "Renderer/RenderPass.h"

#include <stdexcept>

namespace MDSS
{
    RenderPass::RenderPass(VkDevice Device, VkFormat ColorFormat) : Device(Device)
    {
        VkAttachmentDescription ColorAttachment{};
        ColorAttachment.format = ColorFormat;
        ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference ColorReference{};
        ColorReference.attachment = 0;
        ColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription Subpass{};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments = &ColorReference;

        VkSubpassDependency Dependency{};
        Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        Dependency.dstSubpass = 0;
        Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        CreateInfo.attachmentCount = 1;
        CreateInfo.pAttachments = &ColorAttachment;
        CreateInfo.subpassCount = 1;
        CreateInfo.pSubpasses = &Subpass;
        CreateInfo.dependencyCount = 1;
        CreateInfo.pDependencies = &Dependency;

        if (vkCreateRenderPass(Device, &CreateInfo, nullptr, &Handle) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan render pass.");
        }
    }

    RenderPass::~RenderPass()
    {
        if (Handle != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(Device, Handle, nullptr);
            Handle = VK_NULL_HANDLE;
        }
    }

    VkRenderPass RenderPass::GetHandle() const noexcept
    {
        return Handle;
    }
} // namespace MDSS
