#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    struct GraphicsPipelineConfig
    {
        const char* VertexShaderPath = nullptr;
        const char* FragmentShaderPath = nullptr;

        VkPrimitiveTopology                            Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        std::vector<VkVertexInputBindingDescription>   VertexBindings;
        std::vector<VkVertexInputAttributeDescription> VertexAttributes;

        VkPolygonMode         PolygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags       CullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace           FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkSampleCountFlagBits RasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        bool        bDepthTestEnabled = false;
        bool        bDepthWriteEnabled = false;
        VkCompareOp DepthCompareOp = VK_COMPARE_OP_LESS;
        bool        bBlendingEnabled = false;

        std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
        std::vector<VkPushConstantRange>   PushConstantRanges;
    };

    class GraphicsPipeline
    {
    public:
        GraphicsPipeline(VkDevice Device, VkRenderPass RenderPass, const GraphicsPipelineConfig& Config);
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
