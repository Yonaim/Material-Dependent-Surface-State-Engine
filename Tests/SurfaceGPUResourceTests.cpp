/**
 * @file SurfaceGPUResourceTests.cpp
 * @brief Vulkan resource layout, 초기화와 descriptor 연결을 검증한다.
 */

#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"
#include "SurfaceState/Types/SurfaceSolverRates.h"
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// ============================================================
// 00. Fixtures and Test Support
// ============================================================
#pragma region "00_Fixtures_and_Test_Support"
// 회귀 실행에 사용할 입력 자산과 공통 검사·GPU 준비 코드를 제공한다.

namespace
{
    int FailureCount = 0;

    class FVulkanUnavailable final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    void Check(bool bCondition, const std::string& Message)
    {
        if (!bCondition)
        {
            ++FailureCount;
            std::cerr << "FAIL: " << Message << '\n';
        }
    }

#pragma endregion

// ============================================================
// 01. TestMesoPeakPacking
// ============================================================
#pragma region "01_TestMesoPeakPacking"
// 기대 조건: Meso peak must be shared within a chart and isolated across charts and surfaces

    void TestMesoPeakPacking()
    {
        using namespace MDSS::SurfaceState;
        FSharedSurfaceGeometryData         Geometry({{0, {3, 1}}, {1, {1, 1}}});
        const std::array<float, 4>         Heights{-0.2F, 0.3F, 0.1F, 0.5F};
        const std::array<std::uint32_t, 4> Charts{0U, 0U, 1U, 0U};

        for (std::size_t Index = 0; Index < Heights.size(); ++Index)
        {
            auto& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = Index < 3 ? 0U : 1U;
            Texel.Triangle = 0U;
            Texel.Chart = Charts[Index];
            Texel.Geometry.MesoVirtualHeight = Heights[Index];
        }

        Geometry.SetProfileMap({0, 0, 0, 0});
        const auto                 Packed = PackSharedSurfaceGeometry(Geometry);
        const std::array<float, 4> Expected{0.3F, 0.3F, 0.1F, 0.5F};

        for (std::size_t Index = 0; Index < Expected.size(); ++Index)
            Check(std::abs(Packed.MesoNormals[Index].W - Expected[Index]) < 1.0e-6F,
                  "Meso peak must be shared within a chart and isolated across charts and surfaces");
    }

    class FVulkanTestDevice final
    {
    public:
        FVulkanTestDevice()
        {
            VkApplicationInfo ApplicationInfo{};
            ApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            ApplicationInfo.pApplicationName = "MDSS Surface GPU resource tests";
            ApplicationInfo.apiVersion = VK_API_VERSION_1_2;

            VkInstanceCreateInfo InstanceInfo{};
            InstanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            InstanceInfo.pApplicationInfo = &ApplicationInfo;
#if defined(__APPLE__) && defined(VK_KHR_portability_enumeration)
            const char* PortabilityExtension = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            InstanceInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            InstanceInfo.enabledExtensionCount = 1;
            InstanceInfo.ppEnabledExtensionNames = &PortabilityExtension;
#endif
            if (vkCreateInstance(&InstanceInfo, nullptr, &Instance) != VK_SUCCESS)
            {
                throw FVulkanUnavailable("Could not create Vulkan instance for GPU resource tests.");
            }

            std::uint32_t PhysicalDeviceCount = 0;

            if (vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, nullptr) != VK_SUCCESS ||
                PhysicalDeviceCount == 0)
            {
                throw FVulkanUnavailable("No Vulkan physical device is available for GPU resource tests.");
            }

            std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDeviceCount);

            if (vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, PhysicalDevices.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not enumerate Vulkan physical devices.");
            }

            for (VkPhysicalDevice Candidate : PhysicalDevices)
            {
                std::uint32_t QueueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueFamilyCount, nullptr);
                std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueFamilyCount, QueueFamilies.data());

                for (std::uint32_t Index = 0; Index < QueueFamilyCount; ++Index)
                {
                    if ((QueueFamilies[Index].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
                    {
                        PhysicalDevice = Candidate;
                        QueueFamily = Index;
                        break;
                    }
                }

                if (PhysicalDevice != VK_NULL_HANDLE)
                {
                    break;
                }
            }

            if (PhysicalDevice == VK_NULL_HANDLE)
            {
                throw FVulkanUnavailable("No compute-capable Vulkan queue is available for GPU resource tests.");
            }

            constexpr float         QueuePriority = 1.0F;
            VkDeviceQueueCreateInfo QueueInfo{};
            QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueInfo.queueFamilyIndex = QueueFamily;
            QueueInfo.queueCount = 1;
            QueueInfo.pQueuePriorities = &QueuePriority;

            std::vector<const char*> DeviceExtensions;
            std::uint32_t            ExtensionCount = 0;
            vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
            std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
            vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

            for (const VkExtensionProperties& Extension : AvailableExtensions)
            {
                if (std::string(Extension.extensionName) == "VK_KHR_portability_subset")
                {
                    DeviceExtensions.push_back("VK_KHR_portability_subset");
                    break;
                }
            }

            VkDeviceCreateInfo DeviceInfo{};
            DeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            DeviceInfo.queueCreateInfoCount = 1;
            DeviceInfo.pQueueCreateInfos = &QueueInfo;
            DeviceInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
            DeviceInfo.ppEnabledExtensionNames = DeviceExtensions.data();

            if (vkCreateDevice(PhysicalDevice, &DeviceInfo, nullptr, &Device) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create Vulkan device for GPU resource tests.");
            }

            vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);
            VkCommandPoolCreateInfo CommandPoolInfo{};
            CommandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            CommandPoolInfo.flags =
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            CommandPoolInfo.queueFamilyIndex = QueueFamily;

            if (vkCreateCommandPool(Device, &CommandPoolInfo, nullptr, &CommandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create Vulkan command pool for GPU resource tests.");
            }
        }

        ~FVulkanTestDevice()
        {
            if (Device != VK_NULL_HANDLE)
            {
                vkDeviceWaitIdle(Device);

                if (CommandPool != VK_NULL_HANDLE)
                {
                    vkDestroyCommandPool(Device, CommandPool, nullptr);
                }

                vkDestroyDevice(Device, nullptr);
            }

            if (Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(Instance, nullptr);
            }
        }

        FVulkanTestDevice(const FVulkanTestDevice&) = delete;
        FVulkanTestDevice& operator=(const FVulkanTestDevice&) = delete;

        [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept
        {
            return PhysicalDevice;
        }

        [[nodiscard]] VkDevice GetDevice() const noexcept
        {
            return Device;
        }

        void Execute(const std::function<void(VkCommandBuffer)>& Record)
        {
            VkCommandBufferAllocateInfo AllocateInfo{};
            AllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            AllocateInfo.commandPool = CommandPool;
            AllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            AllocateInfo.commandBufferCount = 1;
            VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;

            if (vkAllocateCommandBuffers(Device, &AllocateInfo, &CommandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not allocate Vulkan command buffer for GPU resource tests.");
            }

            VkCommandBufferBeginInfo BeginInfo{};
            BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not begin Vulkan command buffer for GPU resource tests.");
            }

            Record(CommandBuffer);

            if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not finish Vulkan command buffer for GPU resource tests.");
            }

            VkSubmitInfo SubmitInfo{};
            SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            SubmitInfo.commandBufferCount = 1;
            SubmitInfo.pCommandBuffers = &CommandBuffer;

            if (vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS ||
                vkQueueWaitIdle(Queue) != VK_SUCCESS)
            {
                vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
                throw std::runtime_error("Could not submit Vulkan command buffer for GPU resource tests.");
            }

            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
        }

    private:
        VkInstance       Instance = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        std::uint32_t    QueueFamily = 0;
        VkDevice         Device = VK_NULL_HANDLE;
        VkQueue          Queue = VK_NULL_HANDLE;
        VkCommandPool    CommandPool = VK_NULL_HANDLE;
    };

    MDSS::SurfaceState::FSharedSurfaceGeometryData BuildGeometry()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        FSharedSurfaceGeometryData Geometry({{0, {2, 2}}});

        for (std::size_t Index = 0; Index < Geometry.GetTexels().size(); ++Index)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 2.0F, 3.0F};
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>((Index + 1U) % Geometry.GetTexelCount());
        }

        Geometry.SetProfileMap({0, 0, 0, 0});
        return Geometry;
    }

    void CheckZeroBuffer(const MDSS::GPU::FGPUBuffer& Buffer, std::size_t ScalarCount, const std::string& Name)
    {
        std::vector<float> Values(ScalarCount, -1.0F);
        Buffer.Download(Values.data(), static_cast<VkDeviceSize>(Values.size() * sizeof(float)));

        for (float Value : Values)
        {
            Check(Value == 0.0F, Name + " should start with zero values");
        }
    }

#pragma endregion

// ============================================================
// 02. TestCompactScratchStorage
// ============================================================
#pragma region "02_TestCompactScratchStorage"
// 기대 조건: storage switches preserve State and its buffer

    void TestCompactScratchStorage(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS::SurfaceState;
        const std::vector<float>     Weights(4U * SurfaceNeighborCount, 0.5F);
        FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                              Vulkan.GetDevice(),
                                              4,
                                              2,
                                              Weights,
                                              {},
                                              {},
                                              nullptr,
                                              FSurfaceScratchStorageFormat{true, true, false, true});
        const std::array<float, 8>   State{1, 2, 3, 4, 5, 6, 7, 8};
        Instance.GetStateABuffer().Upload(State.data(), sizeof(State));
        const auto StateHandle = Instance.GetStateABuffer().GetHandle();
        Check(Instance.GetRawOutgoingBuffer().GetSize() == 8U * 5U * sizeof(float),
              "FP16 flux allocates five words per State value");
        FSurfaceInstanceGPUResources FullHalf(Vulkan.GetPhysicalDevice(),
                                              Vulkan.GetDevice(),
                                              4,
                                              2,
                                              Weights,
                                              {},
                                              {},
                                              nullptr,
                                              FSurfaceScratchStorageFormat{true, true, true, false});
        std::uint32_t                PackedHalf = 0;
        FullHalf.GetTransferWeightBuffer().Download(&PackedHalf, sizeof(PackedHalf));
        Check(PackedHalf == 0x38003800U, "FP16 initialization also packs weights in an uncompressed-capacity buffer");

        for (const auto Format :
             std::array<FSurfaceScratchStorageFormat, 5>{FSurfaceScratchStorageFormat{false, false, false, true},
                                                         FSurfaceScratchStorageFormat{true, false, true, true},
                                                         FSurfaceScratchStorageFormat{true, true, true, true},
                                                         FSurfaceScratchStorageFormat{true, false, false, true},
                                                         FSurfaceScratchStorageFormat{true, true, false, false}})
        {
            Instance.ConfigureScratchStorage(Vulkan.GetPhysicalDevice(),
                                             Vulkan.GetDevice(),
                                             Format.bCacheRawFlux,
                                             Format.bHalfRawFlux,
                                             Format.bHalfWeights,
                                             Format.bCompact);
            const auto EdgeWords = !Format.bCompact ? 8U : !Format.bCacheRawFlux ? 0U : Format.bHalfRawFlux ? 4U : 8U;
            Check(Instance.GetRawOutgoingBuffer().GetSize() == 8U * (EdgeWords + 1U) * sizeof(float),
                  "flux size follows storage format");
            Check(Instance.GetTransferWeightBuffer().GetSize() ==
                      4U * (Format.bCompact && Format.bHalfWeights ? 4U : 8U) * sizeof(float),
                  "weight size follows storage format");
            std::array<float, 8> Actual{};
            Instance.GetStateABuffer().Download(Actual.data(), sizeof(Actual));
            Check(Actual == State && Instance.GetStateABuffer().GetHandle() == StateHandle,
                  "storage switches preserve State and its buffer");
            // Emulate the static-cache upload performed by the system after each precision change.
            Instance.UpdateTransferWeights(Weights, {}, Format.bHalfWeights);
        }
    }

#pragma endregion

// ============================================================
// 03. TestLazyMeshStorage
// ============================================================
#pragma region "03_TestLazyMeshStorage"
// 요청한 렌더 메시 variant에 필요한 자원만 준비되는지 검증한다.

    void TestLazyMeshStorage(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS::SurfaceState;
        auto Geometry = BuildGeometry();

        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            Geometry.GetTexels()[Index].Position = {float(Index % 2U), float(Index / 2U), 0.0F};
            Geometry.GetTexels()[Index].Normal = {0.0F, 0.0F, 1.0F};
        }

        FSurfaceSharedGeometryGPUResources Shared(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        const auto                         FullHandle = Shared.GetTexelMeshIndexBuffer()->GetHandle();
        const auto                         SmallHandle = Shared.GetTexelMeshVariant(64).VertexBuffer->GetHandle();
        (void)Shared.GetTexelMeshVariant(128);
        const auto Before = MDSS::GPU::FGPUResourceMemoryStats::GetSnapshot().CurrentBufferBytes;
        Shared.TrimRenderMeshVariants(64, 64);
        const auto After = MDSS::GPU::FGPUResourceMemoryStats::GetSnapshot().CurrentBufferBytes;
        Check(After < Before, "trimming an unused mesh actually frees its Vulkan allocation");
        Check(Shared.GetTexelMeshVariant(64).VertexBuffer->GetHandle() == SmallHandle,
              "trimming preserves the selected render mesh");
        Check(Shared.GetTexelMeshIndexBuffer()->GetHandle() == FullHandle,
              "trimming preserves the simulation topology mesh");
        Check(Shared.GetTexelMeshVariant(128).VertexCount > 0U, "a released resolution can be rebuilt on demand");
    }

#pragma endregion

// ============================================================
// 04. TestRetiredBufferReuse
// ============================================================
#pragma region "04_TestRetiredBufferReuse"
// 기대 조건: retired Scene buffers should remain available for reuse

    void TestRetiredBufferReuse(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS::SurfaceState;
        FSurfaceGPUBufferPool    Pool;
        const std::vector<float> Weights(4U * SurfaceNeighborCount, 1.0F);
        {
            FSurfaceInstanceGPUResources Previous(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, Weights);
            const std::array<float, 4>   DirtyState{1.0F, 2.0F, 3.0F, 4.0F};
            Previous.GetStateABuffer().Upload(DirtyState.data(), sizeof(DirtyState));
            Previous.RetireBuffers(Pool);
        }

        Check(Pool.GetRetainedBytes() > 0, "retired Scene buffers should remain available for reuse");
        FSurfaceInstanceGPUResources Replacement(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, Weights, {}, {}, &Pool);
        Check(Pool.GetReuseCount() > 0, "matching Scene buffers should be reused");
        CheckZeroBuffer(Replacement.GetStateABuffer(), 4, "reused State A buffer");
        CheckZeroBuffer(Replacement.GetStateBBuffer(), 4, "reused State B buffer");
        CheckZeroBuffer(
            Replacement.GetRawOutgoingBuffer(), 4U * (SurfaceNeighborCount + 1U), "reused raw outgoing buffer");
    }

#pragma endregion

// ============================================================
// 05. TestGPUResources
// ============================================================
#pragma region "05_TestGPUResources"
// 기대 조건: shared Geometry should retain four texels

    void TestGPUResources(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        VkPhysicalDeviceProperties DeviceProperties{};
        vkGetPhysicalDeviceProperties(Vulkan.GetPhysicalDevice(), &DeviceProperties);
        const std::uint32_t RequiredStorageBufferBindings =
            static_cast<std::uint32_t>(SurfaceState::ESurfaceGPUDescriptorBinding::Count);
        if (DeviceProperties.limits.maxDescriptorSetStorageBuffers < RequiredStorageBufferBindings ||
            DeviceProperties.limits.maxPerStageDescriptorStorageBuffers < RequiredStorageBufferBindings)
        {
            throw FVulkanUnavailable("Vulkan device does not support the Surface descriptor storage-buffer count.");
        }

        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 3.0F;
        Parameters.InputFactor = 0.75F;
        Parameters.SaturationTransferFactor = 0.2F;
        Parameters.GeometryTransferFactor = 0.5F;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(ProfileTable);
        FSharedSurfaceGeometryData                                   Geometry = BuildGeometry();

        SurfaceState::FSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        const std::vector<float> GeometryWeights = BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F));
        SurfaceState::FSurfaceInstanceGPUResources InstanceA(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, GeometryWeights);
        SurfaceState::FSurfaceInstanceGPUResources InstanceB(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, GeometryWeights);
        SurfaceState::FSurfaceStateDescriptorResources DescriptorsA(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceA);
        SurfaceState::FSurfaceStateDescriptorResources DescriptorsB(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceB);

        Check(SharedGeometry.GetTexelCount() == 4, "shared Geometry should retain four texels");
        const auto SparseLayout = GetSurfaceSparseMetadataLayout(4);
        Check(SparseLayout.SolverAWorkgroupMaskWord == SparseLayout.SolverAMaskWord + 1U &&
                  SparseLayout.SolverBWorkgroupMaskWord == SparseLayout.SolverBMaskWord + 1U &&
                  SparseLayout.InputGroupMaskWord == SparseLayout.InputListWord + SparseLayout.WorkgroupCount &&
                  SparseLayout.AccumulationCommandWord == SparseLayout.InputGroupMaskWord + SparseLayout.WorkgroupCount,
              "per-WG A/B masks and compact input masks should not overlap other sparse metadata");
        auto MappingGeometry = BuildGeometry();

        for (std::size_t Index = 0; Index < MappingGeometry.GetTexels().size(); ++Index)
            MappingGeometry.GetTexels()[Index].Position = {float(Index % 2U), float(Index / 2U), 0.0F};
        FSurfaceSharedGeometryGPUResources MappingShared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), MappingGeometry);
        const auto& RenderMesh = MappingShared.GetTexelMeshVariant(2U);
        Check(RenderMesh.IndexBuffer != nullptr, "nondegenerate test surface should produce overlay triangles");

        if (RenderMesh.IndexBuffer)
            for (const std::uint32_t TileSize : {8U, 16U, 32U})
            {
                const auto& Mapping = RenderMesh.TileVertexLists.at(TileSize);
                const auto& Words = Mapping.Words;
                Check(Words.size() == static_cast<std::size_t>(Mapping.TileCount) + 1U + RenderMesh.VertexCount,
                      "tile-to-vertex map should contain offsets followed by every mesh vertex");
                std::vector<std::uint32_t> Seen(RenderMesh.VertexCount, 0U);

                for (std::uint32_t Tile = 0; Tile < Mapping.TileCount; ++Tile)
                {
                    const auto Begin = Words[Tile];
                    const auto End = Words[Tile + 1U];
                    Check(Begin >= Mapping.TileCount + 1U && End >= Begin && End <= Words.size(),
                          "tile-to-vertex offsets should remain inside the packed map");
                    for (std::size_t Offset = Begin; Offset < End && Offset < Words.size(); ++Offset)
                        if (Words[Offset] < Seen.size())
                            ++Seen[Words[Offset]];
                }

                for (const auto Count : Seen)
                    Check(Count == 1U, "each overlay vertex should belong to exactly one render tile");
            }

        Check(SharedGeometry.GetNeighborIndexBuffer().GetSize() ==
                  4U * sizeof(SurfaceState::FSurfaceGPUNeighborIndices),
              "neighbor buffer should use the expected per-texel stride");
        Check(DescriptorsA.GetABSet() != VK_NULL_HANDLE && DescriptorsA.GetBASet() != VK_NULL_HANDLE,
              "both A/B descriptor sets should be allocated");
        Check(DescriptorsB.GetABSet() != VK_NULL_HANDLE && DescriptorsB.GetBASet() != VK_NULL_HANDLE,
              "each instance should own its descriptor sets");

        const auto Bound = [](const SurfaceState::FSurfaceStateDescriptorResources& Descriptors,
                              SurfaceState::ESurfaceGPUDescriptorBinding            Binding,
                              bool bAB) { return Descriptors.GetBoundBufferHandle(Binding, bAB); };
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  SharedGeometry.GetTexelSurfaceIndexBuffer().GetHandle(),
              "descriptor should bind the shared Geometry buffer");
        Check(Bound(DescriptorsB, SurfaceState::ESurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::TexelSurfaceIndices, true),
              "instances should bind the same shared Geometry buffer");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::CurrentState, true) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "AB descriptor should read State A");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::NextState, true) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "AB descriptor should write State B");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::CurrentState, false) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "BA descriptor should read State B");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::NextState, false) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "BA descriptor should write State A");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::TransferWeights, true) ==
                      InstanceA.GetTransferWeightBuffer().GetHandle() &&
                  Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::RawOutgoing, true) ==
                      InstanceA.GetRawOutgoingBuffer().GetHandle(),
              "instance descriptors should bind the TransferWeight and RawOutgoing caches");
        Check(InstanceA.GetStateABuffer().GetHandle() != InstanceB.GetStateABuffer().GetHandle() &&
                  InstanceA.GetStateBBuffer().GetHandle() != InstanceB.GetStateBBuffer().GetHandle(),
              "separate instances should own separate State buffers");
        Check(Bound(DescriptorsB, SurfaceState::ESurfaceGPUDescriptorBinding::CurrentState, true) ==
                  InstanceB.GetStateABuffer().GetHandle(),
              "second instance descriptor should refer to its own state");

        CheckZeroBuffer(InstanceA.GetStateABuffer(), 4, "State A");
        CheckZeroBuffer(InstanceA.GetStateBBuffer(), 4, "State B");
        CheckZeroBuffer(InstanceA.GetOutgoingFluxScaleBuffer(), 4, "OutgoingFluxScale");
        CheckZeroBuffer(InstanceA.GetInputDeltaBuffer(), 4, "InputDelta");
        CheckZeroBuffer(InstanceA.GetRawOutgoingBuffer(), 4, "RawOutgoing");
        CheckZeroBuffer(InstanceB.GetStateABuffer(), 4, "second instance State A");
        CheckZeroBuffer(InstanceB.GetStateBBuffer(), 4, "second instance State B");
        CheckZeroBuffer(InstanceB.GetOutgoingFluxScaleBuffer(), 4, "second instance OutgoingFluxScale");
        CheckZeroBuffer(InstanceB.GetInputDeltaBuffer(), 4, "second instance InputDelta");
        CheckZeroBuffer(InstanceB.GetRawOutgoingBuffer(), 4, "second instance RawOutgoing");
        Check(InstanceA.GetTransferWeightBuffer().GetSize() == 4U * SurfaceNeighborCount * sizeof(float),
              "TransferWeight cache should store one float per texel neighbor direction index");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::ReverseNeighborDirectionIndices, true) ==
                      SharedGeometry.GetReverseNeighborDirectionIndexBuffer().GetHandle() &&
                  Bound(DescriptorsB,
                        SurfaceState::ESurfaceGPUDescriptorBinding::ReverseNeighborDirectionIndices,
                        false) == SharedGeometry.GetReverseNeighborDirectionIndexBuffer().GetHandle(),
              "reverse direction indices should be shared with Geometry across instances and AB/BA sets");
        Check(Bound(DescriptorsA, SurfaceState::ESurfaceGPUDescriptorBinding::WorldTexelAreas, true) ==
                  InstanceA.GetWorldTexelAreaBuffer().GetHandle(),
              "WorldTexelAreas should follow the retained reverse-direction binding without a RawFlux binding");

        std::array<SurfaceState::FSurfaceGPUVec4, 4> UploadedPositions{};
        SharedGeometry.GetPositionBuffer().Download(UploadedPositions.data(), sizeof(UploadedPositions));
        Check(UploadedPositions[2].X == 2.0F && UploadedPositions[2].Y == 2.0F && UploadedPositions[2].Z == 3.0F,
              "position upload should preserve texel-major values");

        std::array<SurfaceState::FSurfaceGPUNeighborIndices, 4> UploadedNeighbors{};
        SharedGeometry.GetNeighborIndexBuffer().Download(UploadedNeighbors.data(), sizeof(UploadedNeighbors));
        Check(UploadedNeighbors[2].Indices[0] == 3U, "neighbor upload should preserve the local neighbor index");

        std::array<SurfaceState::FSurfaceGPUProfileParameters, 1> UploadedProfiles{};
        Profiles.GetParametersBuffer().Download(UploadedProfiles.data(), sizeof(UploadedProfiles));
        Check(UploadedProfiles[0].CapacityInputAndTransfer[0] == 3.0F &&
                  UploadedProfiles[0].CapacityInputAndTransfer[1] == 0.75F &&
                  UploadedProfiles[0].CapacityInputAndTransfer[2] == 0.2F &&
                  UploadedProfiles[0].CapacityInputAndTransfer[3] == 0.5F,
              "Profile ABI slots should contain normalized factors; the solver applies base rates");
        std::array<std::uint32_t, 1> ProfileSupported{};
        Profiles.GetSupportedBuffer().Download(ProfileSupported.data(), sizeof(ProfileSupported));
        Check(ProfileSupported[0] == 1U, "defined Profile channel should be marked supported");
    }

#pragma endregion

} // namespace

// ============================================================
// 06. Test Entry Point
// ============================================================
#pragma region "06_Test_Entry_Point"
// 선택한 회귀 사례를 실행하고 실패를 프로세스 종료 코드로 전달한다.

    int main(int argc, char **argv)
    {
        const std::string Mode = argc > 1 ? argv[1] : "";
        try
        {
            if (Mode == "--meso-peak-only")
            {
                TestMesoPeakPacking();
            }
            else
            {
                FVulkanTestDevice Vulkan;
                TestCompactScratchStorage(Vulkan);
                if (Mode == "--storage-and-mesh-only")
                    TestLazyMeshStorage(Vulkan);
                else if (Mode != "--compact-storage-only")
                {
                    TestRetiredBufferReuse(Vulkan);
                    if (Mode != "--reuse-only")
                        TestGPUResources(Vulkan);
                }
            }
        }
        catch (const FVulkanUnavailable &Exception)
        {
            std::cout << "SKIP: " << Exception.what() << '\n';
            return 77;
        }
        catch (const std::exception &Exception)
        {
            std::cerr << "GPU resource test setup failed: " << Exception.what() << '\n';
            return 1;
        }

        if (FailureCount != 0)
        {
            std::cerr << FailureCount << " GPU resource check(s) failed.\n";
            return 1;
        }

        std::cout << "Surface GPU resource checks passed.\n";
        return 0;
    }

#pragma endregion
