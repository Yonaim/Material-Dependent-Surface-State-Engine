/**
 * @file SurfaceSimulationGpu.cpp
 * @brief 계산 버퍼와 파이프라인을 준비하고 액체 수송 단계 및 상태 복사를 기록한다.
 */

#include "SurfaceState/Simulation/SurfaceSimulationGpu.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "ShaderCatalog.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Compute Layout and Synchronization
    // ============================================================
    #pragma region 00_Compute_Layout_and_Synchronization
        // 계산 셰이더가 읽는 버퍼 오프셋, 단계 ID와 Vulkan 배리어를 한곳에 둔다.

        namespace
        {
            // 표시용 물성과 별개로 계산 셰이더가 읽는 16바이트 물성 블록을 유지한다.
            struct FGpuLiquidMaterial
            {
                float              DensityKgM3;
                float              DynamicViscosityPaS;
                ELocalMaterialRule LocalRule;
                float              IceDensityKgM3;
            };

            // std430 헤더 뒤에 씬에서 사용하는 배열만 32비트 워드 단위로 붙인다.
            struct FSurfaceSimulationBufferHeader
            {
                std::array<std::int32_t, 4> Counts{};
                std::array<float, 4>        Shape{};
                FGpuLiquidMaterial          Liquid{};
                int                         RegionCount = 0;
                int                         CombustionGridColumns = 0;
                FSubstrateQuench            SubstrateQuench{};
                std::uint32_t               EvaporateAbsorbedWater = 0;
                int                         HeightsOffset = 0;
                int                         CellsOffset = 0;
                int                         FacesOffset = 0;
                int                         RegionsOffset = 0;
                int                         CellRegionsOffset = 0;
                int                         SampleElevationsOffset = 0;
                int                         ContactsOffset = 0;
                int                         RegionFacesOffset = 0;
                int                         FaceCountsOffset = 0;
                int                         MaterialsOffset = 0;
                int                         StatesOffset = 0;
                int                         PrimaryLiquidRegionStateOffset = 0;
                int                         CandidatesOffset = 0;
                int                         OutScaleOffset = 0;
                int                         ScheduledInletWeightsOffset = 0;
                int                         EditorLiquidKgOffset = 0;
                int                         EditorIceKgOffset = 0;
                int                         EditorTemperatureKOffset = 0;
                int                         FaceIntegralsOffset = 0;
                int                         ContactWeightsOffset = 0;
                int                         OverflowOffset = 0;
                int                         DispatchOffset = 0;
                int                         FaceWorkgroupsOffset = 0;
                int                         RegionWorkgroupsOffset = 0;
                int                         CellWorkgroupsOffset = 0;
                std::uint32_t               FaceOperations = 3;
                int                         LayerCount = 0;
                int                         FilmMaterialOffset = 0;
                int                         FilmStateOffset = 0;
                int                         FilmInitialMassOffset = 0;

                int   FilmDisplayOffset = 0;
                int   FilmReactionOffset = 0;
                int   FilmReactionCount = 0;
                int   FilmBedCoupling = 1;
                float FilmBedFreezeStart = 0.F;
                int   FilmOmittedSupport = -1;

                int ReactionContactOffset = 0;

                int DisplayLayers = 0;
            };
            static_assert(offsetof(FSurfaceSimulationBufferHeader, Liquid) == 32 &&
                          offsetof(FSurfaceSimulationBufferHeader, SubstrateQuench) == 56);
            static_assert(offsetof(FSurfaceSimulationBufferHeader, HeightsOffset) == 68 &&
                          offsetof(FSurfaceSimulationBufferHeader, FaceOperations) == 168 &&
                          offsetof(FSurfaceSimulationBufferHeader, DisplayLayers) == 216 &&
                          sizeof(FSurfaceSimulationBufferHeader) == 220 && sizeof(FFilmLayer) == 40 &&
                          sizeof(FFilmReaction) == 40);
            static_assert(sizeof(FSurfaceCellState) == 48 && sizeof(FCellUpdateParameters) == 32);
            static_assert(sizeof(FSimulationCell) == 16 && sizeof(FTransportFace) == 16 && sizeof(FStorageRegion) == 16);
            static_assert(sizeof(FPrimaryLiquidRegionState) == 8 && sizeof(FFilmRegionState) == 12 &&
                          sizeof(FIndexRange) == 8);

            struct FGpuArrayData
            {
                std::vector<float>                     Heights;
                std::vector<FSimulationCell>           Cells;
                std::vector<FTransportFace>            Faces;
                std::vector<FStorageRegion>            Regions;
                std::vector<FIndexRange>               CellRegions;
                std::vector<std::array<float, 2>>      SampleElevations;
                std::vector<int>                       Contacts;
                std::vector<int>                       RegionFaces;
                std::vector<int>                       FaceCounts;
                std::vector<FCellUpdateParameters>     Materials;
                std::vector<FSurfaceCellState>         States;
                std::vector<FPrimaryLiquidRegionState> Water;
                std::vector<float>                     ScheduledInletWeights;
            };

            template <class T>
            int AppendGpuArray(std::vector<std::byte> &Storage, std::size_t Count, const std::vector<T> &Values = {})
            {
                static_assert(sizeof(T) % sizeof(std::uint32_t) == 0 && std::is_trivially_copyable_v<T>);
                const auto Start = Storage.size();
                Storage.resize(Start + Count * sizeof(T));
                if (!Values.empty())
                    std::memcpy(Storage.data() + Start, Values.data(), Values.size() * sizeof(T));
                return static_cast<int>((Start - sizeof(FSurfaceSimulationBufferHeader)) / sizeof(std::uint32_t));
            }

            std::size_t GpuByteOffset(int WordOffset)
            {
                return sizeof(FSurfaceSimulationBufferHeader) +
                       static_cast<std::size_t>(WordOffset) * sizeof(std::uint32_t);
            }

            static_assert(sizeof(FGpuLiquidMaterial) == 16 && offsetof(FGpuLiquidMaterial, LocalRule) == 8 &&
                          offsetof(FGpuLiquidMaterial, IceDensityKgM3) == 12);

            struct FSurfaceSimulationStepPush
            {
                FSurfaceTilt  Pose;
                float         Seconds;
                float         InputMass;
                std::uint32_t ReadSlot;
                std::uint32_t Phase;
                FThermalInput Thermal;
                FLiquidDose   Dose;
                std::uint32_t LocalInput;
                std::uint32_t DisplayInterval;
                std::uint32_t CompletedStep;
                std::uint32_t Sparse;
                float         TransportSeconds;
                float         TransferTime;
                std::uint32_t TransportOnly;
            };
            static_assert(sizeof(FSurfaceSimulationStepPush) == 68 && offsetof(FSurfaceSimulationStepPush, Thermal) == 24 &&
                          offsetof(FSurfaceSimulationStepPush, LocalInput) == 40 &&
                          offsetof(FSurfaceSimulationStepPush, Dose) == 32 && offsetof(FLiquidDose, Region) == 4 &&
                          offsetof(FThermalInput, TargetWeight) == 4);

            constexpr std::uint32_t Prepare = 0;
            constexpr std::uint32_t FaceCandidates = 1;
            constexpr std::uint32_t OutflowScale = 2;
            constexpr std::uint32_t Commit = 3;
            constexpr std::uint32_t Schedule = 4;

            constexpr std::uint32_t                            ThreadsPerWorkgroup = 32;
            constexpr std::array<VkDispatchIndirectCommand, 3> EmptySchedule{{{0, 1, 1}, {0, 1, 1}, {0, 1, 1}}};

            constexpr std::uint32_t WorkgroupCount(std::uint32_t Count)
            {
                return (Count + ThreadsPerWorkgroup - 1) / ThreadsPerWorkgroup;
            }

            void Check(VkResult Result, const char *Message)
            {
                if (Result != VK_SUCCESS)
                    throw std::runtime_error(Message);
            }

            void Barrier(VkCommandBuffer      Commands,
                         VkPipelineStageFlags SourceStage,
                         VkPipelineStageFlags DestinationStage,
                         VkAccessFlags        SourceAccess,
                         VkAccessFlags        DestinationAccess)
            {
                VkMemoryBarrier Memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                Memory.srcAccessMask = SourceAccess;
                Memory.dstAccessMask = DestinationAccess;
                vkCmdPipelineBarrier(Commands, SourceStage, DestinationStage, 0, 1, &Memory, 0, nullptr, 0, nullptr);
            }

            VkShaderModule MakeComputeShader(VkDevice Device)
            {
                using Shaders::EShader;
                std::ifstream Shader(Shaders::Path(EShader::Simulation_Surface_SurfaceSimulationStep_comp),
                                     std::ios::binary | std::ios::ate);
                const auto    Size = Shader.tellg();
                if (Size <= 0 || static_cast<std::streamoff>(Size) % 4 != 0)
                    throw std::runtime_error("Could not read the liquid compute shader.");
                std::vector<std::uint32_t> Code(static_cast<std::size_t>(Size) / sizeof(std::uint32_t));
                Shader.seekg(0);
                if (!Shader.read(reinterpret_cast<char *>(Code.data()), static_cast<std::streamsize>(Size)))
                    throw std::runtime_error("Incomplete liquid compute shader.");

                VkShaderModuleCreateInfo Info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                Info.codeSize = Code.size() * sizeof(std::uint32_t);
                Info.pCode = Code.data();
                VkShaderModule Module = VK_NULL_HANDLE;
                Check(vkCreateShaderModule(Device, &Info, nullptr, &Module), "Failed to create liquid shader module.");
                return Module;
            }

        } // namespace

    #pragma endregion

    struct FSurfaceSimulationGpu::FStorage
    {
        FStorage(const GPU::FVulkanContext     &Context,
                 FSurfaceSimulationBufferHeader InHeader,
                 const std::vector<std::byte>  &Bytes)
            : Header(InHeader),
              Buffer(Context.GetPhysicalDevice(),
                     Context.GetDevice(),
                     Bytes.size(),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     GPU::EGPUBufferMemoryCategory::SurfaceRuntime)
        {
            Buffer.Upload(Bytes.data(), Bytes.size());
        }

        FSurfaceSimulationBufferHeader Header;
        GPU::FGPUBuffer                Buffer;
    };

    // ============================================================
    // 01. Compute Resources
    // ============================================================
    #pragma region 01_Compute_Resources
        // 초기 상태와 셰이더의 정적 입력을 올리고 Vulkan 계산 파이프라인을 만든다.

        FSurfaceSimulationGpu::FSurfaceSimulationGpu(const GPU::FVulkanContext      &Context,
                                                     const FSurfaceSimulationDomain &Geometry,
                                                     bool                            bEnableSparseUpdates)
            : Device(Context.GetDevice()),
              CellCount(static_cast<std::uint32_t>(Geometry.Cells.size())),
              RegionCount(static_cast<std::uint32_t>(Geometry.Regions.size())),
              FaceCount(static_cast<std::uint32_t>(Geometry.Faces.size())),
              ContactCount(Geometry.Contacts.size()),
              bEnableSparseUpdates(bEnableSparseUpdates),
              bExternalExchange(Geometry.bExternalExchange),
              InitialTemperatureWeight(Geometry.InitialTemperatureWeight)
        {
            try
            {
                const auto                    &Shape = Geometry.Shape;
                FSurfaceSimulationBufferHeader Header;
                Header.Counts = {Shape.Columns,
                                 static_cast<int>(CellCount),
                                 static_cast<int>(FaceCount),
                                 Geometry.ScheduledInletWeights.empty() ? Geometry.SourceRegion() : -1};
                Header.Shape = {Shape.PitchX(), Shape.PitchY(), Shape.Width, Shape.Length};
                Header.RegionCount = static_cast<int>(RegionCount);
                FGpuArrayData Initial;
                Initial.ScheduledInletWeights = Geometry.ScheduledInletWeights;
                Initial.Heights = Shape.Heights;
                Initial.Cells = Geometry.Cells;
                Initial.Faces = Geometry.Faces;
                Initial.Regions = Geometry.Regions;
                Initial.CellRegions = Geometry.CellRegions;
                Initial.SampleElevations.resize(Shape.Heights.size());
                int        NextSample = 0;
                const auto PackSample = [&](int Index)
                {
                    const int X = Index % Shape.Columns;
                    Initial.SampleElevations[NextSample++] = {Shape.Heights[Index], Shape.EdgeX(X) + 0.5F * Shape.PitchX()};
                };
                for (std::uint32_t Index = 0; Index < RegionCount; ++Index)
                {
                    const auto &Region = Geometry.Regions[Index];
                    Initial.Regions[Index].FirstSample = NextSample;
                    if (Region.FirstSample < 0)
                    {
                        const auto Cell = Geometry.Cells[Region.Cell];
                        for (int Y = Cell.FirstRow; Y < Cell.EndRow; ++Y)
                            for (int X = Cell.FirstColumn; X < Cell.EndColumn; ++X)
                                PackSample(Y * Shape.Columns + X);
                    }
                    else
                    {
                        for (const int Sample :
                             FIndexRange{Region.FirstSample, Region.SampleCount}.In(Geometry.RegionSamples))
                            PackSample(Sample);
                    }
                }
                Initial.Contacts = Geometry.Contacts;
                Initial.RegionFaces = Geometry.RegionFaces;
                Initial.FaceCounts = Geometry.FaceCounts;
                Header.Liquid = {Geometry.Liquid.DensityKgM3,
                                 Geometry.Liquid.DynamicViscosityPaS,
                                 Geometry.Liquid.LocalRule,
                                 Geometry.Liquid.IceDensityKgM3};

                Header.CombustionGridColumns = Geometry.CombustionGridColumns;
                Header.FaceOperations = (Geometry.bTransport ? 1U : 0U) | (Geometry.bExternalExchange ? 2U : 0U);
                Header.SubstrateQuench = Geometry.SubstrateQuench;
                Header.EvaporateAbsorbedWater = Geometry.bEvaporateAbsorbedWater ? 1U : 0U;
                Initial.Materials = Geometry.Materials;
                if (UsesWaterMaterial(Geometry))
                {
                    // Fixed-step probabilities avoid two exponentials per cell in the commit dispatch.
                    for (std::uint32_t Cell = 0; Cell < CellCount; ++Cell)
                    {
                        auto &Rates = Initial.Materials[Cell];
                        Rates.AbsorbPerSecond = -std::expm1(-Rates.AbsorbPerSecond * StepSeconds);
                        Rates.DryPerSecond = -std::expm1(-Rates.DryPerSecond * StepSeconds);
                    }
                }
                const auto State = InitialState(Geometry);
                Initial.States = State.Cells;
                Initial.States.insert(Initial.States.end(), State.Cells.begin(), State.Cells.end());
                Initial.Water = State.Regions;
                Initial.Water.insert(Initial.Water.end(), State.Regions.begin(), State.Regions.end());

                std::vector<std::byte> Bytes(sizeof(Header));
                Header.HeightsOffset = AppendGpuArray<float>(Bytes, Shape.Heights.size(), Initial.Heights);
                Header.CellsOffset = AppendGpuArray<FSimulationCell>(Bytes, CellCount, Initial.Cells);
                Header.FacesOffset = AppendGpuArray<FTransportFace>(Bytes, FaceCount, Initial.Faces);
                Header.RegionsOffset = AppendGpuArray<FStorageRegion>(Bytes, RegionCount, Initial.Regions);
                Header.CellRegionsOffset = AppendGpuArray<FIndexRange>(Bytes, CellCount, Initial.CellRegions);
                Header.SampleElevationsOffset =
                    AppendGpuArray<std::array<float, 2>>(Bytes, Shape.Heights.size(), Initial.SampleElevations);
                Header.ContactsOffset = AppendGpuArray<int>(Bytes, ContactCount, Initial.Contacts);
                Header.RegionFacesOffset = AppendGpuArray<int>(Bytes, Geometry.RegionFaces.size(), Initial.RegionFaces);
                Header.FaceCountsOffset = AppendGpuArray<int>(Bytes, RegionCount, Initial.FaceCounts);
                Header.MaterialsOffset = AppendGpuArray<FCellUpdateParameters>(Bytes, CellCount, Initial.Materials);
                Header.StatesOffset = AppendGpuArray<FSurfaceCellState>(Bytes, 2 * CellCount, Initial.States);
                Header.PrimaryLiquidRegionStateOffset =
                    AppendGpuArray<FPrimaryLiquidRegionState>(Bytes, 2 * RegionCount, Initial.Water);
                auto Layers = Geometry.FilmLayers;
                if (Layers.empty())
                    Layers.push_back(PrimaryFilmMaterial(Geometry.Liquid));
                Header.LayerCount = static_cast<int>(Layers.size());
                Header.DisplayLayers = Geometry.FilmLayers.size() > 1 ? Header.LayerCount : 0;
                this->bEnableSparseUpdates =
                    this->bEnableSparseUpdates && Layers.size() == 1 && Layers.front().WetWaterFraction == 0.F;
                Header.CandidatesOffset = AppendGpuArray<float>(Bytes, Layers.size() * FaceCount);
                Header.OutScaleOffset = AppendGpuArray<float>(Bytes, Layers.size() * RegionCount);
                Header.ScheduledInletWeightsOffset =
                    AppendGpuArray<float>(Bytes, RegionCount, Initial.ScheduledInletWeights);
                Header.EditorLiquidKgOffset = AppendGpuArray<float>(Bytes, RegionCount);
                Header.EditorIceKgOffset = AppendGpuArray<float>(Bytes, CellCount);
                Header.EditorTemperatureKOffset = AppendGpuArray<float>(Bytes, CellCount);
                Header.FaceIntegralsOffset = AppendGpuArray<float>(Bytes, FaceCount);
                Header.ContactWeightsOffset = AppendGpuArray<std::array<float, 2>>(Bytes, ContactCount);
                Header.OverflowOffset = AppendGpuArray<FOverflowTransferRecord>(Bytes, 2 * ContactCount);
                Header.DispatchOffset = AppendGpuArray<VkDispatchIndirectCommand>(Bytes, EmptySchedule.size());
                Header.FaceWorkgroupsOffset = AppendGpuArray<std::uint32_t>(Bytes, 2U * WorkgroupCount(FaceCount));
                Header.RegionWorkgroupsOffset = AppendGpuArray<std::uint32_t>(Bytes, 2U * WorkgroupCount(RegionCount));
                Header.CellWorkgroupsOffset = AppendGpuArray<std::uint32_t>(Bytes, 2U * WorkgroupCount(CellCount));
                Header.FilmBedCoupling = Geometry.bFilmBedCoupling ? 1 : 0;
                Header.FilmBedFreezeStart = Geometry.FilmBedFreezeStart;
                Header.FilmOmittedSupport = Geometry.FilmOmittedSupport;
                TransportSubsteps = std::max(1, Geometry.TransportSubsteps);
                auto Film = State.Film;
                Film.insert(Film.end(), State.Film.begin(), State.Film.end());
                Header.FilmMaterialOffset = AppendGpuArray<FFilmLayer>(Bytes, Layers.size(), Layers);
                Header.FilmStateOffset = AppendGpuArray<FFilmRegionState>(Bytes, Film.size(), Film);
                Header.FilmInitialMassOffset =
                    AppendGpuArray<float>(Bytes, Geometry.InitialFilmKg.size(), Geometry.InitialFilmKg);
                if (Header.DisplayLayers > 0)
                    Header.FilmDisplayOffset =
                        AppendGpuArray<std::array<float, 6>>(Bytes, RegionCount, MakeFilmDisplay(Geometry, State));
                Header.FilmReactionCount = static_cast<int>(Geometry.FilmReactions.size());
                Header.FilmReactionOffset =
                    AppendGpuArray<FFilmReaction>(Bytes, Geometry.FilmReactions.size(), Geometry.FilmReactions);
                Header.ReactionContactOffset = AppendGpuArray<std::array<float, 4>>(Bytes, RegionCount);
                std::memcpy(Bytes.data(), &Header, sizeof(Header));
                Storage = std::make_unique<FStorage>(Context, Header, Bytes);

                const VkDescriptorSetLayoutBinding Binding{0,
                                                           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                           1,
                                                           VK_SHADER_STAGE_COMPUTE_BIT,
                                                           nullptr};
                VkDescriptorSetLayoutCreateInfo    SetInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
                SetInfo.bindingCount = 1;
                SetInfo.pBindings = &Binding;
                Check(vkCreateDescriptorSetLayout(Device, &SetInfo, nullptr, &SetLayout),
                      "Failed to create liquid set layout.");

                const VkDescriptorPoolSize PoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
                VkDescriptorPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
                PoolInfo.maxSets = 1;
                PoolInfo.poolSizeCount = 1;
                PoolInfo.pPoolSizes = &PoolSize;
                Check(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &Pool),
                      "Failed to create liquid descriptor pool.");

                VkDescriptorSetAllocateInfo Allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
                Allocate.descriptorPool = Pool;
                Allocate.descriptorSetCount = 1;
                Allocate.pSetLayouts = &SetLayout;
                Check(vkAllocateDescriptorSets(Device, &Allocate, &Set), "Failed to allocate liquid descriptor.");
                const VkDescriptorBufferInfo BufferInfo{Storage->Buffer.GetHandle(), 0, Storage->Buffer.GetSize()};
                VkWriteDescriptorSet         Write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                Write.dstSet = Set;
                Write.descriptorCount = 1;
                Write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                Write.pBufferInfo = &BufferInfo;
                vkUpdateDescriptorSets(Device, 1, &Write, 0, nullptr);

                const VkPushConstantRange  Range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FSurfaceSimulationStepPush)};
                VkPipelineLayoutCreateInfo LayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
                LayoutInfo.setLayoutCount = 1;
                LayoutInfo.pSetLayouts = &SetLayout;
                LayoutInfo.pushConstantRangeCount = 1;
                LayoutInfo.pPushConstantRanges = &Range;
                Check(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &Layout),
                      "Failed to create liquid compute layout.");
                const VkShaderModule        Module = MakeComputeShader(Device);
                VkComputePipelineCreateInfo PipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
                PipelineInfo.layout = Layout;
                PipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                PipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                PipelineInfo.stage.module = Module;
                PipelineInfo.stage.pName = "main";
                const VkResult Created =
                    vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Pipeline);
                vkDestroyShaderModule(Device, Module, nullptr);
                Check(Created, "Failed to create liquid compute pipeline.");
            }
            catch (...)
            {
                Release();
                throw;
            }
        }

        FSurfaceSimulationGpu::~FSurfaceSimulationGpu()
        {
            Release();
        }

        void FSurfaceSimulationGpu::Release() noexcept
        {
            vkDestroyPipeline(Device, Pipeline, nullptr);
            vkDestroyPipelineLayout(Device, Layout, nullptr);
            vkDestroyDescriptorPool(Device, Pool, nullptr);
            vkDestroyDescriptorSetLayout(Device, SetLayout, nullptr);
        }

    #pragma endregion

    // ============================================================
    // 02. Runoff Dispatch
    // ============================================================
    #pragma region 02_Runoff_Dispatch
        // 후보량, 출발 배율, 상태 반영 순으로 디스패치하고 각 단계의 쓰기를 다음 단계에 보이게 한다.

        void FSurfaceSimulationGpu::RecordStep(VkCommandBuffer        Commands,
                                               FSurfaceTilt           Pose,
                                               float                  InputMass,
                                               FThermalInput          Thermal,
                                               FLiquidDose            Dose,
                                               const FLocalStepInput *LocalInput,
                                               bool                   bRecordOverflow)
        {
            if (!bExternalExchange)
            {
                InputMass = 0.F;
                Thermal = {0.F, InitialTemperatureWeight};
                Dose = {};
                LocalInput = nullptr;
            }
            constexpr auto Compute = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            constexpr auto ComputeAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            Barrier(Commands,
                    Compute | VK_PIPELINE_STAGE_TRANSFER_BIT,
                    Compute,
                    ComputeAccess | VK_ACCESS_TRANSFER_READ_BIT,
                    ComputeAccess);
            vkCmdBindPipeline(Commands, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline);
            vkCmdBindDescriptorSets(Commands, VK_PIPELINE_BIND_POINT_COMPUTE, Layout, 0, 1, &Set, 0, nullptr);

            constexpr auto Transfer = VK_PIPELINE_STAGE_TRANSFER_BIT;
            if (LocalInput)
            {
                // 각 스텝의 GPU 읽기를 마친 뒤 공유 입력 구간을 다시 쓴다.
                Barrier(Commands,
                        Compute | Transfer,
                        Transfer,
                        ComputeAccess | VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT);
                // vkCmdUpdateBuffer의 한 번 전송 한도는 64 KiB다.
                const auto Update = [&](int Offset, const std::vector<float> &Input)
                {
                    constexpr std::size_t FloatsPerUpdate = 65536 / sizeof(float);
                    for (std::size_t Start = 0; Start < Input.size(); Start += FloatsPerUpdate)
                    {
                        const auto Count = std::min(FloatsPerUpdate, Input.size() - Start);
                        vkCmdUpdateBuffer(Commands,
                                          Storage->Buffer.GetHandle(),
                                          GpuByteOffset(Offset) + Start * sizeof(float),
                                          Count * sizeof(float),
                                          Input.data() + Start);
                    }
                };
                const auto &Header = Storage->Header;
                Update(Header.EditorLiquidKgOffset, LocalInput->LiquidKg);
                Update(Header.EditorIceKgOffset, LocalInput->IceKg);
                Update(Header.EditorTemperatureKOffset, LocalInput->TemperatureK);
                Barrier(Commands, Transfer, Compute, VK_ACCESS_TRANSFER_WRITE_BIT, ComputeAccess);
            }
            const bool bPoseChanged = Pose.Sine != PreparedPose.Sine || Pose.Cosine != PreparedPose.Cosine;
            const bool bLocalLiquid = LocalInput && std::ranges::any_of(std::span(LocalInput->LiquidKg).first(RegionCount),
                                                                        [](float Mass) { return Mass > 0.F; });
            if (InputMass > 0.F || Dose.MassKg > 0.F || bLocalLiquid || bPoseChanged)
            {
                RecordPhase(Commands, Prepare, Pose, InputMass, {}, Dose, bLocalLiquid);
                Barrier(Commands, Compute, Compute, ComputeAccess, ComputeAccess);
                PreparedPose = Pose;
            }
            // 전역 상태가 바뀌거나 이웃 온도를 읽는 연소의 경우 두 슬롯을 사용하는 전체 갱신이 필요하다.
            const bool bSparse = TransportSubsteps == 1 && bEnableSparseUpdates && InputMass == 0.F && Dose.MassKg == 0.F &&
                                 CompletedSteps != 0 && !bPoseChanged && !LocalInput && !bPreviousLocalInput &&
                                 Storage->Header.CombustionGridColumns == 0 && Thermal.OffsetK == PreviousThermal.OffsetK &&
                                 Thermal.TargetWeight == PreviousThermal.TargetWeight;
            if (bSparse)
            {
                constexpr auto Indirect = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
                Barrier(Commands,
                        Compute | Transfer | Indirect,
                        Transfer,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT);
                vkCmdUpdateBuffer(Commands,
                                  Storage->Buffer.GetHandle(),
                                  GpuByteOffset(Storage->Header.DispatchOffset),
                                  sizeof(EmptySchedule),
                                  EmptySchedule.data());
                Barrier(Commands, Transfer, Compute, VK_ACCESS_TRANSFER_WRITE_BIT, ComputeAccess);
                RecordPhase(Commands, Schedule, Pose, InputMass);
                Barrier(Commands,
                        Compute,
                        Compute | Indirect,
                        VK_ACCESS_SHADER_WRITE_BIT,
                        ComputeAccess | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
            }

            for (int Index = 0; Index < TransportSubsteps; ++Index)
            {
                const bool bTransportOnly = Index + 1 < TransportSubsteps;
                RecordPhase(Commands, FaceCandidates, Pose, 0.F, {}, {}, false, bRecordOverflow, bSparse, Index);
                Barrier(Commands, Compute, Compute, ComputeAccess, ComputeAccess);
                RecordPhase(Commands, OutflowScale, Pose, 0.F, {}, {}, false, false, bSparse, Index);
                Barrier(Commands, Compute, Compute, ComputeAccess, ComputeAccess);
                RecordPhase(Commands,
                            Commit,
                            Pose,
                            0.F,
                            Thermal,
                            {},
                            LocalInput != nullptr,
                            bRecordOverflow,
                            bSparse,
                            Index,
                            bTransportOnly);
                if (!bSparse)
                    ReadSlot ^= 1U;
                if (bTransportOnly)
                    Barrier(Commands, Compute, Compute, ComputeAccess, ComputeAccess);
            }
            PreviousThermal = Thermal;
            bPreviousLocalInput = LocalInput != nullptr;
            ++CompletedSteps;
        }

        void FSurfaceSimulationGpu::RecordPhase(VkCommandBuffer Commands,
                                                std::uint32_t   Phase,
                                                FSurfaceTilt    Pose,
                                                float           InputMass,
                                                FThermalInput   Thermal,
                                                FLiquidDose     Dose,
                                                bool            bLocalInput,
                                                bool            bRecordOverflow,
                                                bool            bSparse,
                                                int             TransportIndex,
                                                bool            bTransportOnly) const
        {
            const float TransportSeconds = StepSeconds / static_cast<float>(TransportSubsteps);
            const float TransferTime = CompletedSteps * StepSeconds + (TransportIndex + 1) * TransportSeconds;
            const FSurfaceSimulationStepPush Push{Pose,
                                                  StepSeconds,
                                                  InputMass,
                                                  ReadSlot,
                                                  Phase,
                                                  Thermal,
                                                  Dose,
                                                  bLocalInput ? 1U : 0U,
                                                  bRecordOverflow ? DisplayInterval : 0U,
                                                  CompletedSteps + 1U,
                                                  bSparse ? 1U : 0U,
                                                  TransportSeconds,
                                                  TransferTime,
                                                  bTransportOnly ? 1U : 0U};
            vkCmdPushConstants(Commands, Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &Push);
            if (bSparse)
            {
                const auto Offset = GpuByteOffset(Storage->Header.DispatchOffset) +
                                    (Phase - FaceCandidates) * sizeof(VkDispatchIndirectCommand);
                vkCmdDispatchIndirect(Commands, Storage->Buffer.GetHandle(), Offset);
                return;
            }

            const auto Layers = static_cast<std::uint32_t>(Storage->Header.LayerCount);
            const auto Count = Phase == Schedule         ? std::max({FaceCount, RegionCount, CellCount})
                               : Phase == FaceCandidates ? Layers * FaceCount
                               : Phase == Commit         ? CellCount
                               : Phase == Prepare        ? RegionCount
                                                         : Layers * RegionCount;
            if (Count > 0)
                vkCmdDispatch(Commands, WorkgroupCount(Count), 1, 1);
        }

    #pragma endregion

    // ============================================================
    // 03. Display Copy and Readback
    // ============================================================
    #pragma region 03_Display_Copy_and_Readback
        // 최신 읽기 슬롯에서 그래픽 표시용 스냅샷과 CPU 검사 값을 얻는다.

        void FSurfaceSimulationGpu::BeginDisplayInterval() noexcept
        {
            ++DisplayInterval;
        }

        void FSurfaceSimulationGpu::RecordDisplayCopy(VkCommandBuffer Commands, VkBuffer Display) const
        {
            Barrier(Commands,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT);
            const auto                        Layers = static_cast<std::size_t>(Storage->Header.DisplayLayers);
            const FSurfaceSnapshotLayout      DisplayLayout(CellCount, RegionCount, ContactCount, Layers);
            const std::array<VkBufferCopy, 3> Copies{
                {{GpuByteOffset(Storage->Header.StatesOffset) + ReadSlot * CellCount * sizeof(FSurfaceCellState),
                        0,
                        CellCount * sizeof(FSurfaceCellState)},
                    {GpuByteOffset(Storage->Header.PrimaryLiquidRegionStateOffset) +
                        ReadSlot * RegionCount * sizeof(FPrimaryLiquidRegionState),
                        DisplayLayout.RegionOffset,
                        RegionCount * sizeof(FPrimaryLiquidRegionState)},
                    {GpuByteOffset(Storage->Header.OverflowOffset),
                        DisplayLayout.OverflowOffset,
                        2 * ContactCount * sizeof(FOverflowTransferRecord)}}};
            vkCmdCopyBuffer(Commands, Storage->Buffer.GetHandle(), Display, ContactCount > 0 ? 3U : 2U, Copies.data());
            if (Layers > 0)
            {
                const VkBufferCopy FilmCopy{GpuByteOffset(Storage->Header.FilmDisplayOffset),
                                            DisplayLayout.FilmOffset,
                                            RegionCount * sizeof(std::array<float, 6>)};
                const auto         StateBytes = Layers * RegionCount * sizeof(FFilmRegionState);
                const VkBufferCopy FilmStateCopy{GpuByteOffset(Storage->Header.FilmStateOffset) + ReadSlot * StateBytes,
                                                 DisplayLayout.FilmStateOffset,
                                                 StateBytes};
                const std::array   FilmCopies{FilmCopy, FilmStateCopy};
                vkCmdCopyBuffer(Commands,
                                Storage->Buffer.GetHandle(),
                                Display,
                                static_cast<std::uint32_t>(FilmCopies.size()),
                                FilmCopies.data());
            }
            const VkBufferCopy ContactCopy{GpuByteOffset(Storage->Header.ReactionContactOffset),
                                           DisplayLayout.ContactOffset,
                                           RegionCount * sizeof(std::array<float, 4>)};
            vkCmdCopyBuffer(Commands, Storage->Buffer.GetHandle(), Display, 1, &ContactCopy);
            Barrier(Commands,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT);
        }

        std::vector<FOverflowTransferRecord> FSurfaceSimulationGpu::ReadOverflow() const
        {
            std::vector<FOverflowTransferRecord> Result(2 * ContactCount);
            if (!Result.empty())
                Storage->Buffer.Download(Result.data(),
                                         Result.size() * sizeof(FOverflowTransferRecord),
                                         GpuByteOffset(Storage->Header.OverflowOffset));
            return Result;
        }

        FSurfaceSimulationReadback FSurfaceSimulationGpu::Readback() const
        {
            FSurfaceSimulationReadback Result{
                {std::vector<FSurfaceCellState>(CellCount), std::vector<FPrimaryLiquidRegionState>(RegionCount)},
                std::vector<float>(FaceCount),
                std::vector<float>(RegionCount)};
            Storage->Buffer.Download(Result.State.Cells.data(),
                                     CellCount * sizeof(FSurfaceCellState),
                                     GpuByteOffset(Storage->Header.StatesOffset) +
                                         ReadSlot * CellCount * sizeof(FSurfaceCellState));
            Storage->Buffer.Download(Result.State.Regions.data(),
                                     RegionCount * sizeof(FPrimaryLiquidRegionState),
                                     GpuByteOffset(Storage->Header.PrimaryLiquidRegionStateOffset) +
                                         ReadSlot * RegionCount * sizeof(FPrimaryLiquidRegionState));
            if (Storage->Header.LayerCount != 0)
            {
                const auto Count = RegionCount * Storage->Header.LayerCount;
                Result.State.Film.resize(Count);
                Result.State.FilmTime = CompletedSteps * StepSeconds;
                Storage->Buffer.Download(Result.State.Film.data(),
                                         Count * sizeof(FFilmRegionState),
                                         GpuByteOffset(Storage->Header.FilmStateOffset) +
                                             ReadSlot * Count * sizeof(FFilmRegionState));
            }
            if (FaceCount > 0)
                Storage->Buffer.Download(Result.FaceCandidateMassKg.data(),
                                         FaceCount * sizeof(float),
                                         GpuByteOffset(Storage->Header.CandidatesOffset) +
                                             (Storage->Header.LayerCount - 1) * FaceCount * sizeof(float));
            Storage->Buffer.Download(Result.RegionOutflowScale.data(),
                                     RegionCount * sizeof(float),
                                     GpuByteOffset(Storage->Header.OutScaleOffset) +
                                         (Storage->Header.LayerCount - 1) * RegionCount * sizeof(float));
            return Result;
        }

    #pragma endregion

} // namespace MDSS::Liquid
