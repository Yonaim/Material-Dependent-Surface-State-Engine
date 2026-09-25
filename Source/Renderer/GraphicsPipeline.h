/**
 * @file GraphicsPipeline.h
 * @brief shader stage와 고정 기능 설정을 이용한 graphics pipeline 생성.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace MDSS
{
    struct ShaderStageConfig
    {
        VkShaderStageFlagBits Stage = VK_SHADER_STAGE_VERTEX_BIT;
        std::string           ShaderPath;
        std::string           EntryPoint = "main";
    };

    struct GraphicsPipelineConfig
    {
        std::vector<ShaderStageConfig> ShaderStages;

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
        /**
         * @brief 설정에 지정된 shader와 fixed-function state로 pipeline을 생성한다.
         * @throws std::runtime_error shader 파일 또는 Vulkan pipeline 생성이 실패한 경우.
         */
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
