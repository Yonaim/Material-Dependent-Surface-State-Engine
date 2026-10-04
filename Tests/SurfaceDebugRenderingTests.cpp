/**
 * @file SurfaceDebugRenderingTests.cpp
 * @brief 실제 fragment 출력으로 면적 비율과 격자의 해상도·축소 동작을 검증한다.
 */
#include "AssetManager/Assets/MeshSourceData.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "GPU/Vulkan/Pipeline/GraphicsPipeline.h"
#include "GPU/Vulkan/Resource/GPUImage.h"
#include "GPU/Vulkan/Resource/GPUImageView.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Rendering/AccumulationOverlaySides.h"
#include "Rendering/HeightFieldSmoothing.h"
#include "Rendering/Renderer.h"
#include "SurfaceState/Debug/TexelGeometryPreview.h"
#include "SurfaceState/Debug/TexelInspector.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace MDSS::Tests
{
    using namespace MDSS::GPU;
    using namespace MDSS::Rendering;
    using namespace MDSS::Asset;
    using namespace MDSS::SurfaceState;
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
            glm::vec4     DebugOptions{4.0F, 0.01F, 1.0F, 1.0F};
            glm::uvec4    DebugFlags{0};
            glm::uvec4    DemoStateChannels{
                SurfaceState::InvalidStateId, SurfaceState::InvalidStateId, SurfaceState::InvalidStateId, 1U};
            glm::vec4  DemoOptions{0.65F, 0.16F, 0.48F, 1.0F};
            glm::vec4  DemoEffectOptions{1.0F, 1.0F, 1.0F, 0.16F};
            glm::vec4  WetnessTint{0.44F, 0.56F, 0.68F, 1.0F};
            glm::vec4  WaterFilmTint{0.35F, 0.53F, 0.68F, 1.0F};
            glm::uvec4 DemoExtraStateChannels{SurfaceState::InvalidStateId, 0U, 0U, 0U};
            glm::vec4  CameraPosition{-0.35F, -0.55F, 1.0F, 1.0F};
        };
        static_assert(sizeof(TUniform) == 192);

        struct TPush
        {
            glm::mat4 Model{1.0F};
            glm::mat4 ViewProjection{1.0F};
        };
    }

    void TestOverlaySideCompaction(const GPU::TVulkanContext& Context)
    {
        const VkDevice Device = Context.GetDevice();
        const auto     HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}, {1, {2, 2}}});
        for (std::uint32_t Texel = 0; Texel < 8; ++Texel)
        {
            auto& G = Geometry.GetTexels()[Texel];
            G.Surface = Texel / 4;
            G.Triangle = G.Chart = 0;
            G.Position = {float(Texel % 2) + 2.0F * float(Texel / 4), float((Texel / 2) % 2), 0};
            for (std::uint32_t Other = 0, Slot = 0; Other < 4; ++Other)
                if (Other != Texel % 4)
                    G.NeighborIndices[Slot++] = 4 * (Texel / 4) + Other;
        }
        Geometry.SetProfileMap(std::vector<SurfaceState::TSurfaceProfileIndex>(8, 0));
        SurfaceState::TSurfaceResponseProfileData Profile;
        auto                                      Parameters = SurfaceState::TSurfaceStateParameters{};
        Parameters.AccumulationFactor = 1.0F;
        Profile.States.emplace("test", Parameters);
        const SurfaceState::TSurfaceStateRegistry        Registry({Profile});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(Context.GetPhysicalDevice(), Device, Geometry);
        SurfaceState::TSurfaceProfileGPUResources  Profiles(Context.GetPhysicalDevice(), Device, {Profile}, Registry);
        SurfaceState::TSurfaceInstanceGPUResources Instance(Context.GetPhysicalDevice(),
                                                            Device,
                                                            8,
                                                            1,
                                                            std::vector<float>(8 * SurfaceNeighborCount, 0.0F),
                                                            {},
                                                            std::vector<float>(8, SurfaceStateReferenceArea));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Device, Shared, Profiles, Instance);
        TTexelGeometryPreview     Preview(Context.GetPhysicalDevice(), Device, Descriptors.GetLayout(), 1, true);
        TAccumulationOverlaySides Sides(
            Context.GetPhysicalDevice(), Device, Descriptors.GetLayout(), Preview.GetOutputLayout(), 1);
        std::array<VkDrawIndirectCommand, 3> Draws{};
        GPU::TGPUBuffer                      Readback(
            Context.GetPhysicalDevice(), Device, sizeof(Draws), VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostMemory);
        std::array<VkDrawIndexedIndirectCommand, 2> TopDraws{};
        GPU::TGPUBuffer                             TopReadback(
            Context.GetPhysicalDevice(), Device, sizeof(TopDraws), VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostMemory);
        std::array<TTexelGeometryVertex, 8>          Computed{};
        TAccumulationOverlaySides::TTriangleActivity Activity{};
        GPU::TGPUBuffer                              GeometryReadback(
            Context.GetPhysicalDevice(), Device, sizeof(Computed), VK_BUFFER_USAGE_TRANSFER_DST_BIT, HostMemory);
        const auto Run = [&](const std::array<float, 8>& State)
        {
            Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
            auto Command = Context.GetCommands().BeginSingleTime();
            Preview.Record(Command, 0, Descriptors, 8, 0, 1, 1.0F, 1.0F, glm::mat4(1.0F), true, true);
            Sides.Record(Command, 0, 0, 1, Shared, Descriptors, Preview.GetOutputSet(0), true);
            VkBufferMemoryBarrier CopyBarrier{};
            CopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            CopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            CopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            CopyBarrier.srcQueueFamilyIndex = CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            CopyBarrier.buffer = Sides.GetDrawBuffer(0, 0);
            CopyBarrier.size = sizeof(Draws);
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &CopyBarrier,
                                 0,
                                 nullptr);
            VkBufferCopy Copy{0, 0, sizeof(Draws)};
            vkCmdCopyBuffer(Command, CopyBarrier.buffer, Readback.GetHandle(), 1, &Copy);
            CopyBarrier.buffer = Sides.GetTopDrawBuffer(0, 0);
            CopyBarrier.size = sizeof(TopDraws);
            CopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &CopyBarrier,
                                 0,
                                 nullptr);
            Copy.size = sizeof(TopDraws);
            vkCmdCopyBuffer(Command, CopyBarrier.buffer, TopReadback.GetHandle(), 1, &Copy);
            CopyBarrier.buffer = Preview.GetOutputBuffer(0).GetHandle();
            CopyBarrier.size = sizeof(Computed);
            CopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &CopyBarrier,
                                 0,
                                 nullptr);
            Copy.size = sizeof(Computed);
            vkCmdCopyBuffer(Command, CopyBarrier.buffer, GeometryReadback.GetHandle(), 1, &Copy);
            std::array<VkBufferMemoryBarrier, 3> HostBarriers{};
            for (auto& HostBarrier : HostBarriers)
            {
                HostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                HostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                HostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                HostBarrier.srcQueueFamilyIndex = HostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                HostBarrier.size = VK_WHOLE_SIZE;
            }
            HostBarriers[0].buffer = Readback.GetHandle();
            HostBarriers[1].buffer = TopReadback.GetHandle();
            HostBarriers[2].buffer = GeometryReadback.GetHandle();
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 static_cast<std::uint32_t>(HostBarriers.size()),
                                 HostBarriers.data(),
                                 0,
                                 nullptr);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            Activity = Sides.CompleteFrame(0);
            Readback.Download(Draws.data(), sizeof(Draws));
            TopReadback.Download(TopDraws.data(), sizeof(TopDraws));
            GeometryReadback.Download(Computed.data(), sizeof(Computed));
        };
        Run({});
        Require(TopDraws[0].instanceCount == 0 && TopDraws[1].instanceCount == 0,
                "An empty overlay must skip all top draws.");
        const auto UncachedFlat = Computed;
        Run({});
        for (std::size_t T = 0; T < Computed.size(); ++T)
            Require(glm::length(Computed[T].HeightAndNormal - UncachedFlat[T].HeightAndNormal) < 1e-5F,
                    "Cached meso normals must match the uncached height-field fit.");
        Run({0, 1.0F, 0, 0, 0, 0, 0, 0});
        Require(Activity.Total == Shared.GetTexelMeshIndexBuffer()->GetSize() / (3U * sizeof(std::uint32_t)) &&
                    Activity.Active > 0 && Activity.Active <= Activity.Total,
                "Overlay activity must count covered top triangles without exceeding the mesh total.");
        Require(TopDraws[0].instanceCount == 1 && TopDraws[1].instanceCount == 1 &&
                    TopDraws[0].indexCount == Shared.GetTexelMeshRanges()[0].IndexCount,
                "A populated overlay must restore the full top draw commands.");
        Require(Draws[0].vertexCount == 6 && Draws[0].firstVertex == 0,
                "Only the boundary-crossing triangle may enter the active surface draw.");
        Require(Draws[1].vertexCount == 0 && Draws[1].firstVertex == 12 && Draws[2].vertexCount == 12 &&
                    Draws[2].firstVertex == 24,
                "Inactive surface must not draw sides; two active open edges must remain.");
        Require(std::abs(Computed[1].HeightAndNormal.x - 0.01F) < 1e-6F &&
                    std::abs(Computed[1].HeightAndNormal.y) > 0.005F,
                "Cached accumulation height must displace the active texel and affect its normal.");
        Run({});
        Require(Activity.Active == 0 && Activity.Total > 0,
                "Overlay activity must reset when all accumulation disappears.");
        Require(TopDraws[0].instanceCount == 0 && TopDraws[1].instanceCount == 0,
                "Top draws must turn off again when the overlay becomes empty.");
        Require(Draws[0].vertexCount == 0 && Draws[1].vertexCount == 0 && Draws[2].vertexCount == 0,
                "Indirect side counts must reset when all accumulation disappears.");
        Require(Computed[1].HeightAndNormal.x == 0.0F,
                "Cached render height must refresh after accumulation disappears.");
        std::array<SurfaceState::TSurfaceGPUGeometryScalar, 8> MesoRamp{};
        MesoRamp[1].MesoVirtualHeight = 0.05F;
        Shared.GetGeometryScalarBuffer().Upload(MesoRamp.data(), sizeof(MesoRamp));
        Run({});
        const auto UpdatedMeso = Computed;
        Run({});
        Require(std::abs(Computed[1].HeightAndNormal.x - 0.05F) < 1e-6F,
                "Changing meso height must invalidate the render baseline.");
        for (std::size_t T = 0; T < Computed.size(); ++T)
            Require(glm::length(Computed[T].HeightAndNormal - UpdatedMeso[T].HeightAndNormal) < 1e-5F,
                    "Rebuilt meso normals must match the uncached result after a height update.");
    }

    void TestCavityFillDisplayBound(const GPU::TVulkanContext& Context)
    {
        const VkDevice             Device = Context.GetDevice();
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}});
        for (std::uint32_t Texel = 0; Texel < 4; ++Texel)
        {
            auto& G = Geometry.GetTexels()[Texel];
            G.Surface = G.Triangle = G.Chart = 0;
            G.Position = {float(Texel % 2), float(Texel / 2), 0.0F};
            G.Geometry.MesoVirtualHeight = -0.02F;
            for (std::uint32_t Other = 0, Slot = 0; Other < 4; ++Other)
                if (Other != Texel)
                    G.NeighborIndices[Slot++] = Other;
        }
        Geometry.SetProfileMap(std::vector<SurfaceState::TSurfaceProfileIndex>(4, 0));
        SurfaceState::TSurfaceStateParameters Parameters;
        Parameters.StateCapacity = 2.0F;
        Parameters.AccumulationFactor = 0.15F;
        Parameters.CavityFillFactor = 1.0F;
        Parameters.ThicknessPerAmount = 0.01F;
        SurfaceState::TSurfaceResponseProfileData Profile;
        Profile.States.emplace("mud", Parameters);
        const SurfaceState::TSurfaceStateRegistry        Registry({Profile});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(Context.GetPhysicalDevice(), Device, Geometry);
        SurfaceState::TSurfaceProfileGPUResources  Profiles(Context.GetPhysicalDevice(), Device, {Profile}, Registry);
        SurfaceState::TSurfaceInstanceGPUResources Instance(Context.GetPhysicalDevice(),
                                                            Device,
                                                            4,
                                                            1,
                                                            std::vector<float>(4 * SurfaceNeighborCount, 0.0F),
                                                            {},
                                                            std::vector<float>(4, SurfaceStateReferenceArea));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Device, Shared, Profiles, Instance);
        TTexelInspector Inspector(Context.GetPhysicalDevice(), Device, Descriptors.GetLayout(), 1);
        const SurfaceState::TSurfaceTexelSelection Selection{0, 0, 0, 0, {0, 0}, "mud"};
        const auto                                 Sample = [&](float DisplayScale)
        {
            auto Command = Context.GetCommands().BeginSingleTime();
            Inspector.Record(Command, 0, Descriptors, Selection, 0, 1, DisplayScale, glm::mat4(1.0F), true, 1);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            Inspector.CompleteFrame(0);
            Require(Inspector.GetSnapshot().has_value(), "Cavity fill Inspector snapshot must complete.");
            return *Inspector.GetSnapshot();
        };
        const auto           Close = [](float A, float B) { return std::abs(A - B) < 1e-6F; };
        std::array<float, 4> State{2.0F, 2.0F, 2.0F, 2.0F};
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        const auto Zero = Sample(0.0F);
        Require(Close(Zero.Values[3].x, 0.0F) && Close(Zero.Values[3].w, -0.02F),
                "Zero display scale must remove State height while preserving the Meso cavity.");
        const auto Unit = Sample(1.0F);
        Require(Close(Unit.Values[2].y, 0.3F) && Close(Unit.Values[3].x, 0.006F),
                "Unit display scale must preserve the designed cavity fill fraction.");
        const auto Amplified = Sample(4.0F);
        Require(Close(Amplified.Values[3].x, 0.02F) && Close(Amplified.Values[3].w, 0.0F),
                "Display exaggeration must not lift cavity fill above the macro surface.");
        Require(Close(Sample(10.0F).Values[3].x, 0.02F),
                "Further display exaggeration must leave cavity fill capped at its depth.");
        State.fill(8.0F);
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        const auto AboveCapacity = Sample(4.0F);
        Require(Close(AboveCapacity.Values[0].x, 8.0F) && Close(AboveCapacity.Values[3].x, Amplified.Values[3].x),
                "State above capacity must remain stored without increasing cavity geometry.");
        Parameters.CavityFillFactor = 0.8F;
        Profiles.UpdateParameters(0, 0, Parameters);
        const auto Mixed = Sample(4.0F);
        Require(Mixed.Values[3].x <= Mixed.Values[2].x + 1e-6F && Close(Mixed.Values[3].y, 0.0024F),
                "Cavity fill must stay bounded while designed surface-following thickness remains independent.");

        // Smoothing must blend fill fractions, not transfer a deep neighbor's absolute
        // cavity height into a shallow texel and lift it above the macro surface.
        Parameters.CavityFillFactor = 1.0F;
        Parameters.ThicknessPerAmount = 0.0F;
        Profiles.UpdateParameters(0, 0, Parameters);
        State.fill(2.0F);
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        std::array<SurfaceState::TSurfaceGPUGeometryScalar, 4> Scalars{};
        Scalars[0].MesoVirtualHeight = -0.001F;
        for (std::size_t I = 1; I < Scalars.size(); ++I)
            Scalars[I].MesoVirtualHeight = -0.02F;
        Shared.GetGeometryScalarBuffer().Upload(Scalars.data(), sizeof(Scalars));
        TTexelGeometryPreview            Preview(Context.GetPhysicalDevice(), Device, Descriptors.GetLayout(), 1, true);
        Rendering::THeightFieldSmoothing Smoothing(
            Context.GetPhysicalDevice(), Device, Descriptors.GetLayout(), Preview.GetOutputLayout(), 1);
        std::array<TTexelGeometryVertex, 4> Smoothed{};
        GPU::TGPUBuffer                     Readback(Context.GetPhysicalDevice(),
                                 Device,
                                 sizeof(Smoothed),
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        auto                                Command = Context.GetCommands().BeginSingleTime();
        Preview.Record(Command, 0, Descriptors, 4, 0, 1, 4.0F, 1.0F, glm::mat4(1.0F), true, true);
        Smoothing.Record(
            Command, 0, 0, 1, 4, Descriptors, Preview.GetOutputSet(0), Preview.GetOutputBuffer(0), true, 4.0F);
        VkBufferMemoryBarrier CopyBarrier{};
        CopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        CopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        CopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        CopyBarrier.srcQueueFamilyIndex = CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        CopyBarrier.buffer = Smoothing.GetOutputBuffer(0, 0).GetHandle();
        CopyBarrier.size = sizeof(Smoothed);
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &CopyBarrier,
                             0,
                             nullptr);
        const VkBufferCopy Copy{0, 0, sizeof(Smoothed)};
        vkCmdCopyBuffer(Command, CopyBarrier.buffer, Readback.GetHandle(), 1, &Copy);
        VkBufferMemoryBarrier HostBarrier{};
        HostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        HostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        HostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        HostBarrier.srcQueueFamilyIndex = HostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        HostBarrier.buffer = Readback.GetHandle();
        HostBarrier.size = sizeof(Smoothed);
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &HostBarrier,
                             0,
                             nullptr);
        Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
        Readback.Download(Smoothed.data(), sizeof(Smoothed));
        Require(Smoothed[0].HeightAndNormal.x <= 1e-6F && Smoothed[0].HeightAndNormal.x >= -0.001F - 1e-6F,
                "Smoothing must not overfill a shallow cavity using taller neighboring cavities.");
    }

    void TestSurfaceDebugRendering(const GPU::TVulkanContext& Context)
    {
        const VkDevice       Device = Context.GetDevice();
        constexpr VkExtent2D Extent{256, 256};
        constexpr VkFormat   Format = VK_FORMAT_R32G32B32A32_SFLOAT;
        constexpr auto       HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        GPU::TGPUImage       Image(Context.GetPhysicalDevice(),
                             Device,
                             Extent,
                             Format,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        GPU::TGPUImageView   View(Device, Image.GetHandle(), Format, VK_IMAGE_ASPECT_COLOR_BIT);
        GPU::TGPUBuffer      Readback(Context.GetPhysicalDevice(),
                                 Device,
                                 Extent.width * Extent.height * sizeof(glm::vec4),
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 HostMemory);
        GPU::TGPUBuffer      UniformBuffer(
            Context.GetPhysicalDevice(), Device, sizeof(TUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, HostMemory);
        GPU::TGPUBuffer Vertices(
            Context.GetPhysicalDevice(), Device, 6 * sizeof(TVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, HostMemory);

        // Deliberately use invalid simulation texels: geometric diagnostics must still show the mesh.
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}});
        Geometry.SetProfileMap(
            std::vector<SurfaceState::TSurfaceProfileIndex>(4, SurfaceState::InvalidSurfaceProfileIndex));
        SurfaceState::TSurfaceResponseProfileData Profile;
        Profile.States.emplace("test", SurfaceState::TSurfaceStateParameters{});
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(Context.GetPhysicalDevice(), Device, Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(Context.GetPhysicalDevice(), Device, ProfileTable, Registry);
        SurfaceState::TSurfaceInstanceGPUResources Instance(
            Context.GetPhysicalDevice(), Device, 4, 1, std::vector<float>(4 * SurfaceNeighborCount, 0.0F));
        SurfaceState::TSurfaceStateDescriptorResources SurfaceDescriptors(Device, Shared, Profiles, Instance);
        Asset::TextureAsset WhiteTexture(0, "white", {}, Context, 1, 1, {255, 255, 255, 255}, VK_FORMAT_R8G8B8A8_UNORM);
        Asset::TextureAsset FlatNormalTexture(
            1, "flat", {}, Context, 1, 1, {128, 128, 255, 255}, VK_FORMAT_R8G8B8A8_UNORM);
        TRenderHandles Handles{Device};

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

        const std::array<VkDescriptorSetLayoutBinding, 3> Bindings{
            {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
             {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
             {2,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              1,
              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
              nullptr}}};
        VkDescriptorSetLayoutCreateInfo LayoutInfo{};
        LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        LayoutInfo.bindingCount = Bindings.size();
        LayoutInfo.pBindings = Bindings.data();
        RequireVk(vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &Handles.MaterialLayout));
        const std::array<VkDescriptorPoolSize, 2> PoolSizes{
            {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}}};
        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.maxSets = 1;
        PoolInfo.poolSizeCount = PoolSizes.size();
        PoolInfo.pPoolSizes = PoolSizes.data();
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
        const std::array<VkDescriptorImageInfo, 2> Images{
            {{WhiteTexture.GetSampler(), WhiteTexture.GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
             {FlatNormalTexture.GetSampler(),
              FlatNormalTexture.GetImageView(),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};
        for (std::uint32_t I = 0; I < Images.size(); ++I)
        {
            Write.dstBinding = I;
            Write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            Write.pBufferInfo = nullptr;
            Write.pImageInfo = &Images[I];
            vkUpdateDescriptorSets(Device, 1, &Write, 0, nullptr);
        }

        GPU::TGraphicsPipelineConfig Config;
        Config.ShaderStages = {
            {VK_SHADER_STAGE_VERTEX_BIT, std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.vert.spv"},
            {VK_SHADER_STAGE_FRAGMENT_BIT, std::string(MDSS_SHADER_DIR) + "/Debug/SurfaceDebug.frag.spv"}};
        Config.CullMode = VK_CULL_MODE_NONE;
        Config.VertexBindings = {{0, sizeof(TVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
        Config.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TVertex, Position)},
                                   {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TVertex, Normal)},
                                   {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TVertex, UV)},
                                   {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(TVertex, Tangent)}};
        Config.DescriptorSetLayouts = {Handles.MaterialLayout, SurfaceDescriptors.GetLayout()};
        Config.PushConstantRanges = {{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TPush)}};
        GPU::TGraphicsPipeline Pipeline(Device, Handles.Pass, Config);

        TUniform DebugControls;
        auto     Render = [&](TRenderViewMode Mode,
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
            vkCmdPushConstants(Command,
                               Pipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(Push),
                               &Push);
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
        std::array<SurfaceState::TSurfaceGPUGeometryScalar, 4> Scalars{};
        for (auto& Scalar : Scalars)
            Scalar.MesoVirtualHeight = -0.02F;
        Shared.GetGeometryScalarBuffer().Upload(Scalars.data(), sizeof(Scalars));
        std::array<SurfaceState::TSurfaceGPUVec4, 4>            Normals{}, Positions{};
        std::array<SurfaceState::TSurfaceGPUNeighborIndices, 4> Neighbors{};
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
        auto Parameters = SurfaceState::TSurfaceStateParameters{};
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

        TTexelInspector Inspector(Context.GetPhysicalDevice(), Device, SurfaceDescriptors.GetLayout(), 2);
        const SurfaceState::TSurfaceTexelSelection Selection{0, 0, 0, 0, {0, 0}, "test"};
        const auto                                 Sample =
            [&](bool bAB = true, std::uint32_t Channel = 0, float DisplayScale = 1.0F, std::uint64_t Step = 1)
        {
            auto Command = Context.GetCommands().BeginSingleTime();
            Inspector.Record(
                Command, 0, SurfaceDescriptors, Selection, Channel, 1, DisplayScale, glm::mat4(1.0F), bAB, Step);
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
        Require(Close(Sample(true, 0, 1.0F, 50).Values[3].z, S.Values[3].z),
                "Unchanged State must not accumulate height across steps.");
        State.fill(40);
        Instance.GetStateBBuffer().Upload(State.data(), sizeof(State));
        S = Sample(false);
        Require(!S.bStateAB && Close(S.Values[2].y, 1) && Close(S.Values[2].z, 1.4F) && Close(S.Values[3].z, 0.04F),
                "Inspector must read buffer B and preserve cavity excess in Following Height.");
        Require(Close(Sample(false, 0, 2.0F).Values[3].z, 0.08F),
                "Display scale must affect the visible cavity and following heights.");
        Require(Sample(true, 1).Values[2].w == 2, "Unsupported channel must be diagnosed without out-of-bounds reads.");
        // A changed selection/reference must discard already-submitted results.
        auto Command = Context.GetCommands().BeginSingleTime();
        Inspector.Record(Command, 1, SurfaceDescriptors, Selection, 0, 1, 1.0F, glm::mat4(1.0F), true, 60);
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
        GridGeometry.SetProfileMap(std::vector<SurfaceState::TSurfaceProfileIndex>(9, 0));
        SurfaceState::TSurfaceSharedGeometryGPUResources GridShared(Context.GetPhysicalDevice(), Device, GridGeometry);
        SurfaceState::TSurfaceInstanceGPUResources       GridInstance(Context.GetPhysicalDevice(),
                                                                Device,
                                                                9,
                                                                1,
                                                                std::vector<float>(9 * SurfaceNeighborCount, 0),
                                                                      {},
                                                                std::vector<float>(9, SurfaceStateReferenceArea));
        SurfaceState::TSurfaceStateDescriptorResources   GridDescriptors(Device, GridShared, Profiles, GridInstance);
        TTexelGeometryPreview Preview(Context.GetPhysicalDevice(), Device, GridDescriptors.GetLayout(), 1);
        auto                  GridConfig = Config;
        GridConfig.ShaderStages[0].ShaderPath = std::string(MDSS_SHADER_DIR) + "/Debug/TexelGeometry.vert.spv";
        using V = SurfaceState::TSurfaceTexelMeshVertex;
        GridConfig.VertexBindings = {{0, sizeof(V), VK_VERTEX_INPUT_RATE_VERTEX}};
        GridConfig.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Position)},
                                       {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, Normal)},
                                       {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, UVSurface)},
                                       {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(V, DisplacementNormal)},
                                       {4, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(V, Samples)},
                                       {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(V, Weights)}};
        GridConfig.DescriptorSetLayouts = {
            Handles.MaterialLayout, GridDescriptors.GetLayout(), Preview.GetOutputLayout()};
        GPU::TGraphicsPipeline GridPipeline(Device, Handles.Pass, GridConfig);
        // Insert an unrelated State before the demo names: rendering must use resolved IDs, not fixed slots.
        SurfaceState::TSurfaceResponseProfileData LitProfile;
        SurfaceState::TSurfaceStateParameters     LitParameters;
        LitParameters.AccumulationFactor = 1;
        LitParameters.CavityFillFactor = 0;
        LitParameters.ThicknessPerAmount = 0.1F;
        LitProfile.States.emplace("aaa", LitParameters);
        LitProfile.States.emplace("mud", LitParameters);
        LitProfile.States.emplace("wetness", LitParameters);
        const SurfaceState::TSurfaceStateRegistry LitRegistry({LitProfile});
        const auto                                LitBindings = ResolveDemoSurfaceStates(LitRegistry);
        Require(LitBindings.Mud == 1 && LitBindings.Wetness == 2,
                "Demo bindings must follow dynamically assigned Registry IDs.");
        SurfaceState::TSurfaceProfileGPUResources LitProfiles(
            Context.GetPhysicalDevice(), Device, {LitProfile}, LitRegistry);
        SurfaceState::TSurfaceInstanceGPUResources     LitInstance(Context.GetPhysicalDevice(),
                                                               Device,
                                                               9,
                                                               3,
                                                               std::vector<float>(9 * SurfaceNeighborCount, 0),
                                                                   {},
                                                               std::vector<float>(9, SurfaceStateReferenceArea));
        SurfaceState::TSurfaceStateDescriptorResources LitDescriptors(Device, GridShared, LitProfiles, LitInstance);
        auto                                           LitConfig = GridConfig;
        LitConfig.ShaderStages[0].ShaderPath =
            std::string(MDSS_SHADER_DIR) + "/Rendering/Surface/TexelSurfaceLit.vert.spv";
        LitConfig.ShaderStages[1].ShaderPath =
            std::string(MDSS_SHADER_DIR) + "/Rendering/Surface/TexelSurfaceLit.frag.spv";
        GPU::TGraphicsPipeline LitPipeline(Device, Handles.Pass, LitConfig);

        GPU::TGPUBuffer VertexReadback(Context.GetPhysicalDevice(),
                                       Device,
                                       9 * sizeof(TTexelGeometryVertex),
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       HostMemory);
        Parameters.AccumulationFactor = 1;
        Parameters.CavityFillFactor = 0;
        Parameters.ThicknessPerAmount = 0.1F;
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
        const auto                          RenderGrid = [&](bool            bAB,
                                    bool            bAccumulation,
                                    float           Scale,
                                    TRenderViewMode Mode = TRenderViewMode::SurfaceFinalGeometry,
                                    std::uint32_t   GridMode = 0,
                                    std::uint32_t   BlockSize = 1,
                                    const TUniform* LitUniform = nullptr,
                                    const SurfaceState::TSurfaceSharedGeometryGPUResources* MeshOverride = nullptr)
        {
            TUniform Uniform = LitUniform ? *LitUniform : TUniform{};
            Uniform.RenderMode = static_cast<std::uint32_t>(Mode);
            if (!LitUniform)
                Uniform.DebugOptions = {4, 0.5F, 1.0F, Scale};
            Uniform.DebugFlags.z = GridMode;
            Uniform.DebugFlags.w = BlockSize;
            UniformBuffer.Upload(&Uniform, sizeof(Uniform));
            const auto  Command = Context.GetCommands().BeginSingleTime();
            const auto& Descriptors = LitUniform ? LitDescriptors : GridDescriptors;
            const auto& Pipeline = LitUniform ? LitPipeline : GridPipeline;
            Preview.Record(Command,
                           0,
                           Descriptors,
                           9,
                           LitUniform ? LitBindings.Mud : 0,
                           LitUniform ? 3 : 1,
                           LitUniform ? Uniform.DemoOptions.w : 1.0F,
                           Scale,
                           glm::mat4(1.0F),
                           bAB,
                           bAccumulation);
            VkBufferMemoryBarrier CopyBarrier{};
            CopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            CopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            CopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            CopyBarrier.srcQueueFamilyIndex = CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            CopyBarrier.buffer = Preview.GetOutputBuffer(0).GetHandle();
            CopyBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 1,
                                 &CopyBarrier,
                                 0,
                                 nullptr);
            const VkBufferCopy VertexCopy{0, 0, sizeof(Computed)};
            vkCmdCopyBuffer(Command, CopyBarrier.buffer, VertexReadback.GetHandle(), 1, &VertexCopy);
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
            const std::array<VkDescriptorSet, 3> Sets{
                MaterialSet, bAB ? Descriptors.GetABSet() : Descriptors.GetBASet(), Preview.GetOutputSet(0)};
            vkCmdBindDescriptorSets(Command,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    Pipeline.GetLayout(),
                                    0,
                                    Sets.size(),
                                    Sets.data(),
                                    0,
                                    nullptr);
            vkCmdPushConstants(Command,
                               Pipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(Side),
                               &Side);
            const auto&        RenderMesh = MeshOverride ? *MeshOverride : GridShared;
            const VkBuffer     GridVertices = RenderMesh.GetTexelMeshVertexBuffer()->GetHandle();
            const VkDeviceSize GridOffset = 0;
            vkCmdBindVertexBuffers(Command, 0, 1, &GridVertices, &GridOffset);
            vkCmdBindIndexBuffer(Command, RenderMesh.GetTexelMeshIndexBuffer()->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            const auto Range = RenderMesh.GetTexelMeshRanges()[0];
            vkCmdDrawIndexed(Command, Range.IndexCount, 1, Range.FirstIndex, 0, 0);
            vkCmdEndRenderPass(Command);
            VkBufferImageCopy Copy{};
            Copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            Copy.imageExtent = {Extent.width, Extent.height, 1};
            vkCmdCopyImageToBuffer(
                Command, Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Readback.GetHandle(), 1, &Copy);
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
            vkCmdPipelineBarrier(Command,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 HostBarriers.size(),
                                 HostBarriers.data(),
                                 0,
                                 nullptr);
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
        Require(Close(Computed[4].HeightAndNormal.x, 0.4F) && Computed[0].HeightAndNormal.x == 0,
                "Compute must store per-texel heights from the latest State without resampling sparse mesh vertices.");
        // Refine a UV-seamed source quad. The center sample lies exactly on its shared edge,
        // and must raise that edge in both charts without opening a crack.
        std::vector<TVertex>           SeamVertices(6);
        const std::array<glm::vec3, 6> SeamPositions{{{-0.5F, -0.5F, 0},
                                                      {0.5F, -0.5F, 0},
                                                      {0.5F, 0.5F, 0},
                                                      {-0.5F, -0.5F, 0},
                                                      {0.5F, 0.5F, 0},
                                                      {-0.5F, 0.5F, 0}}};
        const std::array<glm::vec2, 6> SeamUVs{
            {{0.05F, 0.05F}, {0.45F, 0.05F}, {0.45F, 0.45F}, {0.55F, 0.55F}, {0.95F, 0.95F}, {0.55F, 0.95F}}};
        for (std::size_t K = 0; K < 6; ++K)
        {
            SeamVertices[K].Position = SeamPositions[K];
            SeamVertices[K].UV = SeamUVs[K];
        }
        const std::vector<TMeshTriangleSource> SeamTriangles{{{0, 1, 2}, {0, 1, 2}, {0, 1, 2}, 0},
                                                             {{3, 4, 5}, {0, 2, 3}, {3, 4, 5}, 0}};
        auto                                   SeamGeometry = GridGeometry;
        for (std::uint32_t T = 0; T < 9; ++T)
        {
            const float X = float(T % 3) * 0.5F, Y = float(T / 3) * 0.5F;
            auto&       G = SeamGeometry.GetTexels()[T];
            G.Triangle = G.Chart = Y <= X ? 0 : 1;
            G.Barycentric = Y <= X ? glm::vec3(1 - X, X - Y, Y) : glm::vec3(1 - Y, X, Y - X);
        }
        SurfaceState::TSurfaceSharedGeometryGPUResources SeamShared(
            Context.GetPhysicalDevice(), Device, SeamGeometry, {}, SeamVertices, SeamTriangles);
        const auto SeamFlat =
            RenderGrid(true, false, 1, TRenderViewMode::SurfaceFinalGeometry, 0, 1, nullptr, &SeamShared);
        const auto SeamRaised =
            RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 0, 1, nullptr, &SeamShared);
        Require(SeamFlat[128 * Extent.width + 220].a == 0 && SeamRaised[128 * Extent.width + 220].a > 0.5F,
                "Source-edge texel heights must survive seam welding and raise the actual GPU silhouette.");
        Require(std::count_if(SeamFlat.begin(), SeamFlat.end(), [](auto P) { return P.a > 0.5F; }) ==
                    std::count_if(Flat.begin(), Flat.end(), [](auto P) { return P.a > 0.5F; }),
                "A flat source-seamed quad must have complete GPU coverage without chart border gaps.");
        const float EdgeNormalX = Computed[3].HeightAndNormal.y;

        const auto  GridOverlay = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 1);
        const auto  GridOnly = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 2);
        const auto  CoarseGrid = RenderGrid(true, true, 1, TRenderViewMode::SurfaceFinalGeometry, 2, 2);
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
        Require(Reduced[128 * Extent.width + 220].a == 0 && Close(Computed[4].HeightAndNormal.x, 0.2F),
                "Display scale must affect actual texel geometry.");
        Require(std::abs(EdgeNormalX) > std::abs(Computed[3].HeightAndNormal.y) + 0.05F,
                "Displayed normal must be reconstructed using the same display scale as the positions.");
        (void)RenderGrid(false, true, 1);
        Require(Close(Computed[4].HeightAndNormal.x, 0.1F),
                "Texel geometry must follow the current State B descriptor.");
        (void)RenderGrid(true, true, 1);
        (void)RenderGrid(true, true, 1);
        Require(Close(Computed[4].HeightAndNormal.x, 0.4F), "Repeated previews must not compound accumulation height.");
        const auto HeightMap = RenderGrid(true, true, 1, TRenderViewMode::SurfaceAccumulation);
        Require(HeightMap[128 * Extent.width + 220].a > 0.5F,
                "Accumulation heatmap must cover the same displaced texel geometry as Final Geometry.");
        std::array<float, 9> StateAfter{};
        GridInstance.GetStateABuffer().Download(StateAfter.data(), sizeof(StateAfter));
        Require(StateAfter == GridState, "Computed display geometry must leave simulation State unchanged.");
        GridState.fill(0);
        GridInstance.GetStateABuffer().Upload(GridState.data(), sizeof(GridState));
        (void)RenderGrid(true, true, 1);
        Require(Computed[4].HeightAndNormal.x == 0, "Reset State must discard the previous computed mound.");
        std::array<SurfaceState::TSurfaceGPUGeometryScalar, 9> MesoRamp{};
        for (std::size_t T = 0; T < MesoRamp.size(); ++T)
            MesoRamp[T].MesoVirtualHeight = float(T % 3) * 0.1F;
        GridShared.GetGeometryScalarBuffer().Upload(MesoRamp.data(), sizeof(MesoRamp));
        Profiles.GetSupportedBuffer().Upload(&Unsupported, sizeof(Unsupported));
        (void)RenderGrid(true, true, 2);
        Require(Close(Computed[4].HeightAndNormal.x, 0.2F) && Computed[4].HeightAndNormal.y < -0.35F,
                "Unsupported State must retain Meso geometry with a normal consistent with its display scale.");
        GridShared.GetGeometryScalarBuffer().Upload(std::array<SurfaceState::TSurfaceGPUGeometryScalar, 9>{}.data(),
                                                    sizeof(MesoRamp));
        Side.ViewProjection = glm::mat4(1.0F);
        Side.ViewProjection[3][2] = 0.2F;
        TUniform LitUniform;
        LitUniform.StateChannelCount = 3;
        LitUniform.BaseColor = {0.4F, 0.4F, 0.4F, 1};
        LitUniform.DemoStateChannels = {LitBindings.Wetness, LitBindings.Mud, SurfaceState::InvalidStateId, 1};
        std::array<float, 27> LitState{};
        auto UploadLitState = [&] { LitInstance.GetStateABuffer().Upload(LitState.data(), sizeof(LitState)); };
        auto RenderLit = [&](bool AB = true, bool Height = false)
        { return RenderGrid(AB, Height, 1, TRenderViewMode::Lit, 0, 1, &LitUniform); };
        UploadLitState();
        const auto DryLit = RenderLit();
        for (std::size_t T = 0; T < 9; ++T)
            LitState[T * 3] = 500;
        UploadLitState();
        const auto UnrelatedLit = RenderLit();
        Require(DryLit == UnrelatedLit, "An unrelated State must not affect Wetness or Mud appearance.");
        for (std::size_t T = 0; T < 9; ++T)
            LitState[T * 3 + LitBindings.Wetness] = 1;
        UploadLitState();
        const auto WetLit = RenderLit();
        Require(WetLit[90 * Extent.width + 90].r < DryLit[90 * Extent.width + 90].r,
                "Wetness must darken diffuse appearance away from the highlight.");
        float DryPeak = 0, WetPeak = 0;
        for (std::size_t P = 0; P < WetLit.size(); ++P)
        {
            Require(std::isfinite(WetLit[P].r), "Lit highlights must remain finite.");
            DryPeak = std::max(DryPeak, DryLit[P].r);
            WetPeak = std::max(WetPeak, WetLit[P].r);
        }
        Require(WetPeak > DryPeak + 0.1F, "Wet roughness must produce a stronger localized specular highlight.");
        LitUniform.CameraPosition = {0.8F, 0.4F, 1, 1};
        const auto MovedCameraLit = RenderLit();
        Require(MovedCameraLit != WetLit, "Specular reflection must follow camera position.");
        // Equal saturation at half texel area must keep the same Lit appearance.
        LitInstance.UpdateWorldTexelAreas(std::vector<float>(9, SurfaceStateReferenceArea * 0.5F));
        for (auto& V : LitState)
            V *= 0.5F;
        UploadLitState();
        Require(RenderLit() == MovedCameraLit, "Lit saturation must respect texel world area and Profile capacity.");
        LitInstance.UpdateWorldTexelAreas(std::vector<float>(9, SurfaceStateReferenceArea));
        for (auto& V : LitState)
            V *= 2;
        UploadLitState();
        LitUniform.CameraPosition = {-0.35F, -0.55F, 1, 1};
        LitUniform.DemoStateChannels.w = 0;
        Require(RenderLit() == DryLit, "Disabling demo effects must recover the dry base material.");
        LitUniform.DemoStateChannels.w = 1;
        LitState.fill(0);
        for (std::size_t T = 0; T < 9; ++T)
            LitState[T * 3 + LitBindings.Mud] = 1;
        UploadLitState();
        const auto MudLit = RenderLit();
        const auto MudPixel = MudLit[90 * Extent.width + 90];
        Require(MudPixel.r > MudPixel.g * 1.25F && MudPixel.g > MudPixel.b * 1.15F,
                "Mud must replace the base color with a brown coating.");
        for (std::size_t T = 0; T < 9; ++T)
            LitState[T * 3 + LitBindings.Wetness] = 1;
        UploadLitState();
        const auto WetMudLit = RenderLit();
        Require(WetMudLit[90 * Extent.width + 90].r < MudPixel.r && WetMudLit[90 * Extent.width + 90].g < MudPixel.g,
                "Wetness must also darken an existing Mud coating.");
        LitUniform.DemoStateChannels.x = LitUniform.DemoStateChannels.y = SurfaceState::InvalidStateId;
        Require(RenderLit() == DryLit, "Absent demo names must produce the dry base material.");
        LitUniform.DemoStateChannels.x = LitBindings.Wetness;
        LitUniform.DemoStateChannels.y = LitBindings.Mud;

        LitInstance.GetStateBBuffer().Upload(std::array<float, 27>{}.data(), sizeof(LitState));
        Require(RenderLit(false) == DryLit, "Lit must read the currently bound State B buffer.");
        std::array<std::uint32_t, 3> Support{1, 1, 0};
        LitProfiles.GetSupportedBuffer().Upload(Support.data(), sizeof(Support));
        LitState.fill(0);
        for (std::size_t T = 0; T < 9; ++T)
            LitState[T * 3 + LitBindings.Wetness] = 1;
        UploadLitState();
        Require(RenderLit() == DryLit,
                "A profile-unsupported State must have no appearance even if its slot contains data.");
        Support = {1, 1, 1};
        LitProfiles.GetSupportedBuffer().Upload(Support.data(), sizeof(Support));
        LitState.fill(0);
        LitState[4 * 3 + LitBindings.Mud] = 4;
        UploadLitState();
        Side.ViewProjection[2][0] = 3;
        const auto LitMound = RenderLit(true, true);
        Require(LitMound[128 * Extent.width + 220].a > 0.5F && Close(Computed[4].HeightAndNormal.x, 0.4F),
                "Mud Lit must use the same computed interior-texel displacement as height debugging.");
        LitUniform.DebugOptions.z = 100;
        Require(RenderLit(true, true) == LitMound, "Debug height settings must not change Lit mud geometry.");
        std::array<float, 27> LitStateAfter{};
        LitInstance.GetStateABuffer().Download(LitStateAfter.data(), sizeof(LitStateAfter));
        Require(LitStateAfter == LitState, "Lit rendering must preserve simulation State.");
        // Optional artifacts for inspecting the actual GPU output without changing the application.
        if (const char* Directory = std::getenv("MDSS_SURFACE_RENDER_CAPTURE_DIR"))
        {
            std::filesystem::create_directories(Directory);
            const auto Capture = [&](const char* Name, const std::vector<glm::vec4>& Pixels)
            {
                std::ofstream File(std::filesystem::path(Directory) / Name, std::ios::binary);
                File << "P6\n" << Extent.width << ' ' << Extent.height << "\n255\n";
                for (const auto& Pixel : Pixels)
                    for (int C = 0; C < 3; ++C)
                    {
                        const float Linear = std::clamp(Pixel[C], 0.0F, 1.0F);
                        const float SRGB =
                            Linear <= 0.0031308F ? Linear * 12.92F : 1.055F * std::pow(Linear, 1.0F / 2.4F) - 0.055F;
                        File.put(static_cast<char>(std::lround(SRGB * 255)));
                    }
                Require(File.good(), "GPU render artifact could not be saved.");
            };
            Capture("dry.ppm", DryLit);
            Capture("wet.ppm", WetLit);
            Capture("mud.ppm", MudLit);
            Capture("mud-mound.ppm", LitMound);
            Capture("height-grid.ppm", GridOnly);
        }
    }
} // namespace MDSS::Tests
