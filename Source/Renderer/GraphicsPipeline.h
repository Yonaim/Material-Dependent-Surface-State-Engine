#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(VkDevice Device, VkRenderPass RenderPass);
        ~GraphicsPipeline();

        GraphicsPipeline(const GraphicsPipeline&) = delete;
        GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
        GraphicsPipeline(GraphicsPipeline&&) = delete;
        GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

        [[nodiscard]] VkPipeline       GetHandle() const noexcept;
        [[nodiscard]] VkPipelineLayout GetLayout() const noexcept;

    private:
        static VkShaderModule CreateShaderModule(VkDevice Device, const char* Path);

        VkDevice         Device = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkPipeline       Pipeline = VK_NULL_HANDLE;
    };
} // namespace MDSS
