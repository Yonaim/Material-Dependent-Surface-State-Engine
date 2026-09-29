/** @file SurfaceDebugRenderingTests.cpp
 * @brief 실제 fragment 출력으로 면적 비율과 격자의 해상도·축소 동작을 검증한다.
 */
#include "AssetManager/Assets/MeshSourceData.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/Renderer.h"
#include "SurfaceStateSystem/Debug/TexelInspector.h"
#include "SurfaceStateSystem/Debug/TexelGeometryPreview.h"
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
            glm::vec4     DebugOptions{4.0F, 0.01F, 0.01F, 1.0F};
            glm::uvec4    DebugFlags{0};
        };
        static_assert(sizeof(TUniform) == 80);

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

        TUniform DebugControls;
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
            TUniform Uniform = DebugControls;
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

        // Diagnose the actual GPU values, not a CPU copy of the height formula.
        const std::array<std::uint32_t, 4> Valid{0, 0, 0, 0};
        Shared.GetTexelSurfaceIndexBuffer().Upload(Valid.data(), sizeof(Valid));
        Shared.GetTexelProfileIndexBuffer().Upload(Valid.data(), sizeof(Valid));
        std::array<TSurfaceGPUGeometryScalar, 4> Scalars{};
        for (auto& Scalar : Scalars)
            Scalar.MesoVirtualHeight = -0.02F;
        Shared.GetGeometryScalarBuffer().Upload(Scalars.data(), sizeof(Scalars));
        std::array<TSurfaceGPUVec4, 4>            Normals{}, Positions{};
        std::array<TSurfaceGPUNeighborIndices, 4> Neighbors{};
        for (std::uint32_t Index = 0; Index < 4; ++Index)
        {
            Normals[Index] = {0, 0, 1, 0};
            Positions[Index] = {float(Index % 2), float(Index / 2), 0, 0};
            Neighbors[Index].Indices.fill(UINT32_MAX);
            for (std::uint32_t Other = 0, Slot = 0; Other < 4; ++Other)
                if (Other != Index)
                    Neighbors[Index].Indices[Slot++] = Other;
        }
        Shared.GetNormalBuffer().Upload(Normals.data(), sizeof(Normals));
        Shared.GetMesoNormalBuffer().Upload(Normals.data(), sizeof(Normals));
        Shared.GetPositionBuffer().Upload(Positions.data(), sizeof(Positions));
        Shared.GetNeighborIndexBuffer().Upload(Neighbors.data(), sizeof(Neighbors));
        Instance.UpdateWorldTexelAreas(std::vector<float>(4, 2 * SurfaceStateReferenceArea));
        auto Parameters = TSurfaceStateParameters{};
        Parameters.StateCapacity = 2;
        Parameters.AccumulationFactor = 0.15F;
        Parameters.CavityFillFactor = 0.8F;
        Profiles.UpdateParameters(0, 0, Parameters);
        std::array<float, 4> State{10, 10, 10, 10};
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        DebugControls.DebugFlags.x = 0;
        const auto StateView = TRenderViewMode::SurfaceStateHeatmap;
        const auto Saturated = Render(StateView, 2, 0);
        State.fill(20);
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        Require(Near(Pixel(Saturated), Pixel(Render(StateView, 2, 0))),
                "Saturation must preserve the existing 100% color cap.");
        DebugControls.DebugFlags.x = 1;
        DebugControls.DebugOptions.x = 40;
        const auto Raw20 = Render(StateView, 2, 0);
        State.fill(10);
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        Require(glm::length(Pixel(Raw20) - Pixel(Render(StateView, 2, 0))) > 0.1F,
                "Raw State must distinguish over-capacity amounts.");
        DebugControls.DebugOptions.x = 5;
        Require(Near(Pixel(Render(StateView, 2, 0)), {1, 0.25F, 0.05F}),
                "Raw State must mark values exceeding the fixed range.");
        DebugControls.DebugOptions.y = 0.02F;
        DebugControls.DebugFlags.y = 0;
        const auto TotalHeight = Render(TRenderViewMode::SurfaceAccumulation, 2, 0);
        DebugControls.DebugFlags.y = 1;
        const auto CavityHeight = Render(TRenderViewMode::SurfaceAccumulation, 2, 0);
        DebugControls.DebugFlags.y = 2;
        const auto FollowingHeight = Render(TRenderViewMode::SurfaceAccumulation, 2, 0);
        Require(glm::length(Pixel(TotalHeight) - Pixel(FollowingHeight)) > 0.1F &&
                    glm::length(Pixel(CavityHeight) - Pixel(FollowingHeight)) > 0.1F,
                "Accumulation components must produce distinct fragment output.");
        DebugControls.DebugFlags.y = 3;
        DebugControls.DebugOptions.y = 100;
        const auto FillView = Render(TRenderViewMode::SurfaceAccumulation, 2, 0);
        DebugControls.DebugOptions.y = 0.001F;
        Require(Near(Pixel(FillView), Pixel(Render(TRenderViewMode::SurfaceAccumulation, 2, 0))),
                "Cavity Fill must keep a fixed 0-1 range independent of height range.");

        TTexelInspector              Inspector(Context.GetPhysicalDevice(), Device, SurfaceDescriptors.GetLayout(), 2);
        const TSurfaceTexelSelection Selection{0, 0, 0, 0, {0, 0}, "test"};
        const auto                   Sample =
            [&](bool bAB = true, std::uint32_t Channel = 0, float Reference = 0.01F, std::uint64_t Step = 1)
        {
            auto Command = Context.GetCommands().BeginSingleTime();
            Inspector.Record(Command, 0, SurfaceDescriptors, Selection, Channel, 1, Reference, bAB, Step);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            Inspector.CompleteFrame(0);
            Require(Inspector.GetSnapshot().has_value(), "Completed GPU snapshot must be available.");
            return *Inspector.GetSnapshot();
        };
        const auto Close = [](float A, float B) { return std::abs(A - B) < 1e-6F; };
        auto       S = Sample();
        Require(S.Values[2].w == 3 && Close(S.Values[0].x, 10) && Close(S.Values[0].y, 4) &&
                    Close(S.Values[0].z, 2.5F) && Close(S.Values[0].w, 5),
                "Inspector must expose unclamped State and area-scaled Capacity and saturation.");
        Require(Close(S.Values[2].y, 0.6F) && Close(S.Values[3].x, 0.012F) && Close(S.Values[3].y, 0.0015F) &&
                    Close(S.Values[3].z, 0.0135F),
                "GPU cavity/following allocation must use the reference-area amount.");
        Require(Close(Sample(true, 0, 0.01F, 50).Values[3].z, S.Values[3].z),
                "Unchanged State must not accumulate height across steps.");
        State.fill(40);
        Instance.GetStateBBuffer().Upload(State.data(), sizeof(State));
        S = Sample(false);
        Require(!S.bStateAB && Close(S.Values[2].y, 1) && Close(S.Values[2].z, 1.4F) && Close(S.Values[3].z, 0.04F),
                "Inspector must read buffer B and preserve cavity excess in Following Height.");
        Require(Close(Sample(false, 0, 0.02F).Values[3].z, 0.06F),
                "Height reference must affect only Surface Following height.");
        Require(Sample(true, 1).Values[2].w == 2, "Unsupported channel must be diagnosed without out-of-bounds reads.");
        // A changed selection/reference must discard already-submitted results.
        auto Command = Context.GetCommands().BeginSingleTime();
        Inspector.Record(Command, 1, SurfaceDescriptors, Selection, 0, 1, 0.01F, true, 60);
        Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
        Inspector.Invalidate();
        Inspector.CompleteFrame(1);
        Require(!Inspector.GetSnapshot(), "Invalidated pending snapshots must never repopulate the Inspector.");
        Parameters.AccumulationFactor = 0;
        Profiles.UpdateParameters(0, 0, Parameters);
        Require(Sample().Values[3].z == 0, "Zero accumulation factor must produce zero height despite positive State.");
        Parameters.AccumulationFactor = 0.15F;
        Profiles.UpdateParameters(0, 0, Parameters);
        // Same physical reference-area amount at a quarter of the original texel area.
        Instance.UpdateWorldTexelAreas(std::vector<float>(4, 0.5F * SurfaceStateReferenceArea));
        State.fill(2.5F);
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        Require(Close(Sample().Values[3].z, 0.0135F),
                "Changing texel area must not change thickness at equal reference-area amount.");
        State = {2.5F, 20.0F, 2.5F, 20.0F};
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        Require(Sample().Values[5].x < -0.02F, "Final normal must respond to the selected State height gradient.");
        std::array<float, 4> AfterInspection{};
        Instance.GetStateABuffer().Download(AfterInspection.data(), sizeof(AfterInspection));
        Require(AfterInspection == State, "Debug views and Inspector must leave State untouched.");
        const std::uint32_t Unsupported = 0, Supported = 1;
        Profiles.GetSupportedBuffer().Upload(&Unsupported, sizeof(Unsupported));
        Require(Sample().Values[2].w == 2, "Profile-defined unsupported State must be diagnosed.");
        Profiles.GetSupportedBuffer().Upload(&Supported, sizeof(Supported));
        Instance.UpdateWorldTexelAreas(std::vector<float>(4, 0));
        Require(Sample().Values[2].w == 4, "Degenerate world area must be diagnosed.");
        Instance.UpdateWorldTexelAreas(std::vector<float>(4, SurfaceStateReferenceArea));
        // A central sample must change the actual silhouette even with zero height on every boundary sample.
        TSharedSurfaceGeometryData GridGeometry({{0, {3, 3}}});
        for (std::uint32_t T = 0; T < 9; ++T)
        {
            auto& G = GridGeometry.GetTexels()[T];
            G.Surface = G.Triangle = G.Chart = 0;
            G.Position = {float(T % 3) * 0.5F - 0.5F, float(T / 3) * 0.5F - 0.5F, 0};
            std::size_t Slot = 0;
            for (std::uint32_t Other = 0; Other < 9; ++Other)
                if (Other != T && std::abs(int(Other % 3) - int(T % 3)) <= 1 &&
                    std::abs(int(Other / 3) - int(T / 3)) <= 1)
                    G.NeighborIndices[Slot++] = Other;
        }
        GridGeometry.SetProfileMap(std::vector<TSurfaceProfileIndex>(9, 0));
        TSurfaceSharedGeometryGPUResources GridShared(Context.GetPhysicalDevice(), Device, GridGeometry);
        TSurfaceInstanceGPUResources GridInstance(Context.GetPhysicalDevice(), Device, 9, 1,
            std::vector<float>(9 * SurfaceNeighborCount, 0), {}, std::vector<float>(9, SurfaceStateReferenceArea));
        TSurfaceStateDescriptorResources GridDescriptors(Device, GridShared, Profiles, GridInstance);
        TTexelGeometryPreview Preview(Context.GetPhysicalDevice(), Device, GridDescriptors.GetLayout(), 1);
        auto GridConfig = Config;
        GridConfig.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Debug/TexelGeometry.vert.spv";
        GridConfig.VertexBindings.clear();
        GridConfig.VertexAttributes.clear();
        GridConfig.DescriptorSetLayouts = {Handles.MaterialLayout, GridDescriptors.GetLayout(), Preview.GetOutputLayout()};
        TGraphicsPipeline GridPipeline(Device, Handles.Pass, GridConfig);
        TGPUBuffer VertexReadback(Context.GetPhysicalDevice(), Device, 9 * sizeof(TTexelGeometryVertex),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostMemory);
        Parameters.AccumulationFactor = 1;
        Parameters.CavityFillFactor = 0;
        Profiles.UpdateParameters(0, 0, Parameters);
        std::array<float, 9> GridState{};
        GridState[4] = 4;
        GridInstance.GetStateABuffer().Upload(GridState.data(), sizeof(GridState));
        auto BState = GridState;
        BState[4] = 1;
        GridInstance.GetStateBBuffer().Upload(BState.data(), sizeof(BState));
        TPush Side;
        Side.ViewProjection[2][0] = 3;
        Side.ViewProjection[3][2] = 0.2F;
        std::array<TTexelGeometryVertex, 9> Computed{};
        const auto RenderGrid = [&](bool bAB, bool bAccumulation, float Scale,
                                    TRenderViewMode Mode = TRenderViewMode::SurfaceFinalGeometry,
                                    std::uint32_t GridMode = 0, std::uint32_t BlockSize = 1)
        {
            TUniform Uniform;
            Uniform.RenderMode = static_cast<std::uint32_t>(Mode);
            Uniform.DebugOptions = {4, 0.5F, 0.1F, Scale};
            Uniform.DebugFlags.z = GridMode;
            Uniform.DebugFlags.w = BlockSize;
            UniformBuffer.Upload(&Uniform, sizeof(Uniform));
            const auto Command = Context.GetCommands().BeginSingleTime();
            Preview.Record(Command, 0, GridDescriptors, 9, 0, 1, 0.1F, Scale, bAB, bAccumulation);
            VkBufferMemoryBarrier CopyBarrier{};
            CopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            CopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            CopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            CopyBarrier.srcQueueFamilyIndex = CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            CopyBarrier.buffer = Preview.GetOutputBuffer(0).GetHandle();
            CopyBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 1, &CopyBarrier, 0, nullptr);
            const VkBufferCopy VertexCopy{0, 0, sizeof(Computed)};
            vkCmdCopyBuffer(Command, CopyBarrier.buffer, VertexReadback.GetHandle(), 1, &VertexCopy);
            VkClearValue Clear{};
            VkRenderPassBeginInfo Begin{};
            Begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            Begin.renderPass = Handles.Pass;
            Begin.framebuffer = Handles.Framebuffer;
            Begin.renderArea.extent = Extent;
            Begin.clearValueCount = 1;
            Begin.pClearValues = &Clear;
            vkCmdBeginRenderPass(Command, &Begin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(Command, VK_PIPELINE_BIND_POINT_GRAPHICS, GridPipeline.GetHandle());
            const VkViewport Viewport{0, 0, float(Extent.width), float(Extent.height), 0, 1};
            const VkRect2D Scissor{{0, 0}, Extent};
            vkCmdSetViewport(Command, 0, 1, &Viewport);
            vkCmdSetScissor(Command, 0, 1, &Scissor);
            const std::array<VkDescriptorSet, 3> Sets{MaterialSet,
                bAB ? GridDescriptors.GetABSet() : GridDescriptors.GetBASet(), Preview.GetOutputSet(0)};
            vkCmdBindDescriptorSets(Command, VK_PIPELINE_BIND_POINT_GRAPHICS, GridPipeline.GetLayout(),
                0, Sets.size(), Sets.data(), 0, nullptr);
            vkCmdPushConstants(Command, GridPipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Side), &Side);
            vkCmdBindIndexBuffer(Command, GridShared.GetTexelMeshIndexBuffer()->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            const auto Range = GridShared.GetTexelMeshRanges()[0];
            vkCmdDrawIndexed(Command, Range.IndexCount, 1, Range.FirstIndex, 0, 0);
            vkCmdEndRenderPass(Command);
            VkBufferImageCopy Copy{};
            Copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            Copy.imageExtent = {Extent.width, Extent.height, 1};
            vkCmdCopyImageToBuffer(Command, Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                Readback.GetHandle(), 1, &Copy);
            std::array<VkBufferMemoryBarrier, 2> HostBarriers{};
            for (std::size_t I = 0; I < HostBarriers.size(); ++I)
            {
                auto& Barrier = HostBarriers[I];
                Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                Barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                Barrier.buffer = I == 0 ? VertexReadback.GetHandle() : Readback.GetHandle();
                Barrier.size = VK_WHOLE_SIZE;
            }
            vkCmdPipelineBarrier(Command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                0, 0, nullptr, HostBarriers.size(), HostBarriers.data(), 0, nullptr);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            VertexReadback.Download(Computed.data(), sizeof(Computed));
            std::vector<glm::vec4> Pixels(Extent.width * Extent.height);
            Readback.Download(Pixels.data(), Pixels.size() * sizeof(glm::vec4));
            return Pixels;
        };
        const auto Flat = RenderGrid(true, false, 1);
        const auto Raised = RenderGrid(true, true, 1);
        Require(Flat[128 * Extent.width + 220].a == 0 && Raised[128 * Extent.width + 220].a > 0.5F,
                "An interior texel must raise the actual silhouette while all boundary samples stay flat.");
        Require(Close(Computed[4].PositionAndHeight.z, 0.4F) && Computed[0].PositionAndHeight.z == 0,
                "Compute must store per-texel positions from the latest State without resampling sparse mesh vertices.");
        const auto EdgeNormal = Computed[3].Normal;

        const auto GridOverlay = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 1);
        const auto GridOnly = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 2);
        const auto CoarseGrid = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 2, 2);
        std::size_t BrightLines = 0, CoarseBrightLines = 0, DarkInterior = 0;
        for (std::size_t P = 0; P < Raised.size(); ++P)
        {
            Require(Raised[P].a == GridOnly[P].a && Raised[P].a == GridOverlay[P].a,
                    "Grid visualization must preserve the displaced silhouette.");
            if (GridOnly[P].a > 0.5F)
            {
                BrightLines += GridOnly[P].b > 0.6F;
                CoarseBrightLines += CoarseGrid[P].b > 0.6F;
                DarkInterior += GridOnly[P].b < 0.1F;
            }
        }
        Require(BrightLines > 50 && DarkInterior > BrightLines && CoarseBrightLines < BrightLines,
                "Height grid must draw sparse texel cell boundaries and respond to block size.");
        const auto Reduced = RenderGrid(true, true, 0.5F);
        Require(Reduced[128 * Extent.width + 220].a == 0 && Close(Computed[4].PositionAndHeight.z, 0.2F),
                "Display scale must affect actual texel geometry.");
        Require(std::abs(EdgeNormal.x) > std::abs(Computed[3].Normal.x) + 0.05F,
                "Displayed normal must be reconstructed using the same display scale as the positions.");
        (void)RenderGrid(false, true, 1);
        Require(Close(Computed[4].PositionAndHeight.z, 0.1F), "Texel geometry must follow the current State B descriptor.");
        (void)RenderGrid(true, true, 1);
        (void)RenderGrid(true, true, 1);
        Require(Close(Computed[4].PositionAndHeight.z, 0.4F), "Repeated previews must not compound accumulation height.");
        const auto HeightMap = RenderGrid(true, true, 1, TRenderViewMode::SurfaceAccumulation);
        Require(HeightMap[128 * Extent.width + 220].a > 0.5F,
                "Accumulation heatmap must cover the same displaced texel geometry as Final Geometry.");
        std::array<float, 9> StateAfter{};
        GridInstance.GetStateABuffer().Download(StateAfter.data(), sizeof(StateAfter));
        Require(StateAfter == GridState, "Computed display geometry must leave simulation State unchanged.");
        GridState.fill(0);
        GridInstance.GetStateABuffer().Upload(GridState.data(), sizeof(GridState));
        (void)RenderGrid(true, true, 1);
        Require(Computed[4].PositionAndHeight.z == 0, "Reset State must discard the previous computed mound.");
        std::array<TSurfaceGPUGeometryScalar, 9> MesoRamp{};
        for (std::size_t T = 0; T < MesoRamp.size(); ++T)
            MesoRamp[T].MesoVirtualHeight = float(T % 3) * 0.1F;
        GridShared.GetGeometryScalarBuffer().Upload(MesoRamp.data(), sizeof(MesoRamp));
        Profiles.GetSupportedBuffer().Upload(&Unsupported, sizeof(Unsupported));
        (void)RenderGrid(true, true, 2);
        Require(Close(Computed[4].PositionAndHeight.z, 0.2F) && Computed[4].Normal.x < -0.35F,
                "Unsupported State must retain Meso geometry with a normal consistent with its display scale.");
    }
} // namespace MDSS::Tests
