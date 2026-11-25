/**
 * @file SurfaceSimulationDisplay.cpp
 * @brief 시뮬레이션 스냅샷을 Surface Simulation 뷰에 그려 제출하고 필요할 때 캡처한다.
 */

#include "Rendering/Surface/Simulation/SurfaceSimulationDisplay.h"
#include "Application/Window.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "GPU/Pass/GraphicsPass.h"
#include "GPU/Vulkan/Pipeline/GraphicsPipeline.h"
#include "GPU/Vulkan/Resource/GPUImage.h"
#include "GPU/Vulkan/Resource/GPUImageView.h"
#include "GPU/Vulkan/Swapchain/Swapchain.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Scene/WorldReferenceGeometry.h"
#include "Rendering/Surface/Simulation/SurfaceSimulationMesh.h"
#include "ShaderCatalog.h"
#include "SurfaceState/Simulation/SurfaceSimulationGpu.h"
#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <imgui_impl_vulkan.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <glm/glm.hpp>
#include <glm/gtc/color_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/intersect.hpp>
#include <limits>
#include <numbers>
#include <numeric>
#include <stb_image_write.h>
#include <stdexcept>
#include <vector>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Display Targets and Pipeline Helpers
    // ============================================================
    #pragma region 00_Display_Targets_and_Pipeline_Helpers
        // 화면 출력과 굴절용 중간 패스의 Vulkan 리소스 수명 및 합성 설정을 묶는다.

        namespace
        {
            constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;
            constexpr auto     HostMemory = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            struct FDrawPush
            {
                glm::mat4            ViewProjection;
                glm::vec4            Camera;
                glm::vec4            MaterialScale;
                Liquid::FSurfaceTilt Pose;
                std::uint32_t        Layer;
                std::uint32_t        Mode;
                std::uint32_t        Palette;
                std::uint32_t        ViewSize;
                std::uint32_t        AppearanceFlags;
                std::uint32_t        LiquidOptions;
            };

            // SurfaceDrawParameters.glsl의 표시 플래그와 일치한다.
            constexpr std::uint32_t CombustionField = 1U << 0;
            constexpr std::uint32_t FireLighting = 1U << 1;
            constexpr std::uint32_t IceLayerField = 1U << 2;
            constexpr std::uint32_t HeaterLighting = 1U << 3;
            constexpr std::uint32_t CellOutlineOverlay = 1U << 4;
            constexpr std::uint32_t LavaDeposit = 1U << 5;
            constexpr std::uint32_t IncandescentLiquid = 1U << 19;
            constexpr std::uint32_t ChartAlbedo = 1U << 20;
            constexpr std::uint32_t IncandescentSubstrate = 1U << 21;
            constexpr std::uint32_t EnvironmentReflections = 1U << 22;
            constexpr std::uint32_t SpecimenMaterials = 1U << 23;
            constexpr std::uint32_t OverflowFilmOnBed = 1U << 6;
            constexpr std::uint32_t NoStudioStrip = 1U << 7;
            constexpr std::uint32_t NoMaterialNormals = 1U << 16; // 비트 8–15는 진단 상태 인덱스다.
            constexpr std::uint32_t MetalSubstrate = 1U << 17;
            constexpr std::uint32_t SnowLayer = 4U;
            constexpr std::uint32_t LavaDepositLayer = 5U;
            constexpr std::uint32_t OverflowLayer = 6U;
            constexpr std::uint32_t WaterJoinLayer = 7U;
            constexpr std::uint32_t SectionWindowLayer = 8U;
            constexpr std::uint32_t SectionWindowBackLayer = 9U;
            constexpr std::uint32_t VesselContentsLayer = 10U;
            constexpr std::uint32_t SmoothLiquidBoundary = 1U;
            constexpr std::uint32_t SloshOptics = 1U << 1U;
            constexpr std::uint32_t SloshHeight = 1U << 2U;
            static_assert(offsetof(FDrawPush, Pose) == 96 && offsetof(FDrawPush, Layer) == 104 &&
                          sizeof(FDrawPush) == 128 && offsetof(FDrawPush, LiquidOptions) == 124);

            void Check(VkResult Result, const char *Message)
            {
                if (Result != VK_SUCCESS)
                    throw std::runtime_error(Message);
            }

            Asset::FTextureData LoadSpecimenAtlas(std::span<const Rendering::FSurfaceSpecimenAppearance> Specimens,
                                                  bool                                                   bNormals)
            {
                constexpr std::uint32_t Tile = 512;
                const auto              Width = Tile * static_cast<std::uint32_t>(Specimens.size());
                Asset::FTextureData     Atlas{Width, Tile, std::vector<std::uint8_t>(4U * Width * Tile)};
                for (std::size_t Index = 0; Index < Specimens.size(); ++Index)
                {
                    const auto &Specimen = Specimens[Index];
                    const auto &Path = bNormals ? Specimen.NormalTexture : Specimen.ColorTexture;
                    const auto  Fallback = bNormals ? Asset::FTextureData{1, 1, {128, 128, 255, 255}}
                                                    : Asset::FTextureData{1, 1, {255, 255, 255, 204}};
                    const auto  Source = Path.empty() ? Fallback : Asset::FTextureLoader::LoadRGBA8(Path);
                    // 직사각 타일은 짧은 축에 반복해 원본 직조와 기공의 비율을 보존한다.
                    const auto Longest = std::max(Source.Width, Source.Height);
                    for (std::uint32_t Y = 0; Y < Tile; ++Y)
                        for (std::uint32_t X = 0; X < Tile; ++X)
                        {
                            const auto SourceX = (X * Longest / Tile) % Source.Width;
                            const auto SourceY = (Y * Longest / Tile) % Source.Height;
                            const auto Src = 4 * (SourceY * Source.Width + SourceX);
                            const auto Dst = 4 * (Y * Width + Index * Tile + X);
                            std::copy_n(Source.Pixels.data() + Src, 4, Atlas.Pixels.data() + Dst);
                        }
                }
                return Atlas;
            }

            void WritePng(void *File, void *Data, int Size)
            {
                static_cast<std::ofstream *>(File)->write(static_cast<const char *>(Data), Size);
            }

            GPU::FGraphicsPipelineConfig PipelineConfig(VkDescriptorSetLayout SetLayout, bool bRefraction = false)
            {
                using Shaders::EShader;
                GPU::FGraphicsPipelineConfig Config;
                Config.ShaderStages = {
                    {VK_SHADER_STAGE_VERTEX_BIT, Shaders::Path(EShader::Rendering_Surface_Simulation_Surface_vert)},
                    {VK_SHADER_STAGE_FRAGMENT_BIT,
                        Shaders::Path(bRefraction ? EShader::Rendering_Surface_Simulation_Refraction_frag
                                               : EShader::Rendering_Surface_Simulation_Surface_frag)}};
                Config.VertexBindings = {{0, sizeof(FSurfaceSimulationVertex), VK_VERTEX_INPUT_RATE_VERTEX}};
                Config.VertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FSurfaceSimulationVertex, Position)},
                                           {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(FSurfaceSimulationVertex, Normal)},
                                           {2, 0, VK_FORMAT_R32_UINT, offsetof(FSurfaceSimulationVertex, Cell)},
                                           {3, 0, VK_FORMAT_R32_UINT, offsetof(FSurfaceSimulationVertex, Region)}};
                Config.CullMode = VK_CULL_MODE_NONE;
                Config.bDepthTestEnabled = true;
                Config.bDepthWriteEnabled = true;
                // Blended materials use source alpha as coverage over an already opaque scene.
                Config.bColorAlphaWriteEnabled = false;
                Config.DescriptorSetLayouts = {SetLayout};
                Config.PushConstantRanges = {
                    {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(FDrawPush)}};
                return Config;
            }

            GPU::FGraphicsPipelineConfig OverflowPipelineConfig(VkDescriptorSetLayout SetLayout, bool bRefraction)
            {
                auto Config = PipelineConfig(SetLayout, bRefraction);
                Config.ShaderStages[0].ShaderPath =
                    Shaders::Path(Shaders::EShader::Rendering_Surface_Simulation_LiquidSurfaceConnections_vert);
                Config.VertexBindings.clear();
                Config.VertexAttributes.clear();
                Config.bDepthWriteEnabled = false;
                Config.bBlendingEnabled = true;
                return Config;
            }

            class FOpticsTargets
            {
                public:
                FOpticsTargets(const GPU::FVulkanContext &Context,
                               VkExtent2D                 Extent,
                               VkFormat                   Format,
                               VkDescriptorSetLayout      SetLayout,
                               VkRenderPass               PresentPass)
                    : Pass(Context.GetDevice(), Format, DepthFormat, true),
                      Color(Context.GetPhysicalDevice(),
                            Context.GetDevice(),
                            Extent,
                            Format,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
                      Depth(Context.GetPhysicalDevice(),
                            Context.GetDevice(),
                            Extent,
                            DepthFormat,
                            VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
                      ColorView(Context.GetDevice(), Color.GetHandle(), Color.GetFormat(), VK_IMAGE_ASPECT_COLOR_BIT),
                      DepthView(Context.GetDevice(), Depth.GetHandle(), DepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT),
                      Framebuffer(Context.GetDevice(),
                                  Pass.GetHandle(),
                                  Extent,
                                  {ColorView.GetHandle()},
                                  {DepthView.GetHandle()}),
                      Opaque(Context.GetDevice(), Pass.GetHandle(), PipelineConfig(SetLayout))
                {
                    auto Config = PipelineConfig(SetLayout, true);
                    Config.bDepthWriteEnabled = false;
                    Refractive = std::make_unique<GPU::FGraphicsPipeline>(Context.GetDevice(), PresentPass, Config);
                    Overflow = std::make_unique<GPU::FGraphicsPipeline>(Context.GetDevice(),
                                                                        PresentPass,
                                                                        OverflowPipelineConfig(SetLayout, true));
                }

                GPU::FRenderPass                        Pass;
                GPU::FGPUImage                          Color;
                GPU::FGPUImage                          Depth;
                GPU::FGPUImageView                      ColorView;
                GPU::FGPUImageView                      DepthView;
                GPU::FFramebuffer                       Framebuffer;
                GPU::FGraphicsPipeline                  Opaque;
                std::unique_ptr<GPU::FGraphicsPipeline> Refractive;
                std::unique_ptr<GPU::FGraphicsPipeline> Overflow;
            };

            class FTargets
            {
                public:
                FTargets(const GPU::FVulkanContext &Context,
                         VkExtent2D                 Extent,
                         VkFormat                   Format,
                         const GPU::FSwapchain     *Swapchain,
                         VkDescriptorSetLayout      SetLayout)
                    : Device(Context.GetDevice()),
                      Pass(Device, Format, DepthFormat, !Swapchain)
                {
                    try
                    {
                        std::vector<VkImageView> ColorViews;
                        if (Swapchain)
                            ColorViews = Swapchain->GetImageViews();
                        else
                        {
                            Color = std::make_unique<GPU::FGPUImage>(Context.GetPhysicalDevice(),
                                                                     Device,
                                                                     Extent,
                                                                     Format,
                                                                     VK_IMAGE_TILING_OPTIMAL,
                                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                                         VK_IMAGE_USAGE_SAMPLED_BIT |
                                                                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                            ColorView = std::make_unique<GPU::FGPUImageView>(Device,
                                                                             Color->GetHandle(),
                                                                             Format,
                                                                             VK_IMAGE_ASPECT_COLOR_BIT);
                            ColorViews.push_back(ColorView->GetHandle());
                        }
                        std::vector<VkImageView> Views;
                        for (std::size_t Index = 0; Index < ColorViews.size(); ++Index)
                        {
                            Depth.emplace_back(std::make_unique<GPU::FGPUImage>(
                                Context.GetPhysicalDevice(),
                                Device,
                                Extent,
                                DepthFormat,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (Swapchain ? 0U : VK_IMAGE_USAGE_SAMPLED_BIT),
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
                            DepthViews.emplace_back(std::make_unique<GPU::FGPUImageView>(Device,
                                                                                         Depth.back()->GetHandle(),
                                                                                         DepthFormat,
                                                                                         VK_IMAGE_ASPECT_DEPTH_BIT));
                            Views.push_back(DepthViews.back()->GetHandle());
                            if (Swapchain)
                            {
                                PresentReady.push_back(VK_NULL_HANDLE);
                                const VkSemaphoreCreateInfo SemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
                                Check(vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &PresentReady.back()),
                                      "Failed to create water presentation semaphore.");
                            }
                        }
                        Framebuffers =
                            std::make_unique<GPU::FFramebuffer>(Device, Pass.GetHandle(), Extent, ColorViews, Views);
                        MakePipelines(SetLayout);
                        if (!Swapchain)
                        {
                            auto Config = Rendering::BuildWorldReferencePipelineConfig();
                            Config.bColorAlphaWriteEnabled = false;
                            WorldReference = std::make_unique<GPU::FGraphicsPipeline>(Device, Pass.GetHandle(), Config);
                        }
                    }
                    catch (...)
                    {
                        ReleaseSemaphores();
                        throw;
                    }
                }

                ~FTargets()
                {
                    ReleaseSemaphores();
                }
                FTargets(const FTargets &) = delete;
                FTargets &operator=(const FTargets &) = delete;

                VkDevice                                         Device;
                GPU::FRenderPass                                 Pass;
                std::unique_ptr<GPU::FGPUImage>                  Color;
                std::unique_ptr<GPU::FGPUImageView>              ColorView;
                std::vector<std::unique_ptr<GPU::FGPUImage>>     Depth;
                std::vector<std::unique_ptr<GPU::FGPUImageView>> DepthViews;
                std::vector<VkSemaphore>                         PresentReady;
                std::unique_ptr<GPU::FFramebuffer>               Framebuffers;
                std::unique_ptr<GPU::FGraphicsPipeline>          SolidPipeline;
                std::unique_ptr<GPU::FGraphicsPipeline>          WaterPipeline;
                std::unique_ptr<GPU::FGraphicsPipeline>          OverflowPipeline;
                std::unique_ptr<GPU::FGraphicsPipeline>          FlamePipeline;
                std::unique_ptr<GPU::FGraphicsPipeline>          WorldReference;

                private:
                void ReleaseSemaphores()
                {
                    for (auto Ready : PresentReady)
                        vkDestroySemaphore(Device, Ready, nullptr);
                }

                void MakePipelines(VkDescriptorSetLayout SetLayout)
                {
                    auto Config = PipelineConfig(SetLayout);
                    SolidPipeline = std::make_unique<GPU::FGraphicsPipeline>(Device, Pass.GetHandle(), Config);

                    Config.bDepthWriteEnabled = false;
                    Config.bBlendingEnabled = true;
                    WaterPipeline = std::make_unique<GPU::FGraphicsPipeline>(Device, Pass.GetHandle(), Config);
                    OverflowPipeline = std::make_unique<GPU::FGraphicsPipeline>(Device,
                                                                                Pass.GetHandle(),
                                                                                OverflowPipelineConfig(SetLayout, false));

                    using Shaders::EShader;
                    Config.ShaderStages = {
                        {VK_SHADER_STAGE_VERTEX_BIT,
                            Shaders::Path(EShader::Rendering_Surface_Simulation_SurfaceVolumeEffects_vert)},
                        {VK_SHADER_STAGE_FRAGMENT_BIT,
                            Shaders::Path(EShader::Rendering_Surface_Simulation_SurfaceVolumeEffects_frag)}};
                    Config.VertexAttributes.resize(1);
                    FlamePipeline = std::make_unique<GPU::FGraphicsPipeline>(Device, Pass.GetHandle(), Config);
                }
            };
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Display Resources
    // ============================================================
    #pragma region 01_Display_Resources
        // 스왑체인, 장면 메시 및 프레임 단위 리소스를 초기화하고 GPU 작업을 기다린 뒤 해제한다.

        float ThicknessDisplayMaxMm(const Rendering::FStateVisualization &State,
                                    const Liquid::FLiquidMaterial        &Liquid,
                                    float                                 SnowBulkDensityKgM3)
        {
            if (State.ThicknessDisplayMaximumMm > 0.F)
                return State.ThicknessDisplayMaximumMm;
            const float SolidDensity = SnowBulkDensityKgM3 > 0.F ? SnowBulkDensityKgM3 : Liquid.IceDensityKgM3;
            const float Density =
                State.Channel == Rendering::ESurfaceStateChannel::IceMass ? SolidDensity : Liquid.DensityKgM3;
            return 1000.F * State.DisplayMaximum / Density;
        }

        class FSurfaceSimulationDisplay::FImpl
        {
            public:
            FImpl(const GPU::FVulkanContext                      &Context,
                  FWindow                                        *Window,
                  const Liquid::FSurfaceSimulationDomain         &Geometry,
                  const Rendering::FSurfaceSimulationAppearance  &Appearance,
                  std::span<const Rendering::FStateVisualization> States,
                  bool                                            bSmoothDisplay);
            ~FImpl();
            bool DrawFrame(Liquid::FSurfaceSimulationGpu             &Simulation,
                           std::span<const Liquid::FSurfaceStepInput> Inputs,
                           const FSurfaceSimulationViews             &Views,
                           const std::filesystem::path               &Capture);
            bool CanCapture() const
            {
                return !Swapchain || Swapchain->CanReadback();
            }
            FSurfaceSimulationDisplayStats Stats() const
            {
                return Timing.Stats();
            }

            VkDescriptorSet PrepareEditorTarget(VkExtent2D Extent, const FSurfaceSimulationViews &Views);
            void            RecordEditorFrame(VkCommandBuffer                            Commands,
                                              std::uint32_t                              FrameSlot,
                                              Liquid::FSurfaceSimulationGpu             &Simulation,
                                              std::span<const Liquid::FSurfaceStepInput> Inputs,
                                              const FSurfaceSimulationViews             &Views,
                                              const std::filesystem::path               &Capture,
                                              int                                        CaptureView);
            void            FinishEditorCapture();
            void            PrepareBedDisplay(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                              const Rendering::FSurfaceSimulationAppearance &Appearance,
                                              const FSurfaceSimulationViews                 &Views);
            [[nodiscard]] std::optional<FSurfaceInjectionFootprint>
            PickInjectionFootprint(glm::vec2 Position, const FInjectionBrush &Brush) const;
            [[nodiscard]] std::optional<glm::vec2> TemperatureRangeK() const
            {
                return LastTemperatureRangeK;
            }

            [[nodiscard]] std::optional<float>                  PickTemperatureK(glm::vec2 Position) const;
            [[nodiscard]] std::vector<std::array<glm::vec2, 2>> ProjectOutletEdge(const FSurfaceSimulationViews &Views,
                                                                                  int View) const;

            [[nodiscard]] FSurfaceInletOverlay ProjectInletOverlay(const FSurfaceSimulationViews &Views, int View) const;

            private:
            struct FBedHit
            {
                glm::vec2     Point;
                glm::mat4     ViewProjection;
                VkRect2D      Rect;
                int           View;
                std::uint32_t Cell;
                std::uint32_t Region;
            };

            struct FTemperatureReadback
            {
                std::unique_ptr<GPU::FGPUBuffer> Buffer;
                bool                             bPending = false;
            };

            [[nodiscard]] std::optional<FBedHit> PickBed(glm::vec2 Position) const;
            void                                 CollectTemperatureReadback(std::uint32_t FrameSlot);
            void RecordTemperatureReadback(VkCommandBuffer Commands, std::uint32_t FrameSlot);

            struct FBedMesh
            {
                FSurfaceSimulationMesh                Picking;
                std::vector<std::array<glm::vec3, 2>> OutletEdges;
                std::vector<std::size_t>              InletQuads;
                std::vector<std::array<glm::vec3, 2>> InletEdges;
                std::unique_ptr<GPU::FGPUBuffer>      Vertices;
                std::unique_ptr<GPU::FGPUBuffer>      Indices;
                std::uint32_t                         TopIndexCount = 0;
                std::uint32_t                         SolidIndexCount = 0;
                std::uint32_t                         EffectIndexCount = 0;
                std::uint32_t                         BackdropIndexCount = 0;
                std::uint32_t                         SectionIndexCount = 0;
                float                                 SectionCutY = 0.F;
                std::optional<glm::vec4>              VesselBounds;
            };
            void               BuildBedMesh(const Liquid::FSurfaceSimulationDomain &Geometry,
                                            const FSurfaceRenderField              &Field,
                                            bool                                    bSmooth,
                                            float                                   ThicknessMeters);
            void               CacheInletOverlay(FBedMesh &Bed, const Liquid::FSurfaceSimulationDomain &Geometry) const;
            [[nodiscard]] bool SmoothBed(const FSurfaceSimulationViews &Views) const
            {
                return Views.SmoothBedOverride < 0 ? bSceneSmoothBed : Views.SmoothBedOverride != 0;
            }
            [[nodiscard]] bool CrossesDisplayGap(int Column, int Neighbor) const
            {
                const int Width = PickColumns / SpecimenCount;
                return DisplayGapMeters > 0.F && Column / Width != Neighbor / Width;
            }
            [[nodiscard]] glm::mat4 OverlayTransform(const FSurfaceSimulationViews &Views, int View) const;
            [[nodiscard]] glm::mat4
            ViewProjection(const FSurfaceSimulationViews &Views, VkExtent2D Extent, glm::vec3 &Camera) const;
            [[nodiscard]] FSurfaceInjectionFootprint ProjectInjectionRegion(std::uint32_t          Region,
                                                                            int                    View,
                                                                            glm::vec2              Center,
                                                                            const FInjectionBrush &Brush,
                                                                            const glm::mat4       &Transform,
                                                                            VkRect2D               Rect) const;
            static bool                              UsesRefraction(const FSurfaceSimulationViews &Views);
            void                                     RecordSceneFrame(VkCommandBuffer                            Commands,
                                                                      std::uint32_t                              FrameSlot,
                                                                      std::uint32_t                              ImageIndex,
                                                                      Liquid::FSurfaceSimulationGpu             &Simulation,
                                                                      std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                                      const FSurfaceSimulationViews             &Views);
            void                                     AdvanceSnapshot(std::span<const Liquid::FSurfaceStepInput> Inputs);
            std::unique_ptr<GPU::FGPUBuffer>         MakeReadback(VkExtent2D Extent) const;
            void                                     PrepareDisplaySets(const Liquid::FSurfaceSimulationDomain &Geometry);
            void                                     PrepareOptics();
            void                                     RecordDraw(VkCommandBuffer                Commands,
                                                                std::uint32_t                  FrameSlot,
                                                                std::uint32_t                  ImageIndex,
                                                                const FSurfaceSimulationViews &Views,
                                                                bool                           bRefract) const;
            void RecordViews(VkCommandBuffer Commands, const FSurfaceSimulationViews &Views, bool bPrepass) const;
            void RecordWorldReference(VkCommandBuffer                Commands,
                                      const FSurfaceSimulationViews &Views,
                                      const glm::mat4               &ViewProjection) const;
            void RecordCapture(VkCommandBuffer Commands, std::uint32_t ImageIndex, VkBuffer Capture, VkRect2D Rect) const;
            void SaveCapture(const GPU::FGPUBuffer       &Capture,
                             VkExtent2D                   Extent,
                             VkFormat                     Format,
                             const std::filesystem::path &Path) const;
            void Present(std::uint32_t ImageIndex);
            void Recreate();
            void ReleaseDescriptors() noexcept;

            const GPU::FVulkanContext       &Context;
            FWindow                         *Window;
            VkDevice                         Device;
            std::unique_ptr<GPU::FSwapchain> Swapchain;
            VkExtent2D                       TargetExtent{};
            VkFormat                         TargetFormat = VK_FORMAT_R8G8B8A8_SRGB;
            VkDescriptorSet                  EditorTexture = VK_NULL_HANDLE;
            std::unique_ptr<GPU::FGPUBuffer> EditorReadback;
            std::array<FTemperatureReadback, Rendering::FRenderContext::MaxFramesInFlight> TemperatureReadbacks;
            std::vector<Liquid::FSurfaceCellState>                                         TemperatureCells;
            std::optional<glm::vec2>                                                       LastTemperatureRangeK;
            std::optional<std::uint32_t>                                                   RequestedTemperatureStep;
            const std::size_t                                                              TemperatureCellCount;
            const std::optional<std::uint32_t>                                             TemperatureStateIndex;
            std::filesystem::path                                                          EditorCapture;
            VkExtent2D                                                                     EditorCaptureExtent{};
            std::unique_ptr<Rendering::FRenderContext>                                     Frame;
            FSurfaceSimulationTiming                                                       Timing;
            std::array<std::unique_ptr<GPU::FGPUBuffer>, 2>                                Display;
            std::array<Liquid::FSurfaceTilt, 2>                                            DisplayPose{};
            std::array<std::uint32_t, 2>                                                   DisplaySteps{};
            float                                                                          SloshDisplacement = 0.F;
            float                                                                          SloshVelocity = 0.F;
            float                                                                          PreviousTiltRate = 0.F;
            glm::vec3                                                                      Tint;
            float                                                                          TextureRepeatsPerMeter;
            float                                                                          SnowRepeatsPerMeter;
            std::uint32_t                                                                  AppearanceFlags;
            bool                                                                           bSnowLayer;
            std::unique_ptr<Asset::FTextureAsset>                                          SurfaceTexture;
            std::unique_ptr<Asset::FTextureAsset>                                          SurfaceNormalTexture;
            std::unique_ptr<Asset::FTextureAsset>                                          SnowColorTexture;
            std::unique_ptr<Asset::FTextureAsset>                                          SnowNormalTexture;
            std::unique_ptr<Asset::FTextureAsset>                                          EnvironmentTexture;
            float                                                                          SceneScale;
            float                                                                          DisplayGapMeters;
            int                                                                            SpecimenCount;
            float                                                                          CameraTargetZ = -0.001F;
            std::array<FBedMesh, 2>                                                        BedMeshes;
            int                                                                            PickColumns = 0;
            int                                                                            PickRows = 0;
            std::size_t                                                                    PickRegionCount = 0;
            VkExtent2D                                                                     LastDrawExtent{};
            FSurfaceSimulationViews                                                        LastDrawViews;
            Liquid::FSurfaceTilt                                                           LastDrawPose;
            std::uint32_t                                                                  DisplaySlot = 0;
            std::uint32_t                                                                  OverflowInstances = 0;
            bool                                                                           bHasOverflowSnapshot = false;
            bool                                                                           bSceneSmoothBed;
            bool                                                                           bHasSteam = false;
            bool                                                                           bHasFlame = false;
            std::unique_ptr<GPU::FGPUBuffer>                                               SurfaceField;
            std::unique_ptr<GPU::FGPUBuffer>                                               WorldReferenceVertices;
            std::uint32_t                                                                  GridVertexCount = 0;
            std::uint32_t                                                                  AxisVertexCount = 0;
            VkSampler                                                                      OpticsSampler = VK_NULL_HANDLE;
            std::unique_ptr<FOpticsTargets>                                                Optics;
            VkDescriptorSetLayout                                                          SetLayout = VK_NULL_HANDLE;
            VkDescriptorPool                                                               DescriptorPool = VK_NULL_HANDLE;
            std::array<VkDescriptorSet, 2>                                                 Sets{};
            std::unique_ptr<FTargets>                                                      Targets;
        };

        FSurfaceSimulationDisplay::FImpl::FImpl(const GPU::FVulkanContext                      &Context,
                                                FWindow                                        *Window,
                                                const Liquid::FSurfaceSimulationDomain         &Geometry,
                                                const Rendering::FSurfaceSimulationAppearance  &Appearance,
                                                std::span<const Rendering::FStateVisualization> States,
                                                bool                                            bSmoothDisplay)
            : Context(Context),
              Window(Window),
              Device(Context.GetDevice()),
              TemperatureCellCount(Geometry.Cells.size()),
              TemperatureStateIndex(
                  [States]() -> std::optional<std::uint32_t>
                  {
                      const auto State = std::ranges::find(States,
                                                           Rendering::ESurfaceStateChannel::Temperature,
                                                           &Rendering::FStateVisualization::Channel);
                      if (State == States.end())
                          return std::nullopt;
                      return static_cast<std::uint32_t>(State - States.begin());
                  }()),
              Timing(Context),
              Tint(Appearance.Surface.BaseColor),
              TextureRepeatsPerMeter(Appearance.Surface.TexturePath.empty() && Appearance.Specimens.empty()
                                         ? 0.F
                                         : Appearance.Surface.TextureRepeatsPerMeter),
              SnowRepeatsPerMeter(Appearance.Snow.RepeatsPerMeter),
              AppearanceFlags(
                  (Appearance.bHeaterLighting ? HeaterLighting : 0U) | (Appearance.bMetalSubstrate ? MetalSubstrate : 0U) |
                  (Geometry.Liquid.bIncandescent ? IncandescentLiquid : 0U) | (Appearance.bChartAlbedo ? ChartAlbedo : 0U) |
                  (Appearance.bIncandescentSubstrate &&
                           std::ranges::any_of(States,
                                               [](const auto &State)
                                               { return State.Channel == Rendering::ESurfaceStateChannel::Temperature; })
                       ? IncandescentSubstrate
                       : 0U) |
                  (!Appearance.Specimens.empty() ? SpecimenMaterials : 0U)),
              bSnowLayer(Appearance.Snow.BulkDensityKgM3 > 0.F),
              SceneScale(
                  std::max((Geometry.Shape.Width + Appearance.DisplayGapMeters * (Appearance.SpecimenCount - 1)) / 0.09F,
                           Geometry.Shape.Length / 0.05F)),
              DisplayGapMeters(Appearance.DisplayGapMeters),
              SpecimenCount(Appearance.SpecimenCount),
              bSceneSmoothBed(bSmoothDisplay)
        {
            try
            {
                OverflowInstances = static_cast<std::uint32_t>(2 * Geometry.Contacts.size());
                auto Field = MakeSurfaceRenderField(Geometry, Appearance);
                if (States.size() > Field.StateVisuals.size())
                    throw std::runtime_error("Too many state channels for the diagnostic buffer.");
                for (std::size_t Index = 0; Index < States.size(); ++Index)
                {
                    const auto &State = States[Index];
                    const auto  Slot = static_cast<std::size_t>(Index);
                    Field.StateChannels[Slot] = static_cast<int>(State.Channel);
                    Field.StateVisuals[Index] = {glm::convertSRGBToLinear(State.Color), 1.F / State.DisplayMaximum};
                    if (State.HasLayerThickness())
                        Field.StateThicknessInverseMeters[Slot] =
                            1000.F / ThicknessDisplayMaxMm(State, Geometry.Liquid, Appearance.Snow.BulkDensityKgM3);
                    ++Field.StateGroups[State.bSubstrate ? 1 : 0];
                }
                // 얼음이 놓이거나 생성되는 레이어만 프래그먼트에서 보간한다.
                if (!Appearance.bIceSubstrate && Field.PhaseLevels.y <= Field.PhaseLevels.x &&
                    (std::ranges::any_of(Geometry.InitialIceKgM2, [](float Mass) { return Mass > 0.F; }) ||
                     std::ranges::any_of(Geometry.Materials,
                                         [](const Liquid::FCellUpdateParameters &Material)
                                         { return Material.FreezeKgM2SecondK > 0.F; })))
                    AppearanceFlags |= IceLayerField;
                if (Appearance.bLavaDeposit)
                    AppearanceFlags |= LavaDeposit;
                if (Appearance.DisplayThicknessMeters > 0.5F * std::min(Geometry.Shape.Width, Geometry.Shape.Length))
                {
                    // 상면에 맞춘 구도로는 두꺼운 본체의 밑면이 잘릴 수 있다.
                    CameraTargetZ = std::midpoint(Field.BedMetrics.z, Field.BedMetrics.y);
                }
                else if (Field.BedMetrics.y - Field.BedMetrics.x > 0.020F)
                    CameraTargetZ = std::lerp(Field.BedMetrics.x, Field.BedMetrics.y, 0.43F);
                const auto FieldBytes = PackSurfaceRenderField(Field);
                SurfaceField = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                                 Device,
                                                                 FieldBytes.size(),
                                                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                                 HostMemory,
                                                                 GPU::EGPUBufferMemoryCategory::SurfaceGeometry);
                SurfaceField->Upload(FieldBytes.data(), FieldBytes.size());
                bHasSteam = Field.BoundsMin.w > 0.F;
                bHasFlame = Field.BoundsMax.w > 0.F;
                if (!Window)
                {
                    PickColumns = Geometry.Shape.Columns;
                    PickRows = Geometry.Shape.Rows;
                    PickRegionCount = Geometry.Regions.size();
                }
                BuildBedMesh(Geometry, Field, bSmoothDisplay, Appearance.DisplayThicknessMeters);
                if (!Window)
                {
                    const auto Reference =
                        Rendering::BuildWorldReferenceVertices(GridVertexCount, AxisVertexCount, 18.F, 0.26F);
                    WorldReferenceVertices =
                        std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                          Device,
                                                          Reference.size() * sizeof(Rendering::FGizmoVertex),
                                                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                          HostMemory);
                    WorldReferenceVertices->Upload(Reference.data(), WorldReferenceVertices->GetSize());
                }
                const bool bSpecimenAtlas = !Appearance.Specimens.empty();
                const auto Pixels = bSpecimenAtlas ? LoadSpecimenAtlas(Appearance.Specimens, false)
                                    : Appearance.Surface.TexturePath.empty()
                                        ? Asset::FTextureData{1, 1, {255, 255, 255, 255}}
                                        : Asset::FTextureLoader::LoadRGBA8(Appearance.Surface.TexturePath);
                SurfaceTexture = std::make_unique<Asset::FTextureAsset>(Asset::InvalidAssetHandle,
                                                                        "Surface Simulation appearance",
                                                                        Appearance.Surface.TexturePath,
                                                                        Context,
                                                                        Pixels.Width,
                                                                        Pixels.Height,
                                                                        Pixels.Pixels,
                                                                        VK_FORMAT_R8G8B8A8_SRGB);
                const auto &NormalPath = Appearance.Surface.NormalTexturePath;
                if (bSpecimenAtlas || !NormalPath.empty())
                {
                    const auto NormalPixels = bSpecimenAtlas ? LoadSpecimenAtlas(Appearance.Specimens, true)
                                                             : Asset::FTextureLoader::LoadRGBA8(NormalPath);
                    SurfaceNormalTexture = std::make_unique<Asset::FTextureAsset>(Asset::InvalidAssetHandle,
                                                                                  "Surface Simulation material normals",
                                                                                  NormalPath,
                                                                                  Context,
                                                                                  NormalPixels.Width,
                                                                                  NormalPixels.Height,
                                                                                  NormalPixels.Pixels,
                                                                                  VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (SnowRepeatsPerMeter > 0.F)
                {
                    const auto LoadSnow = [&](const std::filesystem::path &Path, VkFormat Format)
                    {
                        const auto Map = Asset::FTextureLoader::LoadRGBA8(Path);
                        return std::make_unique<Asset::FTextureAsset>(Asset::InvalidAssetHandle,
                                                                      "Snow surface",
                                                                      Path,
                                                                      Context,
                                                                      Map.Width,
                                                                      Map.Height,
                                                                      Map.Pixels,
                                                                      Format);
                    };
                    SnowColorTexture = LoadSnow(Appearance.Snow.ColorTexture, VK_FORMAT_R8G8B8A8_SRGB);
                    SnowNormalTexture = LoadSnow(Appearance.Snow.NormalTexture, VK_FORMAT_R8G8B8A8_UNORM);
                }
                if (!Appearance.Environment.TexturePath.empty())
                {
                    const auto &EnvironmentPath = Appearance.Environment.TexturePath;
                    const auto  Panorama = Asset::FTextureLoader::LoadHdrRGBA16F(EnvironmentPath);
                    if (Panorama.Width != 2U * Panorama.Height)
                        throw std::runtime_error("Environment map must be a 2:1 equirectangular panorama: " +
                                                 EnvironmentPath.string());
                    EnvironmentTexture = std::make_unique<Asset::FTextureAsset>(Asset::InvalidAssetHandle,
                                                                                "Surface Simulation environment",
                                                                                EnvironmentPath,
                                                                                Context,
                                                                                Panorama.Width,
                                                                                Panorama.Height,
                                                                                Panorama.Pixels,
                                                                                VK_FORMAT_R16G16B16A16_SFLOAT);
                }
                PrepareDisplaySets(Geometry);
                if (Window)
                {
                    Swapchain = std::make_unique<GPU::FSwapchain>(Context, *Window);
                    Frame = std::make_unique<Rendering::FRenderContext>(Context);
                    TargetExtent = Swapchain->GetExtent();
                    TargetFormat = Swapchain->GetImageFormat();
                    Targets = std::make_unique<FTargets>(Context, TargetExtent, TargetFormat, Swapchain.get(), SetLayout);
                }
            }
            catch (...)
            {
                ReleaseDescriptors();
                throw;
            }
        }

        FSurfaceSimulationDisplay::FImpl::~FImpl()
        {
            vkDeviceWaitIdle(Device);
            if (EditorTexture)
                ImGui_ImplVulkan_RemoveTexture(EditorTexture);
            Optics.reset();
            Targets.reset();
            ReleaseDescriptors();
        }

        void FSurfaceSimulationDisplay::FImpl::PrepareDisplaySets(const Liquid::FSurfaceSimulationDomain &Geometry)
        {
            const auto                                        Initial = Liquid::InitialState(Geometry);
            const auto                                        Coatings = Liquid::MakeFilmDisplay(Geometry, Initial);
            const std::array<VkDescriptorSetLayoutBinding, 9> Bindings{
                {{0,
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        nullptr},
                    {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {4,
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        nullptr},
                    {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
            VkDescriptorSetLayoutCreateInfo SetInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            SetInfo.bindingCount = static_cast<std::uint32_t>(Bindings.size());
            SetInfo.pBindings = Bindings.data();
            Check(vkCreateDescriptorSetLayout(Device, &SetInfo, nullptr, &SetLayout),
                  "Failed to create water display layout.");
            const std::array<VkDescriptorPoolSize, 2> PoolSizes{
                {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 14}}};
            VkDescriptorPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            PoolInfo.maxSets = 2;
            PoolInfo.poolSizeCount = static_cast<std::uint32_t>(PoolSizes.size());
            PoolInfo.pPoolSizes = PoolSizes.data();
            Check(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &DescriptorPool),
                  "Failed to create water display pool.");

            const std::array            Layouts{SetLayout, SetLayout};
            VkDescriptorSetAllocateInfo Allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            Allocate.descriptorPool = DescriptorPool;
            Allocate.descriptorSetCount = 2;
            Allocate.pSetLayouts = Layouts.data();
            Check(vkAllocateDescriptorSets(Device, &Allocate, Sets.data()), "Failed to allocate water display slots.");
            const VkDescriptorImageInfo TextureInfo{SurfaceTexture->GetSampler(),
                                                    SurfaceTexture->GetImageView(),
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            // 노멀 자산이 없는 재질도 유효한 descriptor를 갖는다. 강도가 0이면 노멀을 샘플링하지 않는다.
            const auto                 &Normal = SurfaceNormalTexture ? *SurfaceNormalTexture : *SurfaceTexture;
            const VkDescriptorImageInfo NormalInfo{Normal.GetSampler(),
                                                   Normal.GetImageView(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            // 환경이 없는 씬도 유효한 descriptor를 제공한다. 그 경로의 셰이더는 환경 텍스처를 읽지 않는다.
            const auto                 &Environment = EnvironmentTexture ? *EnvironmentTexture : *SurfaceTexture;
            const VkDescriptorImageInfo EnvironmentInfo{Environment.GetSampler(),
                                                        Environment.GetImageView(),
                                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            // 눈 맵이 없는 씬도 유효한 자원을 연결한다. 해당 패스는 이 자원을 읽지 않는다.
            const auto SnowInfo = [&](const auto &Map)
            {
                const auto &Texture = Map ? *Map : *SurfaceTexture;
                return VkDescriptorImageInfo{Texture.GetSampler(),
                                             Texture.GetImageView(),
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            };
            const auto                           SnowColorInfo = SnowInfo(SnowColorTexture);
            const auto                           SnowNormalInfo = SnowInfo(SnowNormalTexture);
            const Liquid::FSurfaceSnapshotLayout Layout(Initial.Cells.size(),
                                                        Initial.Regions.size(),
                                                        OverflowInstances / 2,
                                                        Geometry.FilmLayers.size() > 1 ? Geometry.FilmLayers.size() : 0);
            for (std::size_t Index = 0; Index < Display.size(); ++Index)
            {
                auto &Buffer = Display[Index];
                Buffer = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                           Device,
                                                           Layout.Size,
                                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                           HostMemory,
                                                           GPU::EGPUBufferMemoryCategory::SurfaceRuntime);
                Buffer->Upload(Initial.Cells.data(), Initial.Cells.size() * sizeof(Liquid::FSurfaceCellState));
                Buffer->Upload(Initial.Regions.data(),
                               Initial.Regions.size() * sizeof(Liquid::FPrimaryLiquidRegionState),
                               Layout.RegionOffset);
                const std::vector<std::array<float, 4>> Contacts(Initial.Regions.size());
                Buffer->Upload(Contacts.data(), Contacts.size() * sizeof(Contacts.front()), Layout.ContactOffset);
                if (Geometry.FilmLayers.size() > 1)
                {
                    Buffer->Upload(Coatings.data(), Coatings.size() * sizeof(Coatings.front()), Layout.FilmOffset);
                    Buffer->Upload(Initial.Film.data(),
                                   Initial.Film.size() * sizeof(Initial.Film.front()),
                                   Layout.FilmStateOffset);
                }
                const VkDescriptorBufferInfo BufferInfo{Buffer->GetHandle(), 0, Buffer->GetSize()};
                VkWriteDescriptorSet         Write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                Write.dstSet = Sets[Index];
                Write.descriptorCount = 1;
                Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Write.pBufferInfo = &BufferInfo;
                VkWriteDescriptorSet TextureWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                TextureWrite.dstSet = Sets[Index];
                TextureWrite.dstBinding = 3;
                TextureWrite.descriptorCount = 1;
                TextureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                TextureWrite.pImageInfo = &TextureInfo;
                const VkDescriptorBufferInfo FieldInfo{SurfaceField->GetHandle(), 0, SurfaceField->GetSize()};
                VkWriteDescriptorSet         FieldWrite = Write;
                FieldWrite.dstBinding = 4;
                FieldWrite.pBufferInfo = &FieldInfo;
                VkWriteDescriptorSet NormalWrite = TextureWrite;
                NormalWrite.dstBinding = 5;
                NormalWrite.pImageInfo = &NormalInfo;
                VkWriteDescriptorSet EnvironmentWrite = TextureWrite;
                EnvironmentWrite.dstBinding = 6;
                EnvironmentWrite.pImageInfo = &EnvironmentInfo;
                VkWriteDescriptorSet SnowColorWrite = TextureWrite;
                SnowColorWrite.dstBinding = 7;
                SnowColorWrite.pImageInfo = &SnowColorInfo;
                VkWriteDescriptorSet SnowNormalWrite = TextureWrite;
                SnowNormalWrite.dstBinding = 8;
                SnowNormalWrite.pImageInfo = &SnowNormalInfo;
                const std::array
                    Writes{Write, TextureWrite, FieldWrite, NormalWrite, EnvironmentWrite, SnowColorWrite, SnowNormalWrite};
                vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
            }
        }

        void FSurfaceSimulationDisplay::FImpl::ReleaseDescriptors() noexcept
        {
            vkDestroySampler(Device, OpticsSampler, nullptr);
            vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(Device, SetLayout, nullptr);
        }

    #pragma endregion

    // ============================================================
    // 02. Frame Process
    // ============================================================
    #pragma region 02_Frame_Process
        // 독립 창과 에디터의 프레임 진입점은 같은 시뮬레이션 및 그리기 기록을 사용한다.

        bool FSurfaceSimulationDisplay::FImpl::UsesRefraction(const FSurfaceSimulationViews &Views)
        {
            const auto Modes = std::span(Views.Modes).first(static_cast<std::size_t>(Views.Count));
            return std::ranges::any_of(Modes,
                                       [](int Mode)
                                       {
                                           return Mode == static_cast<int>(ESurfaceSimulationView::Refraction) ||
                                                  Mode == static_cast<int>(ESurfaceSimulationView::Pbr);
                                       });
        }

        bool FSurfaceSimulationDisplay::FImpl::DrawFrame(Liquid::FSurfaceSimulationGpu             &Simulation,
                                                         std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                         const FSurfaceSimulationViews             &Views,
                                                         const std::filesystem::path               &Capture)
        {
            if (UsesRefraction(Views) && !Optics)
                PrepareOptics();
            Frame->WaitForCurrentFrame();
            std::uint32_t  ImageIndex = 0;
            const VkResult Acquired = vkAcquireNextImageKHR(Device,
                                                            Swapchain->GetHandle(),
                                                            std::numeric_limits<std::uint64_t>::max(),
                                                            Frame->GetImageAvailableSemaphore(),
                                                            VK_NULL_HANDLE,
                                                            &ImageIndex);
            if (Acquired == VK_ERROR_OUT_OF_DATE_KHR)
            {
                Recreate();
                return false;
            }
            if (Acquired != VK_SUBOPTIMAL_KHR)
                Check(Acquired, "Failed to acquire water swapchain image.");

            const auto Commands = Frame->GetCurrentCommandBuffer();
            Check(vkResetCommandBuffer(Commands, 0), "Failed to reset water command buffer.");
            const VkCommandBufferBeginInfo Begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            Check(vkBeginCommandBuffer(Commands, &Begin), "Failed to begin water command buffer.");
            RecordSceneFrame(Commands, Frame->GetCurrentFrameIndex(), ImageIndex, Simulation, Inputs, Views);
            const auto                       Extent = TargetExtent;
            const auto                       Format = TargetFormat;
            std::unique_ptr<GPU::FGPUBuffer> Readback;
            if (!Capture.empty() && CanCapture())
            {
                Readback = MakeReadback(TargetExtent);
                RecordCapture(Commands, ImageIndex, Readback->GetHandle(), {{0, 0}, TargetExtent});
            }
            Check(vkEndCommandBuffer(Commands), "Failed to end water command buffer.");
            Present(ImageIndex);
            if (Readback)
            {
                Check(vkDeviceWaitIdle(Device), "Failed to finish screenshot readback.");
                SaveCapture(*Readback, Extent, Format, Capture);
            }
            AdvanceSnapshot(Inputs);
            return true;
        }

        void FSurfaceSimulationDisplay::FImpl::PrepareBedDisplay(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                                                 const Rendering::FSurfaceSimulationAppearance &Appearance,
                                                                 const FSurfaceSimulationViews                 &Views)
        {
            const bool bSmooth = SmoothBed(Views);
            if (!BedMeshes[bSmooth].Vertices)
                BuildBedMesh(Geometry,
                             MakeSurfaceRenderField(Geometry, Appearance),
                             bSmooth,
                             Appearance.DisplayThicknessMeters);
        }

        void FSurfaceSimulationDisplay::FImpl::BuildBedMesh(const Liquid::FSurfaceSimulationDomain &Geometry,
                                                            const FSurfaceRenderField              &Field,
                                                            bool                                    bSmooth,
                                                            float                                   ThicknessMeters)
        {
            auto     Mesh = MakeSurfaceSimulationMesh(Geometry, Field, bSmooth, ThicknessMeters);
            FBedMesh Bed;
            Bed.TopIndexCount = Mesh.TopIndexCount;
            Bed.SolidIndexCount = Mesh.SolidIndexCount;
            Bed.EffectIndexCount = Mesh.EffectIndexCount;
            Bed.BackdropIndexCount = Mesh.BackdropIndexCount;
            Bed.SectionIndexCount = Mesh.SectionIndexCount;
            Bed.SectionCutY = Field.SectionWindow.x;
            if (Field.SectionWindow.w > 0.F)
                Bed.VesselBounds = Field.VesselBounds;
            Bed.Vertices = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                             Device,
                                                             Mesh.Vertices.size() * sizeof(FSurfaceSimulationVertex),
                                                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                             HostMemory,
                                                             GPU::EGPUBufferMemoryCategory::Renderer);
            Bed.Indices = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                            Device,
                                                            Mesh.Indices.size() * sizeof(std::uint32_t),
                                                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                            HostMemory,
                                                            GPU::EGPUBufferMemoryCategory::Renderer);
            Bed.Vertices->Upload(Mesh.Vertices.data(), Bed.Vertices->GetSize());
            Bed.Indices->Upload(Mesh.Indices.data(), Bed.Indices->GetSize());
            if (!Window)
            {
                Mesh.Indices.resize(Mesh.SolidIndexCount);
                // GPU와 동일한 이동을 CPU 선택 메시에도 적용한다. 셀과 저장 영역 ID는 그대로 둔다.
                if (DisplayGapMeters > 0.F)
                    for (auto &Vertex : Mesh.Vertices)
                    {
                        const int Specimen = Geometry.Cells[Vertex.Cell].FirstColumn / (PickColumns / SpecimenCount);
                        Vertex.Position.x += (Specimen - 0.5F * (SpecimenCount - 1)) * DisplayGapMeters;
                    }
                Bed.Picking = std::move(Mesh);
                CacheInletOverlay(Bed, Geometry);
                // 두 표시 메시에서 +X 출구의 테두리와 주입 선택 위치가 각각 화면 바닥과 일치해야 한다.
                for (const auto &Face : Geometry.Faces)
                {
                    if (Face.Right >= 0)
                        continue;
                    for (const int Contact :
                         Liquid::FIndexRange{Face.FirstContact, Face.ContactCount}.In(Geometry.Contacts))
                    {
                        const auto *Quad = Bed.Picking.Vertices.data() + 4 * (Contact / 2);
                        Bed.OutletEdges.push_back({Quad[1].Position, Quad[2].Position});
                    }
                }
            }
            BedMeshes[bSmooth] = std::move(Bed);
        }

        VkDescriptorSet FSurfaceSimulationDisplay::FImpl::PrepareEditorTarget(VkExtent2D                     Extent,
                                                                              const FSurfaceSimulationViews &Views)
        {
            if (Extent.width != TargetExtent.width || Extent.height != TargetExtent.height)
            {
                auto Replacement = std::make_unique<FTargets>(Context, Extent, TargetFormat, nullptr, SetLayout);
                if (EditorTexture)
                {
                    // 이전 프레임의 ImGui가 같은 descriptor와 이미지 뷰를 읽을 수 있다.
                    Check(vkDeviceWaitIdle(Device), "Failed to drain resized editor viewport.");
                    ImGui_ImplVulkan_RemoveTexture(EditorTexture);
                }
                Optics.reset();
                Targets = std::move(Replacement);
                TargetExtent = Extent;
                EditorTexture =
                    ImGui_ImplVulkan_AddTexture(Targets->ColorView->GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            if (UsesRefraction(Views) && !Optics)
                PrepareOptics();
            return EditorTexture;
        }

        void FSurfaceSimulationDisplay::FImpl::RecordEditorFrame(VkCommandBuffer                            Commands,
                                                                 std::uint32_t                              FrameSlot,
                                                                 Liquid::FSurfaceSimulationGpu             &Simulation,
                                                                 std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                                 const FSurfaceSimulationViews             &Views,
                                                                 const std::filesystem::path               &Capture,
                                                                 int                                        CaptureView)
        {
            CollectTemperatureReadback(FrameSlot);
            RecordSceneFrame(Commands, FrameSlot, 0, Simulation, Inputs, Views);
            using enum ESurfaceSimulationView;
            if (TemperatureStateIndex)
                for (int View = 0; View < Views.Count; ++View)
                {
                    const auto Mode = static_cast<ESurfaceSimulationView>(Views.Modes[View]);
                    if (Mode == Temperature || (Mode == StateHeatmap && Views.StateIndices[View] == *TemperatureStateIndex))
                    {
                        RecordTemperatureReadback(Commands, FrameSlot);
                        break;
                    }
                }
            if (!Capture.empty())
            {
                const auto Rect = CaptureView >= 0 && CaptureView < Views.Count
                                      ? SurfaceSimulationViewRect(TargetExtent, Views, CaptureView)
                                      : VkRect2D{{0, 0}, TargetExtent};
                EditorReadback = MakeReadback(Rect.extent);
                EditorCapture = Capture;
                EditorCaptureExtent = Rect.extent;
                RecordCapture(Commands, 0, EditorReadback->GetHandle(), Rect);
            }
            AdvanceSnapshot(Inputs);
        }

        void FSurfaceSimulationDisplay::FImpl::CollectTemperatureReadback(std::uint32_t FrameSlot)
        {
            auto &Readback = TemperatureReadbacks[FrameSlot];
            if (!Readback.bPending)
                return;

            // 호출자가 이 프레임 슬롯의 fence를 이미 기다렸으므로 별도 GPU 대기가 필요 없다.
            TemperatureCells.resize(TemperatureCellCount);
            Readback.Buffer->Download(TemperatureCells.data(), Readback.Buffer->GetSize());
            const auto [Minimum, Maximum] =
                std::ranges::minmax_element(TemperatureCells, {}, &Liquid::FSurfaceCellState::TemperatureK);
            LastTemperatureRangeK = glm::vec2{Minimum->TemperatureK, Maximum->TemperatureK};
            Readback.bPending = false;
        }

        void FSurfaceSimulationDisplay::FImpl::RecordTemperatureReadback(VkCommandBuffer Commands, std::uint32_t FrameSlot)
        {
            if (RequestedTemperatureStep == DisplaySteps[DisplaySlot])
                return;

            auto              &Readback = TemperatureReadbacks[FrameSlot];
            const VkDeviceSize Size = TemperatureCellCount * sizeof(Liquid::FSurfaceCellState);
            if (!Readback.Buffer)
                Readback.Buffer = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                                    Device,
                                                                    Size,
                                                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                                    HostMemory,
                                                                    GPU::EGPUBufferMemoryCategory::SurfaceRuntime);

            // 솔버의 최신 슬롯 대신 이번에 그린 표시 슬롯에서 계산 셀 구간만 복사한다.
            VkMemoryBarrier Snapshot{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            Snapshot.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Snapshot.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(Commands,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 1,
                                 &Snapshot,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr);
            const VkBufferCopy Copy{0, 0, Size};
            vkCmdCopyBuffer(Commands, Display[DisplaySlot]->GetHandle(), Readback.Buffer->GetHandle(), 1, &Copy);
            VkMemoryBarrier Host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            Host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            vkCmdPipelineBarrier(Commands,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT,
                                 0,
                                 1,
                                 &Host,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr);
            Readback.bPending = true;
            RequestedTemperatureStep = DisplaySteps[DisplaySlot];
        }

        void FSurfaceSimulationDisplay::FImpl::FinishEditorCapture()
        {
            if (!EditorReadback)
                return;
            Check(vkDeviceWaitIdle(Device), "Failed to finish editor viewport capture.");
            auto Readback = std::move(EditorReadback);
            SaveCapture(*Readback, EditorCaptureExtent, TargetFormat, EditorCapture);
        }

        void FSurfaceSimulationDisplay::FImpl::RecordSceneFrame(VkCommandBuffer                            Commands,
                                                                std::uint32_t                              FrameSlot,
                                                                std::uint32_t                              ImageIndex,
                                                                Liquid::FSurfaceSimulationGpu             &Simulation,
                                                                std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                                const FSurfaceSimulationViews             &Views)
        {
            // 이 슬롯이 완료된 뒤에 호출하므로 이전 그리기 시간은 대기 없이 읽을 수 있다.
            Timing.Collect(FrameSlot);
            const bool bRefraction = UsesRefraction(Views);
            Timing.Begin(Commands, FrameSlot, !Inputs.empty(), bRefraction);
            if (!Inputs.empty())
                Simulation.BeginDisplayInterval();
            for (const auto &Input : Inputs)
                Simulation
                    .RecordStep(Commands, Input.Pose, Input.InputMass, Input.Thermal, Input.Dose, Input.LocalInput, true);
            Timing.Mark(Commands, FrameSlot, ESurfaceSimulationTimestamp::SolverEnd);
            if (!Inputs.empty())
                Simulation.RecordDisplayCopy(Commands, Display[DisplaySlot ^ 1U]->GetHandle());
            Timing.Mark(Commands, FrameSlot, ESurfaceSimulationTimestamp::SnapshotEnd);
            RecordDraw(Commands, FrameSlot, ImageIndex, Views, bRefraction);
            LastDrawExtent = TargetExtent;
            LastDrawViews = Views;
            LastDrawPose = DisplayPose[DisplaySlot];
            Timing.Mark(Commands, FrameSlot, ESurfaceSimulationTimestamp::DrawEnd);
        }

        void FSurfaceSimulationDisplay::FImpl::AdvanceSnapshot(std::span<const Liquid::FSurfaceStepInput> Inputs)
        {
            if (Inputs.empty())
                return;
            // 자세의 속도 변화로 감쇠 진동을 구동한다. 표시 스냅샷과 같은 고정 스텝을 사용한다.
            constexpr float Seconds = Liquid::StepSeconds;
            constexpr float AngularFrequency = 13.F;
            constexpr float Damping = 5.F;
            constexpr float TiltImpulseGain = 85.F;
            auto            PreviousPose = DisplayPose[DisplaySlot];
            for (const auto &Input : Inputs)
            {
                const float TiltRate = (Input.Pose.Sine - PreviousPose.Sine) / Seconds;
                SloshVelocity = std::clamp(SloshVelocity + TiltImpulseGain * (TiltRate - PreviousTiltRate), -20.F, 20.F);
                SloshVelocity = (SloshVelocity - AngularFrequency * AngularFrequency * SloshDisplacement * Seconds) /
                                (1.F + 2.F * Damping * Seconds);
                SloshDisplacement = std::clamp(SloshDisplacement + SloshVelocity * Seconds, -1.F, 1.F);
                PreviousTiltRate = TiltRate;
                PreviousPose = Input.Pose;
            }

            // 수면과 바닥 자세가 같은 표시 슬롯을 읽어야 기울일 때 두 형상이 어긋나지 않는다.
            DisplaySteps[DisplaySlot ^ 1U] = DisplaySteps[DisplaySlot] + static_cast<std::uint32_t>(Inputs.size());
            DisplaySlot ^= 1U;
            DisplayPose[DisplaySlot] = Inputs.back().Pose;
            bHasOverflowSnapshot = true;
        }

        std::unique_ptr<GPU::FGPUBuffer> FSurfaceSimulationDisplay::FImpl::MakeReadback(VkExtent2D Extent) const
        {
            return std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                     Device,
                                                     VkDeviceSize(Extent.width) * Extent.height * 4U,
                                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                     HostMemory);
        }
    #pragma endregion

    // ============================================================
    // 03. Overlay and Refraction Setup
    // ============================================================
    #pragma region 03_Overlay_and_Refraction_Setup
        // 독립 창의 ImGui와 표시 대상의 굴절용 불투명 패스를 준비한다.

        void FSurfaceSimulationDisplay::FImpl::PrepareOptics()
        {
            Check(vkDeviceWaitIdle(Device), "Failed to drain water display before preparing optics.");
            if (!OpticsSampler)
            {
                VkSamplerCreateInfo Info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
                Info.magFilter = Info.minFilter = VK_FILTER_NEAREST;
                Info.addressModeU = Info.addressModeV = Info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                Check(vkCreateSampler(Device, &Info, nullptr, &OpticsSampler), "Failed to create refraction sampler.");
            }
            Optics =
                std::make_unique<FOpticsTargets>(Context, TargetExtent, TargetFormat, SetLayout, Targets->Pass.GetHandle());
            const std::array<VkDescriptorImageInfo, 2> Images{
                {{OpticsSampler, Optics->ColorView.GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                    {OpticsSampler, Optics->DepthView.GetHandle(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}}};
            for (const auto Set : Sets)
            {
                std::array<VkWriteDescriptorSet, 2> Writes{};
                for (std::uint32_t Index = 0; Index < Writes.size(); ++Index)
                {
                    auto &Write = Writes[Index];
                    Write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    Write.dstSet = Set;
                    Write.dstBinding = Index + 1;
                    Write.descriptorCount = 1;
                    Write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    Write.pImageInfo = &Images[Index];
                }
                vkUpdateDescriptorSets(Device, static_cast<std::uint32_t>(Writes.size()), Writes.data(), 0, nullptr);
            }
        }

    #pragma endregion

    // ============================================================
    // 04. Draw Recording
    // ============================================================
    #pragma region 04_Draw_Recording
        // 동일한 스냅샷으로 불투명 패스와 표시 뷰의 레이어를 기록한다.

        void FSurfaceSimulationDisplay::FImpl::RecordDraw(VkCommandBuffer                Commands,
                                                          std::uint32_t                  FrameSlot,
                                                          std::uint32_t                  ImageIndex,
                                                          const FSurfaceSimulationViews &Views,
                                                          bool                           bRefract) const
        {
            const auto                  Extent = TargetExtent;
            std::array<VkClearValue, 2> Clears{};
            Clears[0].color = {{0.020F, 0.031F, 0.046F, 1.F}};
            Clears[1].depthStencil = {1.F, 0};
            if (bRefract)
            {
                const GPU::FGraphicsPass Prepass(Optics->Pass);
                Prepass.Begin(Commands, Optics->Framebuffer, 0, Extent, Clears);
                RecordViews(Commands, Views, true);
                Prepass.End(Commands);
            }
            Timing.Mark(Commands, FrameSlot, ESurfaceSimulationTimestamp::RefractionPrepassEnd);
            const GPU::FGraphicsPass Pass(Targets->Pass);
            Pass.Begin(Commands, *Targets->Framebuffers, ImageIndex, Extent, Clears);
            RecordViews(Commands, Views, false);
            Pass.End(Commands);
        }

        VkRect2D SurfaceSimulationViewRect(VkExtent2D Extent, const FSurfaceSimulationViews &Views, int Index)
        {
            if (Views.bSplitLowerRow)
            {
                const auto LeftWidth = std::max(1U, Extent.width / 2);
                const auto TopHeight = std::max(1U, Extent.height / 2);
                if (Index == 0 || Extent.height == 1)
                    return {{0, 0}, {Extent.width, TopHeight}};
                const bool bRight = Index == 2 && Extent.width > 1;
                return {{bRight ? static_cast<std::int32_t>(LeftWidth) : 0, static_cast<std::int32_t>(TopHeight)},
                        {bRight ? Extent.width - LeftWidth : LeftWidth, Extent.height - TopHeight}};
            }
            const auto Columns = Views.Count > 1 ? 2U : 1U;
            const auto Rows = Views.Count > 2 ? 2U : 1U;
            const auto Width = std::max(1U, Extent.width / Columns);
            const auto Height = std::max(1U, Extent.height / Rows);
            return {
                {static_cast<std::int32_t>(Index % Columns * Width), static_cast<std::int32_t>(Index / Columns * Height)},
                {Width, Height}};
        }

        glm::mat4 FSurfaceSimulationDisplay::FImpl::ViewProjection(const FSurfaceSimulationViews &Views,
                                                                   VkExtent2D                     Extent,
                                                                   glm::vec3                     &Camera) const
        {
            const glm::vec3 Target{Views.CameraOffset, CameraTargetZ};
            const float     Azimuth = glm::radians(Views.CameraAzimuth), Elevation = glm::radians(Views.CameraElevation);
            const float     FitScale = std::max({SceneScale, Views.CameraFitSize.x / 0.09F, Views.CameraFitSize.y / 0.05F});
            Camera = Target + 0.1653F * FitScale * Views.CameraDistanceScale *
                                  glm::vec3(std::cos(Elevation) * std::cos(Azimuth),
                                            std::cos(Elevation) * std::sin(Azimuth),
                                            std::sin(Elevation));
            const float Aspect = static_cast<float>(Extent.width) / static_cast<float>(Extent.height);
            float       VerticalFov = 30.F * std::max(1.F, 1.35F / std::max(0.65F, Aspect));
            if (Views.CameraFitSize.x > 0.F)
            {
                // 좁게 나뉜 비교 뷰에서도 화면 가로에 시편 전체가 들어올 공간을 둔다.
                constexpr float MinimumHorizontalFov = 48.F;
                const float     FitFov =
                    glm::degrees(2.F * std::atan(std::tan(glm::radians(0.5F * MinimumHorizontalFov)) / Aspect));
                VerticalFov = std::max(VerticalFov, FitFov);
            }
            auto Projection = glm::perspectiveRH_ZO(glm::radians(VerticalFov), Aspect, 0.003F, 1.F);
            Projection[1][1] *= -1.F;
            return Projection * glm::lookAtRH(Camera, Target, glm::vec3{0.F, 0.F, 1.F});
        }

        std::optional<FSurfaceInjectionFootprint>
        FSurfaceSimulationDisplay::FImpl::PickInjectionFootprint(glm::vec2 Position, const FInjectionBrush &Brush) const
        {
            const auto Hit = PickBed(Position);
            if (!Hit)
                return std::nullopt;

            const auto Pose =
                glm::rotate(glm::mat4(1.F), std::atan2(LastDrawPose.Sine, LastDrawPose.Cosine), glm::vec3(0.F, 1.F, 0.F));
            auto Footprint =
                ProjectInjectionRegion(Hit->Region, Hit->View, Hit->Point, Brush, Hit->ViewProjection * Pose, Hit->Rect);
            if (Footprint.Regions.empty())
                return std::nullopt;
            return Footprint;
        }

        std::optional<float> FSurfaceSimulationDisplay::FImpl::PickTemperatureK(glm::vec2 Position) const
        {
            if (TemperatureCells.empty())
                return std::nullopt;
            const auto Hit = PickBed(Position);
            if (Hit && Hit->Cell < TemperatureCells.size())
                return TemperatureCells[Hit->Cell].TemperatureK;
            return std::nullopt;
        }

        std::optional<FSurfaceSimulationDisplay::FImpl::FBedHit>
        FSurfaceSimulationDisplay::FImpl::PickBed(glm::vec2 Position) const
        {
            if (LastDrawExtent.width == 0 || LastDrawExtent.width != TargetExtent.width ||
                LastDrawExtent.height != TargetExtent.height)
                return std::nullopt;

            const auto     &Bed = BedMeshes[SmoothBed(LastDrawViews)];
            const auto     &PickMesh = Bed.Picking;
            const glm::vec2 Pixel = Position * glm::vec2(LastDrawExtent.width, LastDrawExtent.height);
            for (int Index = 0; Index < LastDrawViews.Count; ++Index)
            {
                const auto Rect = SurfaceSimulationViewRect(LastDrawExtent, LastDrawViews, Index);
                const auto UV =
                    (Pixel - glm::vec2(Rect.offset.x, Rect.offset.y)) / glm::vec2(Rect.extent.width, Rect.extent.height);
                if (UV.x < 0.F || UV.x >= 1.F || UV.y < 0.F || UV.y >= 1.F)
                    continue;

                glm::vec3  Camera;
                const auto Transform = ViewProjection(LastDrawViews, Rect.extent, Camera);
                const auto Inverse = glm::inverse(Transform);
                const auto Unproject = [&Inverse, UV](float Depth)
                {
                    const auto Point = Inverse * glm::vec4(UV * 2.F - 1.F, Depth, 1.F);
                    return glm::vec3(Point) / Point.w;
                };
                const auto ToLocal = [Pose = LastDrawPose](glm::vec3 Point)
                {
                    return glm::vec3(Pose.Cosine * Point.x - Pose.Sine * Point.z,
                                     Point.y,
                                     Pose.Sine * Point.x + Pose.Cosine * Point.z);
                };
                const auto Origin = ToLocal(Unproject(0.F));
                const auto Direction = ToLocal(Unproject(1.F)) - Origin;
                const auto Mode = static_cast<ESurfaceSimulationView>(LastDrawViews.Modes[Index]);
                const bool bSectionView = Bed.SectionIndexCount > 0 && (Mode < ESurfaceSimulationView::LayerThickness ||
                                                                        Mode == ESurfaceSimulationView::Pbr);
                float      Best = 1.F;
                const FSurfaceSimulationVertex *HitVertex = nullptr;
                for (std::size_t Triangle = 0; Triangle < PickMesh.Indices.size(); Triangle += 3)
                {
                    const auto &A = PickMesh.Vertices[PickMesh.Indices[Triangle]];
                    const auto &B = PickMesh.Vertices[PickMesh.Indices[Triangle + 1]];
                    const auto &C = PickMesh.Vertices[PickMesh.Indices[Triangle + 2]];
                    glm::vec2   Barycentric;
                    float       Distance;
                    if (!glm::intersectRayTriangle(Origin,
                                                   Direction,
                                                   A.Position,
                                                   B.Position,
                                                   C.Position,
                                                   Barycentric,
                                                   Distance) ||
                        Distance < 0.F || Distance >= Best)
                        continue;
                    if (bSectionView)
                    {
                        const glm::vec2 Hit(Origin + Distance * Direction);
                        if (const auto &Bounds = Bed.VesselBounds)
                        {
                            if (Hit.x < Bounds->x || Hit.x > Bounds->z || Hit.y < Bounds->y || Hit.y > Bounds->w)
                                continue;
                        }
                        else if (Hit.y < Bed.SectionCutY)
                            continue;
                    }
                    Best = Distance;
                    HitVertex = Triangle < PickMesh.TopIndexCount ? &A : nullptr;
                }
                if (!HitVertex)
                    return std::nullopt;
                return FBedHit{glm::vec2(Origin + Best * Direction),
                               Transform,
                               Rect,
                               Index,
                               HitVertex->Cell,
                               HitVertex->Region};
            }
            return std::nullopt;
        }

            // ============================================================
            // Surface Brush Coverage
            // ============================================================
        #pragma region Surface_Brush_Coverage
            namespace
            {
                constexpr int CircleSides = 32;
                using FBrushPolygon = std::array<glm::dvec2, CircleSides + 4>;

                int MakeBrushPolygon(glm::dvec2 Center, const FInjectionBrush &Brush, FBrushPolygon &Polygon)
                {
                    if (Brush.Shape == EInjectionShape::Point)
                        return 0;
                    if (Brush.Shape == EInjectionShape::Circle)
                    {
                        for (int Index = 0; Index < CircleSides; ++Index)
                        {
                            const double Angle = 2.0 * std::numbers::pi * Index / CircleSides;
                            Polygon[Index] =
                                Center + static_cast<double>(Brush.RadiusM) * glm::dvec2{std::cos(Angle), std::sin(Angle)};
                        }
                        return CircleSides;
                    }
                    const glm::dvec2 Half = glm::dvec2(Brush.SizeM) * 0.5;
                    const double     Sine = std::sin(Brush.AngleRadians), Cosine = std::cos(Brush.AngleRadians);
                    const std::array Corners{glm::dvec2{-Half.x, -Half.y},
                                             glm::dvec2{Half.x, -Half.y},
                                             Half,
                                             glm::dvec2{-Half.x, Half.y}};
                    for (std::size_t Index = 0; Index < Corners.size(); ++Index)
                    {
                        const auto P = Corners[Index];
                        Polygon[Index] = Center + glm::dvec2{Cosine * P.x - Sine * P.y, Sine * P.x + Cosine * P.y};
                    }
                    return static_cast<int>(Corners.size());
                }

                double BrushOverlap(FBrushPolygon Polygon, int Count, glm::dvec2 Min, glm::dvec2 Max)
                {
                    FBrushPolygon Output;
                    for (int Edge = 0; Edge < 4; ++Edge)
                    {
                        if (Count < 3)
                            return 0.0;
                        const int    Axis = Edge / 2;
                        const bool   bMinimum = Edge % 2 == 0;
                        const double Limit = bMinimum ? Min[Axis] : Max[Axis];
                        int          ClippedCount = 0;
                        for (int Index = 0; Index < Count; ++Index)
                        {
                            const auto A = Polygon[Index];
                            const auto B = Polygon[(Index + 1) % Count];
                            const bool bInA = bMinimum ? A[Axis] >= Limit : A[Axis] <= Limit;
                            const bool bInB = bMinimum ? B[Axis] >= Limit : B[Axis] <= Limit;
                            if (bInA)
                                Output[ClippedCount++] = A;
                            if (bInA != bInB)
                                Output[ClippedCount++] = A + ((Limit - A[Axis]) / (B[Axis] - A[Axis])) * (B - A);
                        }
                        Count = ClippedCount;
                        std::copy_n(Output.begin(), Count, Polygon.begin());
                    }
                    double TwiceArea = 0.0;
                    for (int I = 1; I + 1 < Count; ++I)
                    {
                        const auto A = Polygon[I] - Polygon[0];
                        const auto B = Polygon[I + 1] - Polygon[0];
                        TwiceArea += A.x * B.y - A.y * B.x;
                    }
                    return std::abs(TwiceArea) * 0.5;
                }
            } // namespace
        #pragma endregion

        FSurfaceInjectionFootprint FSurfaceSimulationDisplay::FImpl::ProjectInjectionRegion(std::uint32_t          Region,
                                                                                            int                    View,
                                                                                            glm::vec2              Center,
                                                                                            const FInjectionBrush &Brush,
                                                                                            const glm::mat4 &Transform,
                                                                                            VkRect2D         Rect) const
        {
            FSurfaceInjectionFootprint Result;
            Result.View = View;
            const auto   &PickMesh = BedMeshes[SmoothBed(LastDrawViews)].Picking;
            const auto    Quad = [&](int X, int Y) { return PickMesh.Vertices.data() + 4 * (Y * PickColumns + X); };
            const bool    bPoint = Brush.Shape == EInjectionShape::Point;
            FBrushPolygon Polygon{};
            const int     Count = MakeBrushPolygon(Center, Brush, Polygon);
            const float   Extent = Brush.Shape == EInjectionShape::Circle ? Brush.RadiusM : 0.5F * glm::length(Brush.SizeM);
            std::vector<float>  Coverage(static_cast<std::size_t>(PickColumns) * PickRows);
            std::vector<double> Areas(PickRegionCount);
            double              Total = 0.;
            for (int Y = 0; Y < PickRows; ++Y)
                for (int X = 0; X < PickColumns; ++X)
                {
                    const auto      *QuadVertices = Quad(X, Y);
                    const glm::dvec2 Min = glm::vec2(QuadVertices[0].Position);
                    const glm::dvec2 Max = glm::vec2(QuadVertices[2].Position);
                    const double     Area = (Max.x - Min.x) * (Max.y - Min.y);
                    if (bPoint ? QuadVertices->Region != Region
                               : Center.x + Extent < Min.x || Center.x - Extent > Max.x || Center.y + Extent < Min.y ||
                                     Center.y - Extent > Max.y)
                        continue;
                    const double Overlap = bPoint ? Area : BrushOverlap(Polygon, Count, Min, Max);
                    Coverage[Y * PickColumns + X] = static_cast<float>(std::clamp(Overlap / Area, 0.0, 1.0));
                    Areas[QuadVertices->Region] += Overlap;
                    Total += Overlap;
                }
            if (Total <= 0.)
                return Result;

            for (int Index = 0; Index < static_cast<int>(Areas.size()); ++Index)
                if (Areas[Index] > 0.)
                    Result.Regions.push_back({Index, static_cast<float>(Areas[Index] / Total)});

            const auto Affected = [&](int X, int Y)
            { return X >= 0 && X < PickColumns && Y >= 0 && Y < PickRows && Coverage[Y * PickColumns + X] > 0.F; };
            const glm::vec2                     ImageSize(LastDrawExtent.width, LastDrawExtent.height);
            const glm::vec2                     ViewSize(Rect.extent.width, Rect.extent.height);
            const glm::vec2                     ViewOrigin(Rect.offset.x, Rect.offset.y);
            constexpr std::array<glm::ivec2, 4> Adjacent{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
            for (int Y = 0; Y < PickRows; ++Y)
                for (int X = 0; X < PickColumns; ++X)
                {
                    if (!Affected(X, Y))
                        continue;
                    FSurfaceInjectionFootprint::FSample Sample{};
                    Sample.Coverage = Coverage[Y * PickColumns + X];
                    const auto *QuadVertices = Quad(X, Y);
                    bool        bInFront = true;
                    for (std::size_t I = 0; I < Sample.Corners.size(); ++I)
                    {
                        const auto Clip = Transform * glm::vec4(QuadVertices[I].Position, 1.F);
                        if (Clip.w <= 0.F)
                        {
                            bInFront = false;
                            break;
                        }
                        Sample.Corners[I] = (ViewOrigin + (glm::vec2(Clip) / Clip.w * 0.5F + 0.5F) * ViewSize) / ImageSize;
                        if (CrossesDisplayGap(X, X + Adjacent[I].x) || !Affected(X + Adjacent[I].x, Y + Adjacent[I].y))
                            Sample.BoundaryEdges |= 1U << I;
                    }
                    if (bInFront)
                        Result.Samples.push_back(Sample);
                }
            return Result;
        }

        void FSurfaceSimulationDisplay::FImpl::CacheInletOverlay(FBedMesh                               &Bed,
                                                                 const Liquid::FSurfaceSimulationDomain &Geometry) const
        {
            const auto &Weights = Geometry.ScheduledInletWeights;
            const auto  Source = static_cast<std::uint32_t>(Geometry.SourceRegion());
            const auto  ReceivesInlet = [&](int X, int Y)
            {
                if (X < 0 || X >= PickColumns || Y < 0 || Y >= PickRows)
                    return false;
                const auto Region = Geometry.SampleRegions[Y * PickColumns + X];
                return Weights.empty() ? Region == Source : Weights[Region] > 0.F;
            };
            constexpr std::array<glm::ivec2, 4> Neighbors{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
            for (int Y = 0; Y < PickRows; ++Y)
                for (int X = 0; X < PickColumns; ++X)
                {
                    if (!ReceivesInlet(X, Y))
                        continue;
                    const auto FirstVertex = static_cast<std::size_t>(4 * (Y * PickColumns + X));
                    Bed.InletQuads.push_back(FirstVertex);
                    const auto *Quad = Bed.Picking.Vertices.data() + FirstVertex;
                    for (std::size_t I = 0; I < Neighbors.size(); ++I)
                        if (CrossesDisplayGap(X, X + Neighbors[I].x) ||
                            !ReceivesInlet(X + Neighbors[I].x, Y + Neighbors[I].y))
                            Bed.InletEdges.push_back({Quad[I].Position, Quad[(I + 1) % Neighbors.size()].Position});
                }
        }

        glm::mat4 FSurfaceSimulationDisplay::FImpl::OverlayTransform(const FSurfaceSimulationViews &Views, int View) const
        {
            glm::vec3  Camera;
            const auto Rect = SurfaceSimulationViewRect(TargetExtent, Views, View);
            const auto Pose = DisplayPose[DisplaySlot];
            const auto Rotation = glm::rotate(glm::mat4(1.F), std::atan2(Pose.Sine, Pose.Cosine), glm::vec3(0.F, 1.F, 0.F));
            return ViewProjection(Views, Rect.extent, Camera) * Rotation;
        }

        // ============================================================
        // 04.01. Overlay Projection
        // ============================================================
        #pragma region 04_01_Overlay_Projection
            namespace
            {
                template <std::size_t Count>
                void AppendProjectedOverlay(const glm::mat4                           &Transform,
                                            const std::array<glm::vec3, Count>        &Positions,
                                            std::vector<std::array<glm::vec2, Count>> &Result)
                {
                    std::array<glm::vec2, Count> UVs;
                    for (std::size_t Index = 0; Index < Count; ++Index)
                    {
                        const auto Clip = Transform * glm::vec4(Positions[Index], 1.F);
                        if (Clip.z < 0.F || Clip.w <= 0.F)
                            return;
                        UVs[Index] = glm::vec2(Clip) / Clip.w * 0.5F + 0.5F;
                    }
                    Result.push_back(UVs);
                }
            } // namespace
        #pragma endregion

        std::vector<std::array<glm::vec2, 2>>
        FSurfaceSimulationDisplay::FImpl::ProjectOutletEdge(const FSurfaceSimulationViews &Views, int View) const
        {
            const auto &Edges = BedMeshes[SmoothBed(LastDrawViews)].OutletEdges;
            if (Edges.empty() || TargetExtent.width == 0 || TargetExtent.height == 0)
                return {};
            const auto                            Transform = OverlayTransform(Views, View);
            std::vector<std::array<glm::vec2, 2>> Result;
            Result.reserve(Edges.size());
            for (const auto &Edge : Edges)
                AppendProjectedOverlay(Transform, Edge, Result);
            return Result;
        }

        FSurfaceInletOverlay FSurfaceSimulationDisplay::FImpl::ProjectInletOverlay(const FSurfaceSimulationViews &Views,
                                                                                   int View) const
        {
            const auto &Bed = BedMeshes[SmoothBed(LastDrawViews)];
            if (Bed.InletQuads.empty() || TargetExtent.width == 0 || TargetExtent.height == 0)
                return {};
            const auto           Transform = OverlayTransform(Views, View);
            FSurfaceInletOverlay Result;
            Result.Quads.reserve(Bed.InletQuads.size());
            Result.Edges.reserve(Bed.InletEdges.size());
            for (const auto FirstVertex : Bed.InletQuads)
            {
                const auto *Quad = Bed.Picking.Vertices.data() + FirstVertex;
                AppendProjectedOverlay(Transform,
                                       std::array{Quad[0].Position, Quad[1].Position, Quad[2].Position, Quad[3].Position},
                                       Result.Quads);
            }
            for (const auto &Edge : Bed.InletEdges)
                AppendProjectedOverlay(Transform, Edge, Result.Edges);
            return Result;
        }

        void FSurfaceSimulationDisplay::FImpl::RecordWorldReference(VkCommandBuffer                Commands,
                                                                    const FSurfaceSimulationViews &Views,
                                                                    const glm::mat4               &ViewProjection) const
        {
            if (!WorldReferenceVertices || (!Views.bWorldGrid && !Views.bWorldAxes))
                return;

            const auto                          &Pipeline = *Targets->WorldReference;
            const VkBuffer                       Buffer = WorldReferenceVertices->GetHandle();
            constexpr VkDeviceSize               Offset = 0;
            constexpr float                      Centimeter = 0.01F;
            const Rendering::FGizmoPushConstants Push{glm::scale(ViewProjection, glm::vec3(Centimeter))};
            vkCmdBindPipeline(Commands, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetHandle());
            vkCmdBindVertexBuffers(Commands, 0, 1, &Buffer, &Offset);
            vkCmdPushConstants(Commands, Pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &Push);
            if (Views.bWorldGrid)
                vkCmdDraw(Commands, GridVertexCount, 1, 0, 0);
            if (Views.bWorldAxes)
                vkCmdDraw(Commands, AxisVertexCount, 1, GridVertexCount, 0);
        }

        void FSurfaceSimulationDisplay::FImpl::RecordViews(VkCommandBuffer                Commands,
                                                           const FSurfaceSimulationViews &Views,
                                                           bool                           bPrepass) const
        {
            const bool         bSmoothBed = SmoothBed(Views);
            const auto        &Bed = BedMeshes[bSmoothBed];
            const VkDeviceSize Offset = 0;
            const VkBuffer     MeshBuffer = Bed.Vertices->GetHandle();
            vkCmdBindIndexBuffer(Commands, Bed.Indices->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
            for (int Index = 0; Index < Views.Count; ++Index)
            {
                const auto Mode = static_cast<std::uint32_t>(Views.Modes[Index]);
                const bool bTextures =
                    Views.TextureOverrides[Index] < 0 ? Views.bTextures : Views.TextureOverrides[Index] != 0;
                const bool bEffects = Views.EffectOverrides[Index] < 0 ? Views.bEffects : Views.EffectOverrides[Index] != 0;
                const auto Palette = Views.PaletteOverrides[Index] < 0 ? Views.Palette : Views.PaletteOverrides[Index];
                const bool bRefractionView = Mode == static_cast<std::uint32_t>(ESurfaceSimulationView::Refraction) ||
                                             Mode == static_cast<std::uint32_t>(ESurfaceSimulationView::Pbr);
                if (bPrepass && !bRefractionView)
                    continue;
                const auto       Scissor = SurfaceSimulationViewRect(TargetExtent, Views, Index);
                const auto       Width = Scissor.extent.width, Height = Scissor.extent.height;
                const VkViewport Viewport{static_cast<float>(Scissor.offset.x),
                                          static_cast<float>(Scissor.offset.y),
                                          static_cast<float>(Width),
                                          static_cast<float>(Height),
                                          0.F,
                                          1.F};
                vkCmdSetViewport(Commands, 0, 1, &Viewport);
                vkCmdSetScissor(Commands, 0, 1, &Scissor);
                vkCmdBindVertexBuffers(Commands, 0, 1, &MeshBuffer, &Offset);

                glm::vec3  Camera;
                const auto Transform = ViewProjection(Views, Scissor.extent, Camera);
                const bool bLiquidView =
                    Mode <= static_cast<std::uint32_t>(ESurfaceSimulationView::LayerThickness) || bRefractionView;
                const bool bSloshView =
                    (Mode == static_cast<std::uint32_t>(ESurfaceSimulationView::Fresnel) || bRefractionView) &&
                    (AppearanceFlags & IncandescentLiquid) == 0U;
                std::uint32_t LiquidOptions = Views.SmoothWater[Index] && bLiquidView ? SmoothLiquidBoundary : 0U;
                if (bSloshView)
                {
                    LiquidOptions |= Views.SloshOptics[Index] ? SloshOptics : 0U;
                    LiquidOptions |= Views.SloshHeight[Index] && !bSmoothBed ? SloshHeight : 0U;
                    if ((LiquidOptions & (SloshOptics | SloshHeight)) != 0U)
                        LiquidOptions |= static_cast<std::uint32_t>(std::lround(32767.F * SloshDisplacement)) << 16U;
                }
                FDrawPush Push{
                    Transform,
                    glm::vec4(Camera, static_cast<float>(DisplaySteps[DisplaySlot]) * Liquid::StepSeconds),
                    glm::vec4(Tint, bTextures ? TextureRepeatsPerMeter : 0.F),
                    DisplayPose[DisplaySlot],
                    0U,
                    Mode,
                    static_cast<std::uint32_t>(Palette),
                    Width | (Height << 16U),
                    AppearanceFlags | (bHasFlame ? CombustionField : 0U) | (bEffects && bHasFlame ? FireLighting : 0U) |
                        (bSmoothBed && Views.bShowOverflow && bHasOverflowSnapshot && OverflowInstances > 0
                             ? OverflowFilmOnBed
                             : 0U) |
                        (Views.CellOutlines[Index] ? CellOutlineOverlay : 0U) | (Views.bStudioStrip ? 0U : NoStudioStrip) |
                        (Views.MaterialNormalMaps[Index] ? 0U : NoMaterialNormals) | (Views.StateIndices[Index] << 8U) |
                        (Views.bEnvironmentReflections && EnvironmentTexture ? EnvironmentReflections : 0U),
                    LiquidOptions};
                auto Bind = [&](const GPU::FGraphicsPipeline &Pipeline)
                {
                    const auto Layout = Pipeline.GetLayout();
                    vkCmdBindPipeline(Commands, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetHandle());
                    vkCmdBindDescriptorSets(Commands,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            Layout,
                                            0,
                                            1,
                                            &Sets[DisplaySlot],
                                            0,
                                            nullptr);
                    vkCmdPushConstants(Commands,
                                       Layout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0,
                                       sizeof(Push),
                                       &Push);
                };
                auto Draw = [&](const GPU::FGraphicsPipeline &Pipeline, std::uint32_t Count, std::uint32_t First)
                {
                    Bind(Pipeline);
                    vkCmdDrawIndexed(Commands, Count, 1, First, 0, 0);
                };
                const auto &Solid = bPrepass ? Optics->Opaque : *Targets->SolidPipeline;
                if (Bed.BackdropIndexCount > 0 && Mode == static_cast<std::uint32_t>(ESurfaceSimulationView::Pbr))
                {
                    Push.Layer = 3U;
                    Draw(Solid, Bed.BackdropIndexCount, Bed.SolidIndexCount + Bed.EffectIndexCount);
                    Push.Layer = 0U;
                }
                Draw(Solid, Bed.SolidIndexCount, 0);
                const bool bAppearanceView =
                    Mode < static_cast<std::uint32_t>(ESurfaceSimulationView::LayerThickness) || bRefractionView;
                if (bAppearanceView && bSnowLayer)
                {
                    // 굴절에도 적설의 실제 깊이가 들어가도록 불투명 단계에서 같은 윗면을 다시 그린다.
                    Push.Layer = SnowLayer;
                    Push.MaterialScale.a = bTextures ? SnowRepeatsPerMeter : 0.F;
                    Draw(Solid, Bed.TopIndexCount, 0);
                    Push.MaterialScale.a = bTextures ? TextureRepeatsPerMeter : 0.F;
                }
                if (bAppearanceView && (AppearanceFlags & LavaDeposit) != 0U)
                {
                    // 돌 표면이 굴절의 색과 깊이에 들어가도록 바닥과 같은 불투명 패스에 넣는다.
                    Push.Layer = LavaDepositLayer;
                    Draw(Solid, Bed.TopIndexCount, 0);
                }
                if (bAppearanceView && (AppearanceFlags & IncandescentLiquid) != 0U)
                {
                    // 수송으로 구한 높이를 사용하며 불투명한 표면의 깊이를 남긴다.
                    Push.Layer = 1U;
                    Draw(Solid, Bed.TopIndexCount, 0);
                }
                if (!bPrepass)
                {
                    RecordWorldReference(Commands, Views, Transform);
                    vkCmdBindVertexBuffers(Commands, 0, 1, &MeshBuffer, &Offset);
                }
                if (bPrepass || !bAppearanceView)
                    continue;
                const auto SectionFirst = Bed.SolidIndexCount + Bed.EffectIndexCount + Bed.BackdropIndexCount;
                if (Bed.VesselBounds)
                {
                    Push.Layer = SectionWindowBackLayer;
                    Draw(*Targets->WaterPipeline, Bed.SectionIndexCount, SectionFirst);
                }
                if ((AppearanceFlags & IncandescentLiquid) == 0U)
                {
                    if (bSmoothBed && OverflowInstances > 0)
                    {
                        Push.Layer = WaterJoinLayer;
                        Bind(bRefractionView ? *Optics->Overflow : *Targets->OverflowPipeline);
                        vkCmdDraw(Commands, 6, OverflowInstances / 2, 0, 0);
                    }
                    Push.Layer = 1U;
                    Draw(bRefractionView ? *Optics->Refractive : *Targets->WaterPipeline, Bed.TopIndexCount, 0);
                }
                if (!bSmoothBed && Views.bShowOverflow && bHasOverflowSnapshot && OverflowInstances > 0)
                {
                    Push.Layer = OverflowLayer;
                    Bind(bRefractionView ? *Optics->Overflow : *Targets->OverflowPipeline);
                    vkCmdDraw(Commands, 24, OverflowInstances, 0, 0);
                }
                if (Bed.SectionIndexCount > 0)
                {
                    if (Bed.VesselBounds)
                    {
                        Push.Layer = VesselContentsLayer;
                        Draw(*Targets->WaterPipeline, Bed.SectionIndexCount, SectionFirst);
                    }
                    Push.Layer = SectionWindowLayer;
                    Draw(*Targets->WaterPipeline, Bed.SectionIndexCount, SectionFirst);
                }
                if (!bEffects)
                    continue;
                if (bHasFlame)
                {
                    Push.Layer = 0U;
                    Draw(*Targets->FlamePipeline, Bed.EffectIndexCount, Bed.SolidIndexCount);
                }
                if (bHasSteam)
                {
                    Push.Layer = 2U;
                    Draw(*Targets->FlamePipeline, Bed.EffectIndexCount, Bed.SolidIndexCount);
                }
            }
        }

    #pragma endregion

    // ============================================================
    // 05. Frame Capture
    // ============================================================
    #pragma region 05_Frame_Capture
        // 표시 이미지의 내용을 호스트에 복사하고 스왑체인 채널 순서에 맞춰 RGB PNG로 저장한다.

        void FSurfaceSimulationDisplay::FImpl::RecordCapture(VkCommandBuffer Commands,
                                                             std::uint32_t   ImageIndex,
                                                             VkBuffer        Capture,
                                                             VkRect2D        Rect) const
        {
            VkImageMemoryBarrier Image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            Image.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            const auto FinalLayout = Swapchain ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            Image.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            Image.oldLayout = FinalLayout;
            Image.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            Image.srcQueueFamilyIndex = Image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            Image.image = Swapchain ? Swapchain->GetImages()[ImageIndex] : Targets->Color->GetHandle();
            Image.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(Commands,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0,
                                 0,
                                 nullptr,
                                 0,
                                 nullptr,
                                 1,
                                 &Image);
            VkBufferImageCopy Region{};
            Region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            Region.imageOffset = {Rect.offset.x, Rect.offset.y, 0};
            Region.imageExtent = {Rect.extent.width, Rect.extent.height, 1};
            vkCmdCopyImageToBuffer(Commands, Image.image, Image.newLayout, Capture, 1, &Region);
            VkMemoryBarrier Host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            Host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            Image.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            Image.dstAccessMask = Swapchain ? 0U : VK_ACCESS_SHADER_READ_BIT;
            Image.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            Image.newLayout = FinalLayout;
            vkCmdPipelineBarrier(Commands,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT | (Swapchain ? 0U : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
                                 0,
                                 1,
                                 &Host,
                                 0,
                                 nullptr,
                                 1,
                                 &Image);
        }

        void FSurfaceSimulationDisplay::FImpl::SaveCapture(const GPU::FGPUBuffer       &Capture,
                                                           VkExtent2D                   Extent,
                                                           VkFormat                     Format,
                                                           const std::filesystem::path &Path) const
        {
            const bool bBgra = Format == VK_FORMAT_B8G8R8A8_SRGB || Format == VK_FORMAT_B8G8R8A8_UNORM;
            if (!bBgra && Format != VK_FORMAT_R8G8B8A8_SRGB && Format != VK_FORMAT_R8G8B8A8_UNORM)
                throw std::runtime_error("Cannot encode the active swapchain format as PNG.");
            std::vector<std::uint8_t> Pixels(static_cast<std::size_t>(Capture.GetSize()));
            Capture.Download(Pixels.data(), Capture.GetSize());
            std::vector<std::uint8_t> CompositedRgb(Pixels.size() / 4 * 3);
            const std::size_t         Red = bBgra ? 2 : 0;
            const std::size_t         Blue = bBgra ? 0 : 2;
            for (std::size_t Pixel = 0; Pixel < Pixels.size() / 4; ++Pixel)
            {
                CompositedRgb[Pixel * 3] = Pixels[Pixel * 4 + Red];
                CompositedRgb[Pixel * 3 + 1] = Pixels[Pixel * 4 + 1];
                CompositedRgb[Pixel * 3 + 2] = Pixels[Pixel * 4 + Blue];
            }
            if (!Path.parent_path().empty())
                std::filesystem::create_directories(Path.parent_path());
            std::ofstream File(Path, std::ios::binary);
            if (!File ||
                stbi_write_png_to_func(WritePng,
                                       &File,
                                       static_cast<int>(Extent.width),
                                       static_cast<int>(Extent.height),
                                       3,
                                       CompositedRgb.data(),
                                       static_cast<int>(Extent.width * 3)) == 0 ||
                !File)
                throw std::runtime_error("Failed to write Surface Simulation screenshot.");
        }

    #pragma endregion

    // ============================================================
    // 06. Present and Swapchain Recreation
    // ============================================================
    #pragma region 06_Present_and_Swapchain_Recreation
        // 제출 완료 후 스왑체인 상태에 따라 대상 리소스를 다시 준비한다.

        void FSurfaceSimulationDisplay::FImpl::Present(std::uint32_t ImageIndex)
        {
            const auto                     Commands = Frame->GetCurrentCommandBuffer();
            const auto                     Available = Frame->GetImageAvailableSemaphore();
            const auto                     Ready = Targets->PresentReady[ImageIndex];
            constexpr VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo                   Submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            Submit.waitSemaphoreCount = 1;
            Submit.pWaitSemaphores = &Available;
            Submit.pWaitDstStageMask = &WaitStage;
            Submit.commandBufferCount = 1;
            Submit.pCommandBuffers = &Commands;
            Submit.signalSemaphoreCount = 1;
            Submit.pSignalSemaphores = &Ready;
            Frame->ResetCurrentFence();
            Check(vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &Submit, Frame->GetInFlightFence()),
                  "Failed to submit water frame.");

            const auto       Handle = Swapchain->GetHandle();
            VkPresentInfoKHR PresentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            PresentInfo.waitSemaphoreCount = 1;
            PresentInfo.pWaitSemaphores = &Ready;
            PresentInfo.swapchainCount = 1;
            PresentInfo.pSwapchains = &Handle;
            PresentInfo.pImageIndices = &ImageIndex;
            const VkResult Result = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
            Frame->AdvanceFrame();
            if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR || Window->WasFramebufferResized())
                Recreate();
            else
                Check(Result, "Failed to present water frame.");
        }

        void FSurfaceSimulationDisplay::FImpl::Recreate()
        {
            Window->WaitForNonZeroFramebuffer();
            if (Window->ShouldClose())
                return;
            Check(vkDeviceWaitIdle(Device), "Failed to drain water frame resources.");
            Optics.reset();
            Targets.reset();
            Swapchain->Recreate(Context, *Window);
            TargetExtent = Swapchain->GetExtent();
            TargetFormat = Swapchain->GetImageFormat();
            Targets = std::make_unique<FTargets>(Context, TargetExtent, TargetFormat, Swapchain.get(), SetLayout);
            Window->ResetFramebufferResized();
        }

    #pragma endregion

    // ============================================================
    // 07. Public Display Interface
    // ============================================================
    #pragma region 07_Public_Display_Interface
        // 장면 실행 루프가 프레임 제출, 캡처 가능 여부와 GPU 통계에 접근한다.

        FSurfaceSimulationDisplay::FSurfaceSimulationDisplay(const GPU::FVulkanContext                      &Context,
                                                             FWindow                                        &Window,
                                                             const Liquid::FSurfaceSimulationDomain         &Geometry,
                                                             const Rendering::FSurfaceSimulationAppearance  &Appearance,
                                                             std::span<const Rendering::FStateVisualization> States,
                                                             bool                                            bSmoothDisplay)
            : Impl(std::make_unique<FImpl>(Context, &Window, Geometry, Appearance, States, bSmoothDisplay))
        {
        }

        FSurfaceSimulationDisplay::FSurfaceSimulationDisplay(const GPU::FVulkanContext                      &Context,
                                                             const Liquid::FSurfaceSimulationDomain         &Geometry,
                                                             const Rendering::FSurfaceSimulationAppearance  &Appearance,
                                                             std::span<const Rendering::FStateVisualization> States,
                                                             bool                                            bSmoothDisplay)
            : Impl(std::make_unique<FImpl>(Context, nullptr, Geometry, Appearance, States, bSmoothDisplay))
        {
        }

        FSurfaceSimulationDisplay::~FSurfaceSimulationDisplay() = default;

        void FSurfaceSimulationDisplay::PrepareBedDisplay(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                                          const Rendering::FSurfaceSimulationAppearance &Appearance,
                                                          const FSurfaceSimulationViews                 &Views)
        {
            Impl->PrepareBedDisplay(Geometry, Appearance, Views);
        }

        VkDescriptorSet FSurfaceSimulationDisplay::PrepareEditorTarget(VkExtent2D                     Extent,
                                                                       const FSurfaceSimulationViews &Views)
        {
            return Impl->PrepareEditorTarget(Extent, Views);
        }

        std::optional<FSurfaceInjectionFootprint>
        FSurfaceSimulationDisplay::PickInjectionFootprint(glm::vec2 Position, const FInjectionBrush &Brush) const
        {
            return Impl->PickInjectionFootprint(Position, Brush);
        }

        std::optional<glm::vec2> FSurfaceSimulationDisplay::TemperatureRangeK() const
        {
            return Impl->TemperatureRangeK();
        }

        std::optional<float> FSurfaceSimulationDisplay::PickTemperatureK(glm::vec2 Position) const
        {
            return Impl->PickTemperatureK(Position);
        }

        std::vector<std::array<glm::vec2, 2>>
        FSurfaceSimulationDisplay::ProjectOutletEdge(const FSurfaceSimulationViews &Views, int View) const
        {
            return Impl->ProjectOutletEdge(Views, View);
        }

        FSurfaceInletOverlay FSurfaceSimulationDisplay::ProjectInletOverlay(const FSurfaceSimulationViews &Views,
                                                                            int                            View) const
        {
            return Impl->ProjectInletOverlay(Views, View);
        }

        void FSurfaceSimulationDisplay::RecordEditorFrame(VkCommandBuffer                            Commands,
                                                          std::uint32_t                              FrameSlot,
                                                          Liquid::FSurfaceSimulationGpu             &Simulation,
                                                          std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                          const FSurfaceSimulationViews             &Views,
                                                          const std::filesystem::path               &Capture,
                                                          int                                        CaptureView)
        {
            Impl->RecordEditorFrame(Commands, FrameSlot, Simulation, Inputs, Views, Capture, CaptureView);
        }

        void FSurfaceSimulationDisplay::FinishEditorCapture()
        {
            Impl->FinishEditorCapture();
        }

        bool FSurfaceSimulationDisplay::DrawFrame(Liquid::FSurfaceSimulationGpu             &Simulation,
                                                  std::span<const Liquid::FSurfaceStepInput> Inputs,
                                                  const FSurfaceSimulationViews             &Views,
                                                  const std::filesystem::path               &Capture)
        {
            return Impl->DrawFrame(Simulation, Inputs, Views, Capture);
        }

        bool FSurfaceSimulationDisplay::CanCapture() const
        {
            return Impl->CanCapture();
        }
        FSurfaceSimulationDisplayStats FSurfaceSimulationDisplay::Stats() const
        {
            return Impl->Stats();
        }

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
