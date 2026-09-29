/** @file SurfaceDebugRenderingTests.cpp
 * @brief 실제 fragment 출력으로 면적 비율과 격자의 해상도·축소 동작을 검증한다.
 */
#include "AssetManager/Assets/MeshSourceData.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/Renderer.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "VulkanContext/GPU/GPUImage.h"
#include "VulkanContext/GPU/GPUImageView.h"
#include "VulkanContext/VulkanContext.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace MDSS::Tests
{
    namespace
    {
        void Require(bool Condition, const char* Message)
        {
            if (!Condition)
                throw std::runtime_error(Message);
        }

        void RequireVk(VkResult Result)
        {
            Require(Result == VK_SUCCESS, "Could not create Surface debug render test resources.");
        }

        // Raw handles are released before their referenced image, buffer and Surface resources.
        struct TRenderHandles
        {
            VkDevice              Device;
            VkRenderPass          Pass = VK_NULL_HANDLE;
            VkFramebuffer         Framebuffer = VK_NULL_HANDLE;
            VkDescriptorSetLayout MaterialLayout = VK_NULL_HANDLE;
            VkDescriptorPool      Pool = VK_NULL_HANDLE;
            ~TRenderHandles()
            {
                if (Pool)
                    vkDestroyDescriptorPool(Device, Pool, nullptr);
                if (MaterialLayout)
                    vkDestroyDescriptorSetLayout(Device, MaterialLayout, nullptr);
                if (Framebuffer)
                    vkDestroyFramebuffer(Device, Framebuffer, nullptr);
                if (Pass)
                    vkDestroyRenderPass(Device, Pass, nullptr);
            }
        };

        struct alignas(16) TUniform
        {
            glm::vec4     BaseColor{1.0F};
            std::uint32_t RenderMode = 17;
            std::uint32_t FlipNormalY = 0;
            float         NormalStrength = 1.0F;
            float         AmbientLight = 0.25F;
            std::uint32_t DebugStateChannel = 0;
            std::uint32_t StateChannelCount = 1;
            float         DebugViewParameter = 1.0F / (256.0F * 256.0F);
            float         ReliefShadingEnabled = 0.0F;
        };
        static_assert(sizeof(TUniform) == 48);

        struct TPush
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };
    }

    void TestSurfaceDebugRendering(const TVulkanContext& Context)
    {
        const VkDevice       Device = Context.GetDevice();
        constexpr VkExtent2D Extent{256, 256};
        constexpr VkFormat   Format = VK_FORMAT_R32G32B32A32_SFLOAT;
        constexpr auto       HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        TGPUImage            Image(Context.GetPhysicalDevice(),
                        Device,
                        Extent,
                        Format,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        TGPUImageView        View(Device, Image.GetHandle(), Format, VK_IMAGE_ASPECT_COLOR_BIT);
        TGPUBuffer           Readback(Context.GetPhysicalDevice(),
                            Device,
                            Extent.width * Extent.height * sizeof(glm::vec4),
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            HostMemory);
        TGPUBuffer           UniformBuffer(
            Context.GetPhysicalDevice(), Device, sizeof(TUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, HostMemory);
        TGPUBuffer Vertices(
            Context.GetPhysicalDevice(), Device, 6 * sizeof(TVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, HostMemory);

        // Deliberately use invalid simulation texels: geometric diagnostics must still show the mesh.
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}});
        Geometry.SetProfileMap(std::vector<TSurfaceProfileIndex>(4, InvalidSurfaceProfileIndex));
        TSurfaceResponseProfileData Profile;
        Profile.States.emplace("test", TSurfaceStateParameters{});
        const std::vector<TSurfaceResponseProfileData> ProfileTable{Profile};
        const TSurfaceStateRegistry                    Registry(ProfileTable);
        TSurfaceSharedGeometryGPUResources             Shared(Context.GetPhysicalDevice(), Device, Geometry);
        TSurfaceProfileGPUResources  Profiles(Context.GetPhysicalDevice(), Device, ProfileTable, Registry);
        TSurfaceInstanceGPUResources Instance(
            Context.GetPhysicalDevice(), Device, 4, 1, std::vector<float>(4 * SurfaceNeighborCount, 0.0F));
        TSurfaceStateDescriptorResources SurfaceDescriptors(Device, Shared, Profiles, Instance);
        TRenderHandles                   Handles{Device};

        VkAttachmentDescription Attachment{};
        Attachment.format = Format;
        Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference Reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription  Subpass{};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments = &Reference;
        VkSubpassDependency Dependency{};
        Dependency.srcSubpass = 0;
        Dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        Dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo PassInfo{};
        PassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        PassInfo.attachmentCount = 1;
        PassInfo.pAttachments = &Attachment;
        PassInfo.subpassCount = 1;
        PassInfo.pSubpasses = &Subpass;
        PassInfo.dependencyCount = 1;
        PassInfo.pDependencies = &Dependency;
        RequireVk(vkCreateRenderPass(Device, &PassInfo, nullptr, &Handles.Pass));
        const VkImageView       ImageView = View.GetHandle();
        VkFramebufferCreateInfo FrameInfo{};
        FrameInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FrameInfo.renderPass = Handles.Pass;
        FrameInfo.attachmentCount = 1;
        FrameInfo.pAttachments = &ImageView;
        FrameInfo.width = Extent.width;
        FrameInfo.height = Extent.height;
        FrameInfo.layers = 1;
        RequireVk(vkCreateFramebuffer(Device, &FrameInfo, nullptr, &Handles.Framebuffer));

        VkDescriptorSetLayoutBinding    Binding{2,
                                             VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             1,
                                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                             nullptr};
        VkDescriptorSetLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = 1;
        LayoutInfo.pBindings = &Binding;
        RequireVk(vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &Handles.MaterialLayout));
        VkDescriptorPoolSize       PoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.maxSets = 1;
        PoolInfo.poolSizeCount = 1;
        PoolInfo.pPoolSizes = &PoolSize;
        RequireVk(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Handles.Pool));
        VkDescriptorSetAllocateInfo AllocateInfo{};
        AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        AllocateInfo.descriptorPool = Handles.Pool;
        AllocateInfo.descriptorSetCount = 1;
        AllocateInfo.pSetLayouts = &Handles.MaterialLayout;
        VkDescriptorSet MaterialSet = VK_NULL_HANDLE;
        RequireVk(vkAllocateDescriptorSets(Device, &AllocateInfo, &MaterialSet));
        VkDescriptorBufferInfo BufferInfo{UniformBuffer.GetHandle(), 0, sizeof(TUniform)};
        VkWriteDescriptorSet   Write{};
        Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        Write.dstSet = MaterialSet;
        Write.dstBinding = 2;
        Write.descriptorCount = 1;
        Write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        Write.pBufferInfo = &BufferInfo;
        vkUpdateDescriptorSets(Device, 1, &Write, 0, nullptr);

        TGraphicsPipelineConfig Config;
        Config.ShaderStages = {{VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.vert.spv"},
                               {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.frag.spv"}};
        Config.CullMode = VK_CULL_MODE_NONE;
        Config.VertexBindings = {{0, sizeof(TVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
        Config.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TVertex, Position)},
                                   {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TVertex, Normal)},
                                   {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TVertex, UV)},
                                   {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TVertex, Tangent)}};
        Config.DescriptorSetLayouts = {Handles.MaterialLayout, SurfaceDescriptors.GetLayout()};
        Config.PushConstantRanges = {{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(TPush)}};
        TGraphicsPipeline Pipeline(Device, Handles.Pass, Config);

        auto Render = [&](TRenderViewMode Mode,
                          std::uint32_t   Resolution,
                          float           Parameter,
                          const TPush&    Push = TPush{},
                          glm::vec2       UVScale = glm::vec2(1.0F),
                          glm::vec2       UVOffset = glm::vec2(0.0F))
        {
            const std::array<glm::vec2, 6> Corners = {{{0, 0}, {1, 0}, {1, 1}, {0, 0}, {1, 1}, {0, 1}}};
            std::array<TVertex, 6>         Data{};
            for (std::size_t Index = 0; Index < Data.size(); ++Index)
            {
                Data[Index].Position = glm::vec3(Corners[Index] - 0.5F, 0.0F);
                Data[Index].Normal = {0, 0, 1};
                Data[Index].UV = Corners[Index] * UVScale + UVOffset;
                Data[Index].Tangent = {1, 0, 0, 1};
            }
            Vertices.Upload(Data.data(), sizeof(Data));
            TUniform Uniform;
            Uniform.RenderMode = static_cast<std::uint32_t>(Mode);
            Uniform.DebugViewParameter = Parameter;
            UniformBuffer.Upload(&Uniform, sizeof(Uniform));
            const glm::uvec4 Range{0, Resolution, Resolution, Resolution * Resolution};
            Shared.GetSurfaceRangeBuffer().Upload(&Range, sizeof(Range));
            const auto            Command = Context.GetCommands().BeginSingleTime();
            VkClearValue          Clear{};
            VkRenderPassBeginInfo Begin{};
            Begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            Begin.renderPass = Handles.Pass;
            Begin.framebuffer = Handles.Framebuffer;
            Begin.renderArea.extent = Extent;
            Begin.clearValueCount = 1;
            Begin.pClearValues = &Clear;
            vkCmdBeginRenderPass(Command, &Begin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetHandle());
            const VkViewport Viewport{0, 0, float(Extent.width), float(Extent.height), 0, 1};
            const VkRect2D   Scissor{{0, 0}, Extent};
            vkCmdSetViewport(Command, 0, 1, &Viewport);
            vkCmdSetScissor(Command, 0, 1, &Scissor);
            const std::array<VkDescriptorSet, 2> Sets{MaterialSet, SurfaceDescriptors.GetABSet()};
            vkCmdBindDescriptorSets(Command,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    Pipeline.GetLayout(),
                                    0,
                                    Sets.size(),
                                    Sets.data(),
                                    0,
                                    nullptr);
            const VkBuffer     VertexBuffer = Vertices.GetHandle();
            const VkDeviceSize Offset = 0;
            vkCmdBindVertexBuffers(Command, 0, 1, &VertexBuffer, &Offset);
            vkCmdPushConstants(Command, Pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &Push);
            vkCmdDraw(Command, 6, 1, 0, 0);
            vkCmdEndRenderPass(Command);
            VkBufferImageCopy Copy{};
            Copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            Copy.imageExtent = {Extent.width, Extent.height, 1};
            vkCmdCopyImageToBuffer(
                Command, Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Readback.GetHandle(), 1, &Copy);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            std::vector<glm::vec4> Pixels(Extent.width * Extent.height);
            Readback.Download(Pixels.data(), Pixels.size() * sizeof(glm::vec4));
            return Pixels;
        };
        auto Pixel = [&](const auto& Pixels, std::size_t X = 127, std::size_t Y = 127)
        { return glm::vec3(Pixels[Y * Extent.width + X]); };
        auto Near = [](glm::vec3 Actual, glm::vec3 Expected) { return glm::length(Actual - Expected) < 0.025F; };
        constexpr float ReferenceArea = 1.0F / (256.0F * 256.0F);
        const auto      AreaView = TRenderViewMode::SurfaceTexelArea;
        const glm::vec3 Green{0.10F, 0.78F, 0.24F}, Red{0.92F, 0.18F, 0.12F}, Blue{0.12F, 0.52F, 0.92F};
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea)), Green),
                "Unit square at reference resolution must be green.");
        Require(Near(Pixel(Render(AreaView, 128, ReferenceArea)), Red),
                "Half resolution must cover four times the area.");
        Require(Near(Pixel(Render(AreaView, 512, ReferenceArea)), Blue),
                "Double resolution must cover one quarter of the area.");
        TPush Scaled;
        Scaled.Model = glm::scale(glm::mat4(1), glm::vec3(2));
        Scaled.ViewProjection = glm::scale(glm::mat4(1), glm::vec3(0.5F));
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, Scaled)), Red),
                "World scale must affect area even at identical screen size.");
        Scaled.Model = glm::scale(glm::mat4(1), glm::vec3(2, 0.5F, 3));
        Scaled.ViewProjection = glm::inverse(Scaled.Model);
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, Scaled)), Green),
                "Nonuniform scale must use surface area, not volume.");
        TPush Camera;
        Camera.ViewProjection = glm::perspective(glm::radians(45.0F), 1.0F, 0.1F, 10.0F) *
                                glm::lookAt(glm::vec3(1, 0.5F, 2), glm::vec3(0), glm::vec3(0, 1, 0));
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, Camera)), Green),
                "Perspective and viewing angle must preserve area colors.");
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, {}, glm::vec2(0.5F))), Red),
                "Smaller UV allocation must increase texel area.");
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, {}, {-1, 1}, {1, 0})), Green),
                "Mirrored UVs must preserve positive area.");
        Require(Near(Pixel(Render(AreaView, 256, ReferenceArea, {}, glm::vec2(0))), {1, 0.18F, 0.72F}),
                "Degenerate UVs must show the invalid-area legend color.");
        const auto GridView = TRenderViewMode::SurfaceTexelGrid;
        const auto Grid8 = Render(GridView, 128, 8, {}, glm::vec2(0.125F));
        const auto Grid16 = Render(GridView, 128, 16, {}, glm::vec2(0.125F));
        Require(Pixel(Grid8).b > Pixel(Grid16).b + 0.15F,
                "Changing 8 to 16 must change the coarse boundary without moving the fine grid.");
        Require(Near(Pixel(Render(GridView, 512, 8)), {0.215F, 0.245F, 0.275F}),
                "Subpixel grid patterns must fade to a neutral fill.");
    }
} // namespace MDSS::Tests
