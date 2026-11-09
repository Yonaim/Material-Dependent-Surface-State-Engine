#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
// ============================================================
// 00. Material and GPU Resource Setup
// ============================================================
#pragma region 00_Material_and_GPU_Resource_Setup

    VkDescriptorSetLayout FRenderer::CreateMaterialDescriptorSetLayout(VkDevice Device)
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
        Bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        Bindings[2].descriptorCount = 1;
        // Meso Offset은 정점 셰이더도 렌더 모드 값을 읽어 변위 여부를 결정한다.
        Bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

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

    void FRenderer::CreateRenderFinishedSemaphores()
    {
        if (!RenderFinishedSemaphores.empty())
        {
            throw std::logic_error("Render-finished semaphores already exist for the current swapchain.");
        }

        RenderFinishedSemaphores.resize(SwapchainData.GetImages().size(), VK_NULL_HANDLE);
        VkSemaphoreCreateInfo SemaphoreInfo{};
        SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (VkSemaphore& Semaphore : RenderFinishedSemaphores)
        {
            if (vkCreateSemaphore(Context.GetDevice(), &SemaphoreInfo, nullptr, &Semaphore) != VK_SUCCESS)
            {
                DestroyRenderFinishedSemaphores();
                throw std::runtime_error("Failed to create swapchain image render-finished semaphore.");
            }
        }
    }

    void FRenderer::DestroyRenderFinishedSemaphores() noexcept
    {
        for (VkSemaphore Semaphore : RenderFinishedSemaphores)
        {
            if (Semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(Context.GetDevice(), Semaphore, nullptr);
            }
        }
        RenderFinishedSemaphores.clear();
    }

    void FRenderer::CreateTimestampQueryPool(std::size_t SolverInstanceCount)
    {
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(Context.GetDevice(), TimestampQueryPool, nullptr);
            TimestampQueryPool = VK_NULL_HANDLE;
        }
        if (TimestampValidBits == 0U)
        {
            return;
        }
        const std::uint64_t SolverQuerySlots = SolverInstanceCount > 0U ? 1U : 0U;
        const std::uint64_t SolverQueryCount = SolverQuerySlots * 8U * SurfaceState::MaxSimulationStepsPerFrame;
        const std::uint64_t OverlayQueryCount =
            static_cast<std::uint64_t>(SolverInstanceCount) * 3U * OverlayTimestampQueriesPerLayer;
        const std::uint64_t QueriesPerFrame = FixedTimestampQueryCount + SolverQueryCount + OverlayQueryCount;
        if (QueriesPerFrame > std::numeric_limits<std::uint32_t>::max() / FRenderContext::MaxFramesInFlight)
        {
            FLogger::Warning("TRenderer", "GPU timing query count exceeds the supported range.");
            return;
        }

        SolverTimestampGroupCount = static_cast<std::uint32_t>(SolverQuerySlots);
        TimestampQueriesPerFrame = static_cast<std::uint32_t>(QueriesPerFrame);
        VkQueryPoolCreateInfo QueryPoolInfo{};
        QueryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        QueryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        QueryPoolInfo.queryCount =
            static_cast<std::uint32_t>(FRenderContext::MaxFramesInFlight) * TimestampQueriesPerFrame;
        if (vkCreateQueryPool(Context.GetDevice(), &QueryPoolInfo, nullptr, &TimestampQueryPool) != VK_SUCCESS)
        {
            TimestampQueryPool = VK_NULL_HANDLE;
            LastRenderGpuMilliseconds = -1.0F;
            LastSolverGpuMilliseconds = -1.0F;
            LastSolverPass1GpuMilliseconds = -1.0F;
            LastSolverPass2GpuMilliseconds = -1.0F;
            FLogger::Warning("TRenderer", "GPU timestamp queries unavailable; profiling timings will be hidden.");
        }
        bTimestampQueriesSubmitted.fill(false);
        SolverTimestampStepsSubmitted.fill(0);
        OverlayTimestampLayersSubmitted.fill(0);
    }

    void FRenderer::CreateMaterialDescriptorResources()
    {
        const std::size_t MaterialCount = Assets.GetMaterialCount();
        if (MaterialCount == 0)
        {
            FLogger::Warning("TRenderer",
                             "No materials are registered; material descriptor resources were not created.");
            return;
        }

        FLogger::Debug("TRenderer",
                       "Creating descriptor resources for " + std::to_string(MaterialCount) + " material(s).");

        std::array<VkDescriptorPoolSize, 2> PoolSizes{};
        PoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        PoolSizes[0].descriptorCount =
            static_cast<std::uint32_t>(MaterialCount * FRenderContext::MaxFramesInFlight * 2U);
        PoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        PoolSizes[1].descriptorCount = static_cast<std::uint32_t>(MaterialCount * FRenderContext::MaxFramesInFlight);

        VkDescriptorPoolCreateInfo PoolInfo{};
        PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
        PoolInfo.pPoolSizes = PoolSizes.data();
        PoolInfo.maxSets = static_cast<std::uint32_t>(MaterialCount * FRenderContext::MaxFramesInFlight);

        VkDescriptorPool NewPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(Context.GetDevice(), &PoolInfo, nullptr, &NewPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create material descriptor pool.");
        }

        std::vector<FMaterialRenderResource> NewResources;
        try
        {
            NewResources.resize(MaterialCount);
            std::vector<VkDescriptorSetLayout> Layouts(MaterialCount * FRenderContext::MaxFramesInFlight,
                                                       MaterialDescriptorSetLayout);
            std::vector<VkDescriptorSet>       Sets(MaterialCount * FRenderContext::MaxFramesInFlight, VK_NULL_HANDLE);

            VkDescriptorSetAllocateInfo AllocateInfo{};
            AllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            AllocateInfo.descriptorPool = NewPool;
            AllocateInfo.descriptorSetCount =
                static_cast<std::uint32_t>(MaterialCount * FRenderContext::MaxFramesInFlight);
            AllocateInfo.pSetLayouts = Layouts.data();

            if (vkAllocateDescriptorSets(Context.GetDevice(), &AllocateInfo, Sets.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate material descriptor sets.");
            }

            for (std::size_t Index = 0; Index < MaterialCount; ++Index)
            {
                const Asset::FMaterialAssetHandle Handle = static_cast<Asset::FMaterialAssetHandle>(Index);
                const Asset::FMaterialAsset&      Material = Assets.GetMaterial(Handle);
                const Asset::FTextureAsset&       BaseTexture = Assets.GetTexture(Material.GetBaseColorTexture());
                const Asset::FTextureAsset&       NormalTexture = Assets.GetTexture(Material.GetNormalTexture());

                for (std::uint32_t Frame = 0; Frame < FRenderContext::MaxFramesInFlight; ++Frame)
                {
                    const std::size_t SetIndex = Index * FRenderContext::MaxFramesInFlight + Frame;
                    NewResources[Index].UniformBuffers[Frame] = std::make_unique<GPU::FGPUBuffer>(
                        Context.GetPhysicalDevice(),
                        Context.GetDevice(),
                        MaterialUniformStride * MaterialViewportCapacity,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        GPU::EGPUBufferMemoryCategory::Renderer);
                    NewResources[Index].DescriptorSets[Frame] = Sets[SetIndex];

                    VkDescriptorImageInfo BaseImage{};
                    BaseImage.sampler = BaseTexture.GetSampler();
                    BaseImage.imageView = BaseTexture.GetImageView();
                    BaseImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    VkDescriptorImageInfo NormalImage{};
                    NormalImage.sampler = NormalTexture.GetSampler();
                    NormalImage.imageView = NormalTexture.GetImageView();
                    NormalImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    VkDescriptorBufferInfo MaterialBuffer{};
                    MaterialBuffer.buffer = NewResources[Index].UniformBuffers[Frame]->GetHandle();
                    MaterialBuffer.offset = 0;
                    MaterialBuffer.range = sizeof(FMaterialUniform);

                    std::array<VkWriteDescriptorSet, 3> Writes{};
                    for (VkWriteDescriptorSet& Write : Writes)
                    {
                        Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        Write.dstSet = Sets[SetIndex];
                        Write.descriptorCount = 1;
                    }
                    Writes[0].dstBinding = 0;
                    Writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Writes[0].pImageInfo = &BaseImage;
                    Writes[1].dstBinding = 1;
                    Writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Writes[1].pImageInfo = &NormalImage;
                    Writes[2].dstBinding = 2;
                    Writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                    Writes[2].pBufferInfo = &MaterialBuffer;

                    vkUpdateDescriptorSets(
                        Context.GetDevice(), static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
                }
            }
        }
        catch (...)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), NewPool, nullptr);
            throw;
        }

        if (MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), MaterialDescriptorPool, nullptr);
        }
        MaterialDescriptorPool = NewPool;
        MaterialResources = std::move(NewResources);

        FLogger::Info("TRenderer",
                      "Material descriptor sets created: " + std::to_string(MaterialResources.size()) + ".");
    }

    void FRenderer::UploadMaterialUniforms(std::uint32_t   Frame,
                                           const FScene&   SceneData,
                                           const FDebugUI& DebugInterface,
                                           float           LitHeightDisplayScale)
    {
        const auto             Bindings = GetDemoSurfaceStateBindings();
        const std::size_t      MaterialCount = std::min(Assets.GetMaterialCount(), MaterialResources.size());
        FDemoStateColorMapping WaterColorMap = DemoEffects.WaterFilmColorMap;
        if (DemoEffects.bEmphasizeWaterFilm)
        {
            WaterColorMap.LowSaturationColor = {0.14F, 0.55F, 0.58F};
            WaterColorMap.HighSaturationColor = {0.04F, 0.39F, 0.55F};
        }
        for (std::size_t Index = 0; Index < MaterialCount; ++Index)
        {
            if (!MaterialResources[Index].UniformBuffers[Frame])
            {
                continue;
            }

            const Asset::FMaterialAsset& Material = Assets.GetMaterial(static_cast<Asset::FMaterialAssetHandle>(Index));
            FMaterialUniform             Uniform{
                Material.GetBaseColor(),
                static_cast<std::uint32_t>(GetRenderViewMode(0)),
                bFlipNormalY ? 1U : 0U,
                NormalStrength,
                AmbientLight,
                DebugStateChannel,
                static_cast<std::uint32_t>(SurfaceData.GetSurfaceStateRegistry().GetStateCount()),
                GetDebugViewParameter(GetRenderViewMode(0)),
                bStateHeatmapReliefShadingEnabled ? 1.0F : 0.0F,
                            {0.0F, 0.0F, LitHeightDisplayScale, SurfaceDebugSettings.DisplacementScale},
                            {bStateHeatmapAllStatesEnabled ? 1U : 0U,
                 SurfaceDebugSettings.AccumulationComponent,
                 SurfaceDebugSettings.HeightGridMode,
                 SurfaceDebugSettings.HeightGridBlockSize},
                            {Bindings.Heat, Bindings.Mud, Bindings.WaterFilm, DemoEffects.bEnabled ? 1U : 0U},
                            {DemoEffects.DryRoughness, DemoEffects.LavaThreshold, DemoEffects.MudRoughness, LitHeightDisplayScale},
                            {DemoEffects.HeatStrength,
                             DemoEffects.MudNoiseAmount,
                             DemoEffects.WaterFilmOpacity,
                             DemoEffects.WaterFilmRoughness},
                            {Bindings.Lava,
                 bOverlayOnlyDebug ? static_cast<std::uint32_t>(OverlayShadingDiagnostic) : 0U,
                             (DemoEffects.bCoverageSmoothing ? 1U : 0U) |
                                 (DemoEffects.bCoverageSmoothing && bPrecomputeCoverageSmoothingEnabled ? 2U : 0U) |
                                 (bRenderStateTextureSamplingEnabled ? 4U : 0U),
                             (DemoEffects.bHeatLayer ? 1U : 0U) | (DemoEffects.bMudDisplacement ? 2U : 0U) |
                                 (DemoEffects.bWaterFilmDisplacement ? 4U : 0U) | (DemoEffects.bLavaDisplacement ? 8U : 0U) |
                                 (Bindings.AbsorbedWetness != SurfaceState::InvalidStateId ? 16U : 0U) |
                                 (DemoEffects.bIceDisplacement ? 32U : 0U)},
                            {DemoEffects.HeatColorMap.RampStart,
                             DemoEffects.MudColorMap.RampStart,
                             WaterColorMap.RampStart,
                             DemoEffects.LavaColorMap.RampStart},
                            {DemoEffects.HeatColorMap.RampEnd,
                             DemoEffects.MudColorMap.RampEnd,
                             WaterColorMap.RampEnd,
                             DemoEffects.LavaColorMap.RampEnd},
                            {glm::vec4(DemoEffects.HeatColorMap.LowSaturationColor, 1.0F),
                             glm::vec4(DemoEffects.MudColorMap.LowSaturationColor, 1.0F),
                             glm::vec4(WaterColorMap.LowSaturationColor, 1.0F),
                             glm::vec4(DemoEffects.LavaColorMap.LowSaturationColor, 1.0F)},
                            {glm::vec4(DemoEffects.HeatColorMap.HighSaturationColor, 1.0F),
                             glm::vec4(DemoEffects.MudColorMap.HighSaturationColor, 1.0F),
                             glm::vec4(WaterColorMap.HighSaturationColor, 1.0F),
                             glm::vec4(DemoEffects.LavaColorMap.HighSaturationColor, 1.0F)},
                glm::vec4(0.0F),
                Material.GetWaterAppearance(),
                            {Bindings.AbsorbedWetness, DemoEffects.bEmphasizeWaterFilm ? 1U : 0U, Bindings.Ice, 0U},
                            {Bindings.Burn, Bindings.Scorched, 0U, 0U}};
            for (std::size_t ViewportIndex = 0; ViewportIndex < DebugInterface.GetViewportCount(); ++ViewportIndex)
            {
                const ERenderViewMode Mode = GetRenderViewMode(ViewportIndex);
                Uniform.RenderMode = Mode == ERenderViewMode::Wireframe && bWireframeUniformWhite
                                         ? RenderModeWireframeUniformWhite
                                         : static_cast<std::uint32_t>(Mode);
                Uniform.DebugViewParameter = GetDebugViewParameter(Mode);
                Uniform.CameraPosition =
                    glm::vec4(DebugInterface.GetViewportCamera(SceneData, ViewportIndex).GetPosition(), 1.0F);
                MaterialResources[Index].UniformBuffers[Frame]->Upload(
                    &Uniform, sizeof(Uniform), MaterialUniformStride * ViewportIndex);
            }
        }
    }

#pragma endregion
// ============================================================
// 01. Format Selection
// ============================================================
#pragma region 01_Format_Selection

    VkFormat FRenderer::FindDepthFormat(VkPhysicalDevice PhysicalDevice)
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

    VkFormat FRenderer::FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
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
#pragma endregion
} // namespace MDSS::Rendering
