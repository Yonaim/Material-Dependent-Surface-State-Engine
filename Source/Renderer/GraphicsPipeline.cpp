#include "Renderer/GraphicsPipeline.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace MDSS
{
    namespace
    {
        std::vector<std::uint32_t> ReadSpirvFile(const char* Path)
        {
            std::ifstream File(Path, std::ios::ate | std::ios::binary);
            if (!File.is_open())
            {
                throw std::runtime_error(std::string("Failed to open shader file: ") + Path);
            }

            const std::streamsize Size = File.tellg();
            if (Size <= 0 || (Size % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0)
            {
                throw std::runtime_error(std::string("Invalid SPIR-V file size: ") + Path);
            }

            std::vector<std::uint32_t> Data(static_cast<std::size_t>(Size) / sizeof(std::uint32_t));
            File.seekg(0);
            File.read(reinterpret_cast<char*>(Data.data()), Size);

            if (!File)
            {
                throw std::runtime_error(std::string("Failed to read shader file: ") + Path);
            }

            return Data;
        }
    } // namespace

    GraphicsPipeline::GraphicsPipeline(VkDevice Device, VkRenderPass RenderPass, const GraphicsPipelineConfig& Config)
        : Device(Device)
    {
        if (Config.VertexShaderPath == nullptr || Config.FragmentShaderPath == nullptr)
        {
            throw std::invalid_argument("Graphics pipeline shader paths must not be null.");
        }

        VkShaderModule VertexShader = VK_NULL_HANDLE;
        VkShaderModule FragmentShader = VK_NULL_HANDLE;

        try
        {
            VertexShader = CreateShaderModule(Device, Config.VertexShaderPath);
            FragmentShader = CreateShaderModule(Device, Config.FragmentShaderPath);

            VkPipelineShaderStageCreateInfo VertexStage{};
            VertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            VertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
            VertexStage.module = VertexShader;
            VertexStage.pName = "main";

            VkPipelineShaderStageCreateInfo FragmentStage{};
            FragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            FragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            FragmentStage.module = FragmentShader;
            FragmentStage.pName = "main";

            const std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {VertexStage, FragmentStage};

            VkPipelineVertexInputStateCreateInfo VertexInput{};
            VertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(Config.VertexBindings.size());
            VertexInput.pVertexBindingDescriptions =
                Config.VertexBindings.empty() ? nullptr : Config.VertexBindings.data();
            VertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(Config.VertexAttributes.size());
            VertexInput.pVertexAttributeDescriptions =
                Config.VertexAttributes.empty() ? nullptr : Config.VertexAttributes.data();

            VkPipelineInputAssemblyStateCreateInfo InputAssembly{};
            InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            InputAssembly.topology = Config.Topology;
            InputAssembly.primitiveRestartEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo ViewportState{};
            ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            ViewportState.viewportCount = 1;
            ViewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo Rasterizer{};
            Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            Rasterizer.depthClampEnable = VK_FALSE;
            Rasterizer.rasterizerDiscardEnable = VK_FALSE;
            Rasterizer.polygonMode = Config.PolygonMode;
            Rasterizer.cullMode = Config.CullMode;
            Rasterizer.frontFace = Config.FrontFace;
            Rasterizer.depthBiasEnable = VK_FALSE;
            Rasterizer.lineWidth = 1.0F;

            VkPipelineMultisampleStateCreateInfo Multisampling{};
            Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            Multisampling.rasterizationSamples = Config.RasterizationSamples;
            Multisampling.sampleShadingEnable = VK_FALSE;

            VkPipelineDepthStencilStateCreateInfo DepthStencil{};
            DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            DepthStencil.depthTestEnable = Config.bDepthTestEnabled ? VK_TRUE : VK_FALSE;
            DepthStencil.depthWriteEnable = Config.bDepthWriteEnabled ? VK_TRUE : VK_FALSE;
            DepthStencil.depthCompareOp = Config.DepthCompareOp;
            DepthStencil.depthBoundsTestEnable = VK_FALSE;
            DepthStencil.stencilTestEnable = VK_FALSE;

            VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
            ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            ColorBlendAttachment.blendEnable = Config.bBlendingEnabled ? VK_TRUE : VK_FALSE;
            ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo ColorBlending{};
            ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            ColorBlending.logicOpEnable = VK_FALSE;
            ColorBlending.attachmentCount = 1;
            ColorBlending.pAttachments = &ColorBlendAttachment;

            const std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo    DynamicState{};
            DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            DynamicState.dynamicStateCount = static_cast<std::uint32_t>(DynamicStates.size());
            DynamicState.pDynamicStates = DynamicStates.data();

            VkPipelineLayoutCreateInfo LayoutInfo{};
            LayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            LayoutInfo.setLayoutCount = static_cast<std::uint32_t>(Config.DescriptorSetLayouts.size());
            LayoutInfo.pSetLayouts = Config.DescriptorSetLayouts.empty() ? nullptr : Config.DescriptorSetLayouts.data();
            LayoutInfo.pushConstantRangeCount = static_cast<std::uint32_t>(Config.PushConstantRanges.size());
            LayoutInfo.pPushConstantRanges =
                Config.PushConstantRanges.empty() ? nullptr : Config.PushConstantRanges.data();

            if (vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Vulkan graphics pipeline layout.");
            }

            VkGraphicsPipelineCreateInfo PipelineInfo{};
            PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            PipelineInfo.stageCount = static_cast<std::uint32_t>(ShaderStages.size());
            PipelineInfo.pStages = ShaderStages.data();
            PipelineInfo.pVertexInputState = &VertexInput;
            PipelineInfo.pInputAssemblyState = &InputAssembly;
            PipelineInfo.pViewportState = &ViewportState;
            PipelineInfo.pRasterizationState = &Rasterizer;
            PipelineInfo.pMultisampleState = &Multisampling;
            PipelineInfo.pDepthStencilState = &DepthStencil;
            PipelineInfo.pColorBlendState = &ColorBlending;
            PipelineInfo.pDynamicState = &DynamicState;
            PipelineInfo.layout = PipelineLayout;
            PipelineInfo.renderPass = RenderPass;
            PipelineInfo.subpass = 0;

            if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create Vulkan graphics pipeline.");
            }
        }
        catch (...)
        {
            if (Pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(Device, Pipeline, nullptr);
                Pipeline = VK_NULL_HANDLE;
            }
            if (PipelineLayout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
                PipelineLayout = VK_NULL_HANDLE;
            }
            if (FragmentShader != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(Device, FragmentShader, nullptr);
            }
            if (VertexShader != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(Device, VertexShader, nullptr);
            }
            throw;
        }

        vkDestroyShaderModule(Device, FragmentShader, nullptr);
        vkDestroyShaderModule(Device, VertexShader, nullptr);
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
        if (Pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(Device, Pipeline, nullptr);
            Pipeline = VK_NULL_HANDLE;
        }

        if (PipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
            PipelineLayout = VK_NULL_HANDLE;
        }
    }

    VkPipeline GraphicsPipeline::GetHandle() const noexcept
    {
        return Pipeline;
    }

    VkPipelineLayout GraphicsPipeline::GetLayout() const noexcept
    {
        return PipelineLayout;
    }

    VkShaderModule GraphicsPipeline::CreateShaderModule(VkDevice Device, const char* Path)
    {
        const std::vector<std::uint32_t> Code = ReadSpirvFile(Path);

        VkShaderModuleCreateInfo CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        CreateInfo.codeSize = Code.size() * sizeof(std::uint32_t);
        CreateInfo.pCode = Code.data();

        VkShaderModule Module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(Device, &CreateInfo, nullptr, &Module) != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Failed to create shader module: ") + Path);
        }

        return Module;
    }
} // namespace MDSS
