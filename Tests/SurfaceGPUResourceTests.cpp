/**
 * @file SurfaceGPUResourceTests.cpp
 * @brief Vulkan resource layout, 초기화와 descriptor 연결을 검증한다.
 */

#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"
#include "SurfaceState/Solver/SurfaceStateSolver.h"
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

// ============================================================
// 06. TestDirectionalCavityRetention
// ============================================================
#pragma region "06_TestDirectionalCavityRetention"

    void TestDirectionalCavityRetention(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceStateParameters Parameters{};
        Parameters.SaturationTransferFactor = 1.0F;
        Parameters.CavityTransportRetentionFactor = 1.0F;
        SurfaceState::FSurfaceResponseProfileData Profile;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(Table);
        FSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});

        for (std::size_t I = 0; I < 2; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = Texel.Triangle = Texel.Chart = 0;
            Texel.Position = {float(I), 0.0F, 0.0F};
            Texel.Normal = {0.0F, 0.0F, 1.0F};
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - I);
        }

        Geometry.GetTexels()[0].Geometry.ConcavityWeight = 1.0F;
        Geometry.SetProfileMap({0, 0});
        SurfaceState::FSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const auto                                     Run = [&](std::array<float, 2> Initial,
                                                                 const glm::mat4&     Model = glm::mat4(1.0F),
                                                                 glm::vec3            Gravity = glm::vec3(0.0F))
        {
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute([&](VkCommandBuffer Command)
                           { Solver.RecordStep(Command, Descriptors, true, 2, 1, 0.25F, Model, Gravity); });
            std::array<float, 2> Result{};
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));

            return Result;
        };
        const auto Exit = Run({1.0F, 0.0F});
        Check(std::abs(Exit[0] - 1.0F) < 1.0e-5F && std::abs(Exit[1]) < 1.0e-5F,
              "cavity exit should be blocked at full directional retention");
        const auto Entry = Run({0.0F, 1.0F});
        Check(std::abs(Entry[0] - 0.25F) < 1.0e-5F && std::abs(Entry[1] - 0.75F) < 1.0e-5F,
              "entry into a cavity should remain mobile and conserve State");
        Parameters.CavityTransportRetentionFactor = 0.0F;
        Profiles.UpdateParameters(0, 0, Parameters);
        const auto Neutral = Run({1.0F, 0.0F});
        Check(std::abs(Neutral[0] - 0.75F) < 1.0e-5F && std::abs(Neutral[1] - 0.25F) < 1.0e-5F,
              "zero cavity retention should preserve the previous transport result");
        Parameters.SaturationTransferFactor = 0.008F;
        Parameters.GeometryTransferFactor = 0.002F;
        Parameters.CavityTransportRetentionFactor = 0.98F;
        Profiles.UpdateParameters(0, 0, Parameters);
        const glm::mat4 Inverted = glm::rotate(glm::mat4(1.0F), glm::radians(180.0F), glm::vec3(1, 0, 0));
        const auto      Retained = Run({1.0F, 0.0F}, Inverted, glm::vec3(1.0F, 0.0F, 0.0F));
        Parameters.CavityTransportRetentionFactor = 0.0F;
        Profiles.UpdateParameters(0, 0, Parameters);
        const auto Unretained = Run({1.0F, 0.0F}, Inverted, glm::vec3(1.0F, 0.0F, 0.0F));
        Check(Retained[1] > 0.0F && Retained[1] < Unretained[1] * 0.1F,
              "inverted Lava-like cavity must strongly resist downhill exit");
    }

#pragma endregion

// ============================================================
// 07. TestGPUSolver
// ============================================================
#pragma region "07_TestGPUSolver"

    void TestGPUSolver(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 1.0F;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(ProfileTable);

        FSharedSurfaceGeometryData Geometry({{0, {2, 1}}});

        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - Index);
        }

        Geometry.SetProfileMap({0, 0});

        SurfaceState::FSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        const std::array<float, 2> Input{1.0F, 0.0F};
        Instance.GetInputDeltaBuffer().Upload(Input.data(), sizeof(Input));
        auto DispatchAndRead =
            [&](bool bCurrentStateAB, std::array<float, 2>& State, std::array<float, 2>& RemainingInput)
        {
            Vulkan.Execute(
                [&](VkCommandBuffer CommandBuffer)
                {
                    Solver.RecordStep(CommandBuffer,
                                      Descriptors,
                                      bCurrentStateAB,
                                      2,
                                      1,
                                      0.25F,
                                      glm::mat4(1.0F),
                                      glm::vec3(0.0F, -1.0F, 0.0F));
                });
            const GPU::FGPUBuffer& StateBuffer =
                bCurrentStateAB ? Instance.GetStateBBuffer() : Instance.GetStateABuffer();
            StateBuffer.Download(State.data(), sizeof(State));
            Instance.GetInputDeltaBuffer().Download(RemainingInput.data(), sizeof(RemainingInput));
        };

        std::array<float, 2> State{};
        std::array<float, 2> RemainingInput{};
        DispatchAndRead(true, State, RemainingInput);
        Check(std::abs(State[0] - 1.0F) < 1.0e-5F && std::abs(State[1]) < 1.0e-5F,
              "first solver step should apply the one-shot input to texel zero");
        Check(RemainingInput[0] == 0.0F && RemainingInput[1] == 0.0F,
              "solver should consume and clear InputDelta after applying it");

        DispatchAndRead(false, State, RemainingInput);
        Check(std::abs(State[0] - 0.75F) < 1.0e-4F && std::abs(State[1] - 0.25F) < 1.0e-4F,
              "second solver step should transfer state across the neighbor edge");
        Check(std::abs(State[0] + State[1] - 1.0F) < 1.0e-4F,
              "saturation transfer without decay should conserve total state");

        DispatchAndRead(true, State, RemainingInput);
        Check(std::abs(State[0] - 0.625F) < 1.0e-4F && std::abs(State[1] - 0.375F) < 1.0e-4F,
              "third solver step should continue deterministic diffusion");
    }

#pragma endregion

// ============================================================
// 08. TestPerWorkgroupChannelMask
// ============================================================
#pragma region "08_TestPerWorkgroupChannelMask"

    void TestPerWorkgroupChannelMask(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::SurfaceState;
        FSurfaceStateParameters Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 1.0F;
        FSurfaceResponseProfileData Profile;
        Profile.States.emplace("mud", Parameters);
        Profile.States.emplace("heat", Parameters);
        const std::vector<FSurfaceResponseProfileData> Table{Profile};
        const FSurfaceStateRegistry                    Registry(Table);
        constexpr std::uint32_t                        Texels = 65U;
        const auto                                     Channels = static_cast<std::uint32_t>(Registry.GetStateCount());
        const auto                                     Mud = Registry.GetStateId("mud");
        const auto                                     Heat = Registry.GetStateId("heat");
        FSharedSurfaceGeometryData                     Geometry({{0, {Texels, 1}}});

        for (std::uint32_t I = 0; I < Texels; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = Texel.Triangle = Texel.Chart = 0;
            Texel.Position = {float(I), 0.0F, 0.0F};
            Texel.Normal = {0.0F, 0.0F, 1.0F};
        }

        Geometry.GetTexels()[63].NeighborIndices[0] = 64;
        Geometry.GetTexels()[64].NeighborIndices[0] = 63;
        Geometry.SetProfileMap(std::vector<FSurfaceProfileIndex>(Texels, 0U));
        FSurfaceSharedGeometryGPUResources Shared(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        FSurfaceProfileGPUResources        Profiles(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        FSurfaceInstanceGPUResources       Instance(Vulkan.GetPhysicalDevice(),
                                                    Vulkan.GetDevice(),
                                                    Texels,
                                                    Channels,
                                                    BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        FSurfaceStateDescriptorResources   Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        FSurfaceStateSolver                Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        std::vector<float>                 Initial(static_cast<std::size_t>(Texels) * Channels, 0.0F);
        Initial[63U * Channels + Mud] = 1.0F;
        Initial[Heat] = 1.0F;
        Instance.GetStateABuffer().Upload(Initial.data(), Initial.size() * sizeof(float));
        const std::uint32_t Flags = SurfaceSolverSparseSolverFlag | SurfaceSolverActiveChannelMaskFlag |
                                    SurfaceSolverPerWorkgroupChannelMaskFlag;
        Vulkan.Execute(
            [&](VkCommandBuffer Command)
            {
                Solver.RecordStep(Command,
                                  Descriptors,
                                  true,
                                  Texels,
                                  Channels,
                                  0.0F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F),
                                  Flags | SurfaceSolverSeedPersistentActivityFlag);
            });
        const auto                   Layout = GetSurfaceSparseMetadataLayout(Texels);
        std::array<std::uint32_t, 2> NextGroupMasks{};
        Instance.GetAccumulationHeightBuffer().Download(
            NextGroupMasks.data(), sizeof(NextGroupMasks), Layout.SolverBWorkgroupMaskWord * sizeof(std::uint32_t));
        Check((NextGroupMasks[0] & (1U << Mud)) != 0U && (NextGroupMasks[0] & (1U << Heat)) != 0U &&
                  NextGroupMasks[1] == (1U << Mud),
              "per-WG mask should extend Mud one hop across the WG boundary without extending unrelated Heat");
        Vulkan.Execute(
            [&](VkCommandBuffer Command)
            {
                Solver.RecordStep(
                    Command, Descriptors, false, Texels, Channels, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F), Flags);
            });
        std::vector<float> Result(Initial.size(), 0.0F);
        Instance.GetStateABuffer().Download(Result.data(), Result.size() * sizeof(float));
        Check(Result[64U * Channels + Mud] > 0.0F,
              "per-WG mask should permit incoming transport into an initially empty WG");
        Parameters.DecayRate = 100.0F;
        Profiles.UpdateParameters(0U, Mud, Parameters);
        Vulkan.Execute(
            [&](VkCommandBuffer Command)
            {
                Solver.RecordStep(
                    Command, Descriptors, true, Texels, Channels, 1.0F, glm::mat4(1.0F), glm::vec3(0.0F), Flags);
            });
        Instance.GetStateBBuffer().Download(Result.data(), Result.size() * sizeof(float));
        Check(Result[64U * Channels + Mud] == 0.0F,
              "decay should make the destination WG empty before ping-pong cleanup");
        Instance.GetAccumulationHeightBuffer().Download(
            NextGroupMasks.data(), sizeof(NextGroupMasks), Layout.SolverBWorkgroupMaskWord * sizeof(std::uint32_t));
        Check((NextGroupMasks[1] & (1U << Mud)) != 0U,
              "per-WG mask should retain a channel for one cleanup generation after it reaches zero");
        Vulkan.Execute(
            [&](VkCommandBuffer Command)
            {
                Solver.RecordStep(
                    Command, Descriptors, false, Texels, Channels, 0.0F, glm::mat4(1.0F), glm::vec3(0.0F), Flags);
            });
        Instance.GetStateABuffer().Download(Result.data(), Result.size() * sizeof(float));
        Check(Result[64U * Channels + Mud] == 0.0F,
              "per-WG cleanup should overwrite stale state in the opposite ping-pong buffer");
    }

#pragma endregion

// ============================================================
// 09. TestAreaAndMobility
// ============================================================
#pragma region "09_TestAreaAndMobility"

    void TestAreaAndMobility(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceStateParameters Parameters{};
        Parameters.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;

        // Nonzero Saturation factor verifies that its debug flag is independent of Geometry mobility.
        Parameters.SaturationTransferFactor = 1.0F;
        SurfaceState::FSurfaceResponseProfileData Profile;
        Profile.States.emplace("test_flow", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(Table);
        FSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});

        for (std::size_t I = 0; I < 2; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {0, 0, 1.0F - float(I)};
            Texel.Normal = {1, 0, 0};
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1 - I);
        }

        Geometry.SetProfileMap({0, 0});
        SurfaceState::FSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(
            Vulkan.GetPhysicalDevice(),
            Vulkan.GetDevice(),
            2,
            1,
            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1)),
            {},
            {2 * SurfaceStateReferenceArea, 4 * SurfaceStateReferenceArea});
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const std::uint32_t                            Flags = (1U << 1U);
        auto Run = [&](std::array<float, 2> Initial, std::uint32_t ExtraFlags = 0U)
        {
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
                {
                    Solver.RecordStep(
                        Commands, Descriptors, true, 2, 1, 0.1F, glm::mat4(1), glm::vec3(0, 0, -1), Flags | ExtraFlags);
                });
            std::array<float, 2> Result{};
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));

            return Result;
        };
        const auto A = Run({4, 0}); // Capacity=2, source saturation=2.
        const auto B = Run({8, 0}); // source saturation=4, no upper clamp.
        Check(std::abs(A[1] - 0.2F) < 1e-5F && std::abs(B[1] - 0.4F) < 1e-5F,
              "Geometry must use area-adjusted Capacity and remain proportional above saturation one, with "
              "SaturationDrive OFF");
        Check(std::abs(A[0] + A[1] - 4) < 1e-5F && std::abs(B[0] + B[1] - 8) < 1e-5F,
              "Geometry mobility must conserve the total amount");
        Parameters.GeometryTransferFactor = 0;
        Parameters.SaturationTransferFactor = 1;
        Profiles.UpdateParameters(0, 0, Parameters);

        // Re-enable SaturationDrive explicitly for unequal area at equal saturation.
        const std::array<float, 2> EqualInitial{2, 4};
        Instance.GetStateABuffer().Upload(EqualInitial.data(), sizeof(EqualInitial));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            { Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.1F, glm::mat4(1), glm::vec3(0, 0, -1)); });
        std::array<float, 2> Result{};
        Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
        Check(Result == EqualInitial, "unequal amounts at equal area-adjusted saturation must not diffuse");
        Parameters.DecayRate = 1;
        Profiles.UpdateParameters(0, 0, Parameters);
        const auto Decayed = Run({2, 4});
        Check(std::abs(Decayed[0] - 1.8F) < 1e-5F && std::abs(Decayed[1] - 3.6F) < 1e-5F,
              "Decay must scale with area to retain equal density loss");
        Parameters.DecayRate = 0;
        Parameters.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        Profiles.UpdateParameters(0, 0, Parameters);
        const std::array<float, 2> Empty{};
        const std::array<float, 2> Input{0.3F, 0};
        Instance.GetStateABuffer().Upload(Empty.data(), sizeof(Empty));
        Instance.GetInputDeltaBuffer().Upload(Input.data(), sizeof(Input));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.1F, glm::mat4(1), glm::vec3(0, 0, -1), Flags);
                Solver.RecordStep(Commands, Descriptors, false, 2, 1, 0.1F, glm::mat4(1), glm::vec3(0, 0, -1), Flags);
            });
        Instance.GetStateABuffer().Download(Result.data(), sizeof(Result));
        Check(std::abs(Result[1] - 0.015F) < 1e-5F && std::abs(Result[0] + Result[1] - 0.3F) < 1e-5F,
              "substeps in one command buffer must use updated State and apply input exactly once");
        for (float Resolution : {128.0F, 256.0F, 512.0F})
        {
            // Local one-edge scaling check, distinct from an entire-scene resolution comparison.
            const float                                        H = 1.0F / Resolution;
            const std::array<SurfaceState::FSurfaceGPUVec4, 2> Positions{{{0, 0, H, 0}, {0, 0, 0, 0}}};
            Shared.GetPositionBuffer().Upload(Positions.data(), sizeof(Positions));
            Instance.UpdateWorldTexelAreas({H * H, H * H});
            const float Mass = H * H / SurfaceStateReferenceArea;
            const auto  Scaled = Run({Mass, 0});
            const float WorldDisplacement = Scaled[1] * H / Mass;
            const float ExpectedDisplacement = 0.1F * SurfaceStateReferenceArea;
            Check(std::abs(WorldDisplacement - ExpectedDisplacement) < ExpectedDisplacement * 1e-4F,
                  "Geometry one-edge world displacement must retain its scale at 128/256/512");
            Check(std::abs(Scaled[0] + Scaled[1] - Mass) < Mass * 1e-5F,
                  "resolution scaling must conserve total amount");
        }

        Instance.UpdateWorldTexelAreas({0, 0});
        const auto Collapsed = Run({3, 7});
        Check(Collapsed == std::array<float, 2>{3, 7},
              "zero area must freeze transport while retaining existing amounts");
    }

#pragma endregion

// ============================================================
// 10. TestCalibratedGeometrySpeed
// ============================================================
#pragma region "10_TestCalibratedGeometrySpeed"

    void TestCalibratedGeometrySpeed(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.GeometryTransferFactor = 0.5F;
        Profile.States.emplace("flow", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(Table);
        constexpr std::size_t Width = 5, Count = Width * Width, Source = Count / 2;
        constexpr float       Dt = 0.001F;

        for (float Resolution : {128.0F, 256.0F, 512.0F})
        {
            const float                H = 1.0F / Resolution;
            FSharedSurfaceGeometryData Geometry({{0, {Width, Width}}});

            for (int Y = 0; Y < int(Width); ++Y)
                for (int X = 0; X < int(Width); ++X)
                {
                    auto& Texel = Geometry.GetTexels()[Y * Width + X];
                    Texel.Surface = 0;
                    Texel.Triangle = 0;
                    Texel.Position = {0, (X - 2) * H, (2 - Y) * H};
                    Texel.Normal = {1, 0, 0};
                    Texel.AreaVector = {H * H, 0, 0};
                    std::size_t DirectionIndex = 0;

                    for (int DY = -1; DY <= 1; ++DY)
                        for (int DX = -1; DX <= 1; ++DX)
                        {
                            if (DX == 0 && DY == 0)
                                continue;
                            if (X + DX >= 0 && X + DX < int(Width) && Y + DY >= 0 && Y + DY < int(Width))
                                Texel.NeighborIndices[DirectionIndex] =
                                    static_cast<FLocalTexelIndex>((Y + DY) * Width + X + DX);
                            ++DirectionIndex;
                        }
                }

            Geometry.SetProfileMap(std::vector<SurfaceState::FSurfaceProfileIndex>(Count, 0));
            SurfaceState::FSurfaceSharedGeometryGPUResources Shared(
                Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
            SurfaceState::FSurfaceProfileGPUResources Profiles(
                Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
            SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                                Vulkan.GetDevice(),
                                                                Count,
                                                                1,
                                                                BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1)),
                                                                {},
                                                                BuildSurfaceGPUWorldTexelAreas(Geometry, glm::mat4(1)));
            SurfaceState::FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
            SurfaceState::FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
            std::array<float, Count>                       Initial{}, Result{};
            const double Mass = 2.0 * H * H / SurfaceStateReferenceArea; // Saturation=2, deliberately above 1.
            Initial[Source] = static_cast<float>(Mass);
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
                { Solver.RecordStep(Commands, Descriptors, true, Count, 1, Dt, glm::mat4(1), glm::vec3(0, 0, -1)); });
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            double Total = 0, DownwardMoment = 0;

            for (std::size_t I = 0; I < Count; ++I)
            {
                Check(std::isfinite(Result[I]) && Result[I] >= 0, "calibrated flow must remain finite and nonnegative");
                Total += Result[I];
                DownwardMoment -= Result[I] * Geometry.GetTexels()[I].Position.z;
            }

            const double Speed = DownwardMoment / Mass / Dt;
            Check(std::abs(Speed - 0.101033333) < 1e-5,
                  "factor 0.5 must produce about 0.101 world units/s at 128/256/512 with the calibrated rate");
            Check(std::abs(Total - Mass) < Mass * 1e-6, "calibrated flow must conserve total amount");
            std::cout << "Calibrated Geometry R=" << Resolution << ": " << Speed << " world units/s\n";
        }
    }

#pragma endregion

// ============================================================
// 11. TestGeometryDrivenSolver
// ============================================================
#pragma region "11_TestGeometryDrivenSolver"

    void TestGeometryDrivenSolver(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 0.0F;
        Parameters.GeometryTransferFactor = 8.0F / BaseGeometryTransferRate;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(ProfileTable);

        FSharedSurfaceGeometryData Geometry({{0, {2, 1}}});

        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = Index == 0 ? glm::vec3(1.0F, 0.0F, 0.0F) : glm::vec3(0.0F);
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - Index);
        }

        Geometry.SetProfileMap({0, 0});

        SurfaceState::FSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        const glm::mat4 ModelMatrix = glm::rotate(glm::mat4(1.0F), glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
        auto            Run = [&](std::array<float, 2> InitialState, glm::vec3 Gravity, float DeltaTime)
        {
            Instance.GetStateABuffer().Upload(InitialState.data(), sizeof(InitialState));
            Vulkan.Execute(
                [&](VkCommandBuffer CommandBuffer)
                { Solver.RecordStep(CommandBuffer, Descriptors, true, 2, 1, DeltaTime, ModelMatrix, Gravity); });

            std::array<float, 2> Result{};
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            return Result;
        };

        const std::array<float, 2> Downhill = Run({1.0F, 0.0F}, glm::vec3(0.0F, -1.0F, 0.0F), 0.0625F);
        Check(std::abs(Downhill[0] - 0.5F) < 1.0e-4F && std::abs(Downhill[1] - 0.5F) < 1.0e-4F,
              "geometry flux should follow world gravity after applying the instance rotation");
        Check(std::abs(Downhill[0] + Downhill[1] - 1.0F) < 1.0e-4F,
              "geometry-only transfer without decay should conserve state");

        const std::array<float, 2> Uphill = Run({1.0F, 0.0F}, glm::vec3(0.0F, 1.0F, 0.0F), 0.25F);
        Check(std::abs(Uphill[0] - 1.0F) < 1.0e-4F && std::abs(Uphill[1]) < 1.0e-4F,
              "geometry flux should be zero when the neighbor direction opposes world gravity");

        const std::array<float, 2> Limited = Run({0.5F, 0.9F}, glm::vec3(0.0F, -1.0F, 0.0F), 0.25F);
        Check(std::abs(Limited[0]) < 1.0e-4F && std::abs(Limited[1] - 1.4F) < 1.0e-4F,
              "유출은 source 보유량으로 제한하고 target의 Capacity 초과량은 보존해야 한다.");
    }

#pragma endregion

// ============================================================
// 12. TestMesoGeometryDrive
// ============================================================
#pragma region "12_TestMesoGeometryDrive"

    void TestMesoGeometryDrive(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        Profile.States.emplace("deposit", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(ProfileTable);
        FSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});

        for (std::size_t Index = 0; Index < 2; ++Index)
        {
            auto& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {1.0F - static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = {0.0F, 0.0F, 1.0F};
            Texel.MesoNormal = glm::normalize(glm::vec3(-1.0F, 0.0F, 1.0F));
            Texel.bHasMesoNormal = true;
            Texel.Geometry.MesoVirtualHeight = Index == 0 ? 0.2F : 0.0F;
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - Index);
        }

        Geometry.SetProfileMap({0, 0});
        SurfaceState::FSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const std::array<float, 2>                     EmptyState{0.0F, 0.0F};
        const std::array<float, 2>                     EventInput{0.125F, 0.0F};
        Instance.GetStateABuffer().Upload(EmptyState.data(), sizeof(EmptyState));
        Instance.GetInputDeltaBuffer().Upload(EventInput.data(), sizeof(EventInput));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(
                    Commands, Descriptors, true, 2, 1, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F));
            });
        std::array<float, 2> EventResult{};
        Instance.GetStateBBuffer().Download(EventResult.data(), sizeof(EventResult));
        Check(std::abs(EventResult[0] - EventInput[0]) < 1.0e-5F && EventResult[1] == 0.0F,
              "empty source skipping should preserve event input and defer its transport to the next step");
        Instance.GetInputDeltaBuffer().Download(EventResult.data(), sizeof(EventResult));
        Check(EventResult[0] == 0.0F && EventResult[1] == 0.0F,
              "empty source skipping should still consume event input exactly once");
        CheckZeroBuffer(Instance.GetOutgoingFluxScaleBuffer(), 2, "empty source alpha");
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(
                    Commands, Descriptors, false, 2, 1, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F));
            });
        Instance.GetStateABuffer().Download(EventResult.data(), sizeof(EventResult));
        Check(EventResult[1] > 0.0F && std::abs(EventResult[0] + EventResult[1] - EventInput[0]) < 1.0e-5F,
              "a previously empty target must receive neighbor flux and transport the previous step's event input");

        Parameters.DecayRate = 4.0F;
        Profiles.UpdateParameters(0, 0, Parameters);
        const std::array<float, 2> DepletedState{0.5F, 0.0F};
        const std::array<float, 2> DepletedInput{0.125F, 0.25F};
        Instance.GetStateABuffer().Upload(DepletedState.data(), sizeof(DepletedState));
        Instance.GetInputDeltaBuffer().Upload(DepletedInput.data(), sizeof(DepletedInput));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(
                    Commands, Descriptors, true, 2, 1, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F));
            });
        Instance.GetStateBBuffer().Download(EventResult.data(), sizeof(EventResult));
        Check(std::abs(EventResult[0] - DepletedInput[0]) < 1.0e-5F &&
                  std::abs(EventResult[1] - DepletedInput[1]) < 1.0e-5F,
              "decay-depleted sources must skip outgoing but still apply external input to both texels");
        Parameters.DecayRate = 0.0F;
        Profiles.UpdateParameters(0, 0, Parameters);

        Check(SurfaceState::FSurfaceSolverDebugSettings{}.IsEnabled(
                  SurfaceState::ESurfaceSolverTerm::MesoDirectionNormal),
              "Meso direction normal should be enabled by default");
        const glm::vec3 Gravity(0.0F, 0.0F, -1.0F);

        for (const glm::mat4 Model :
             {glm::mat4(1.0F),
              glm::scale(glm::mat4(1.0F), glm::vec3(2.0F, 1.0F, 0.5F)),
              glm::translate(glm::rotate(glm::scale(glm::mat4(1.0F), glm::vec3(2.0F, 1.0F, 0.5F)),
                                         0.25F,
                                         glm::vec3(0.0F, 0.0F, 1.0F)),
                             glm::vec3(3.0F, -2.0F, 5.0F))})
        {
            Instance.UpdateTransferWeights(BuildSurfaceGPUTransferWeights(Geometry, Model));
            const std::array<float, 2> Initial{1.0F, 0.0F};
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute([&](VkCommandBuffer Commands)
                           { Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, Gravity); });
            std::array<float, 2> Result{};
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            const glm::vec3 Normal =
                glm::normalize(glm::transpose(glm::inverse(glm::mat3(Model))) * Geometry.GetTexels()[0].MesoNormal);
            const glm::vec3 SurfaceGravity = Gravity - Normal * glm::dot(Gravity, Normal);
            const glm::vec3 Edge = glm::mat3(Model) * glm::vec3(-1.0F, 0.0F, -0.2F);
            const float     Expected = 0.25F * std::abs(Edge.z) *
                                       std::max(glm::dot(glm::normalize(SurfaceGravity), glm::normalize(Edge)), 0.0F);
            Check(Expected > 0.0F && std::abs(Result[1] - Expected) < 1.0e-5F &&
                      std::abs(Result[0] + Result[1] - 1.0F) < 1.0e-5F,
                  "both passes should use Meso height and inverse-transpose Meso normal under instance scaling");
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute([&](VkCommandBuffer Commands)
                           { Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, Gravity, 1U << 4U); });
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            Check(std::abs(Result[0] - Initial[0]) < 1.0e-5F && std::abs(Result[1] - Initial[1]) < 1.0e-5F,
                  "macro direction normal should suppress geometry flux on a horizontal base surface in both passes");
            Vulkan.Execute([&](VkCommandBuffer Commands)
                           { Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, Gravity); });
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            Check(std::abs(Result[1] - Expected) < 1.0e-5F,
                  "re-enabling Meso direction normal should restore geometry flux from the same initial state");
        }

        for (const auto& [Model, TestGravity, Flags] :
             {std::tuple{glm::mat4(1.0F), glm::vec3(0.0F), 0U},
              std::tuple{glm::scale(glm::mat4(1.0F), glm::vec3(0.0F)), Gravity, 0U},
              std::tuple{glm::mat4(1.0F), glm::vec3(std::numeric_limits<float>::quiet_NaN()), 0U},
              std::tuple{glm::mat4(1.0F), Gravity, 1U}})
        {
            const std::array<float, 2> Initial{1.0F, 0.0F};
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute([&](VkCommandBuffer Commands)
                           { Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, TestGravity, Flags); });
            Instance.GetStateBBuffer().Download(EventResult.data(), sizeof(EventResult));
            Check(std::abs(EventResult[0] - 1.0F) < 1.0e-5F && EventResult[1] == 0.0F,
                  "CPU geometry constants must suppress singular transforms, invalid gravity and disabled geometry");
        }
    }

#pragma endregion

// ============================================================
// 13. TestSourceGeometryChannelReuse
// ============================================================
#pragma region "13_TestSourceGeometryChannelReuse"

    void TestSourceGeometryChannelReuse(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Static{}, Flow{}, FastFlow{};
        Flow.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        FastFlow.GeometryTransferFactor = 2.0F / BaseGeometryTransferRate;
        FastFlow.StateCapacity = 2.0F;
        Profile.States.emplace("a_static", Static);
        Profile.States.emplace("b_flow", Flow);
        Profile.States.emplace("c_flow", FastFlow);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(Table);
        const auto                                                   StaticChannel = Registry.GetStateId("a_static");
        const auto                                                   FlowChannel = Registry.GetStateId("b_flow");
        const auto                                                   FastChannel = Registry.GetStateId("c_flow");
        FSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});

        for (std::size_t Index = 0; Index < 2; ++Index)
        {
            auto& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {1.0F - static_cast<float>(Index), 0.0F, 0.0F};
            Texel.MesoNormal = glm::normalize(glm::vec3(-1.0F, 0.0F, 1.0F));
            Texel.bHasMesoNormal = true;
            Texel.Geometry.MesoVirtualHeight = Index == 0 ? 0.2F : 0.0F;
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - Index);
        }

        Geometry.SetProfileMap({0, 0});
        SurfaceState::FSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            3,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        std::array<float, 6>                           Initial{};
        Initial[StaticChannel] = 0.7F;
        Initial[FlowChannel] = 0.5F;
        Initial[FastChannel] = 1.0F;
        Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            { Solver.RecordStep(Commands, Descriptors, true, 2, 3, 0.25F, glm::mat4(1.0F), glm::vec3(0, 0, -1)); });
        std::array<float, 6> Result{};
        Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
        const glm::vec3 Normal = Geometry.GetTexels()[0].MesoNormal;
        const glm::vec3 Gravity(0, 0, -1);
        const glm::vec3 ProjectedGravity = Gravity - Normal * glm::dot(Gravity, Normal);
        const float     Expected =
            0.25F * 0.2F *
            std::max(glm::dot(glm::normalize(ProjectedGravity), glm::normalize(glm::vec3(-1, 0, -0.2F))), 0.0F);
        Check(std::abs(Result[StaticChannel] - 0.7F) < 1.0e-5F && Result[3 + StaticChannel] == 0.0F,
              "a geometry-disabled channel before active channels must retain its independent state");
        Check(std::abs(Result[3 + FlowChannel] - 0.5F * Expected) < 1.0e-5F &&
                  std::abs(Result[3 + FastChannel] - Expected) < 1.0e-5F &&
                  std::abs(Result[FlowChannel] + Result[3 + FlowChannel] - 0.5F) < 1.0e-5F &&
                  std::abs(Result[FastChannel] + Result[3 + FastChannel] - 1.0F) < 1.0e-5F,
              "shared source geometry must preserve distinct Registry channel rates, capacities and conservation");
    }

#pragma endregion

// ============================================================
// 14. TestTransferWeightSolver
// ============================================================
#pragma region "14_TestTransferWeightSolver"

    void TestTransferWeightSolver(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        SurfaceState::FSurfaceResponseProfileData ProfileA;
        SurfaceState::FSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 1.0F;
        ProfileA.States.emplace("wetness", Parameters);
        SurfaceState::FSurfaceResponseProfileData                    ProfileB = ProfileA;
        SurfaceState::FSurfaceResponseProfileData                    ProfileWithoutWetness;
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{
            ProfileA, ProfileB, ProfileWithoutWetness};
        const SurfaceState::FSurfaceStateRegistry Registry(ProfileTable);

        FSharedSurfaceGeometryData     Geometry({{0, {3, 1}}});
        const std::array<glm::vec3, 3> Positions{
            glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(4.0F, 0.0F, 0.0F), glm::vec3(1.0F, 0.0F, 0.0F)};
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = Positions[Index];
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
        }

        Geometry.GetTexels()[0].NeighborIndices[0] = 1;
        Geometry.GetTexels()[0].NeighborIndices[1] = 2;
        Geometry.GetTexels()[1].NeighborIndices[0] = 0;
        Geometry.GetTexels()[2].NeighborIndices[0] = 0;
        Geometry.SetProfileMap({0, 1, 2});

        const std::vector<float> CachedWeights = BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F));
        const float              ExpectedLongEdgeWeight = (3.25F / 4.0F) * 0.5F;
        Check(std::abs(CachedWeights[0] - ExpectedLongEdgeWeight) < 1.0e-5F &&
                  std::abs(CachedWeights[SurfaceNeighborCount] - ExpectedLongEdgeWeight) < 1.0e-5F,
              "directed cache entries should preserve the symmetric edge weight in both directions");

        SurfaceState::FSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            3,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        const std::array<float, 3> InitialState{1.0F, 0.0F, 0.0F};
        Instance.GetStateABuffer().Upload(InitialState.data(), sizeof(InitialState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(
                    CommandBuffer, Descriptors, true, 3, 1, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F, -1.0F, 0.0F));
            });

        std::array<float, 3> Result{};
        Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));

        // dRef = ((4 + 1) / 2 + 4) / 2 = 3.25, so the long edge weight is 3.25 / 4.
        // The Profile boundary halves that flux; the unsupported third Profile receives none.
        const float ExpectedTransfer = 0.25F * (3.25F / 4.0F) * 0.5F;
        Check(
            std::abs(Result[0] - (1.0F - ExpectedTransfer)) < 1.0e-4F &&
                std::abs(Result[1] - ExpectedTransfer) < 1.0e-4F && std::abs(Result[2]) < 1.0e-5F,
            "distance and Profile boundary weights should scale flux and unsupported channels should remain unchanged");

        SurfaceState::FSurfaceResponseProfileData NormalProfile;
        NormalProfile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> NormalProfileTable{NormalProfile};
        const SurfaceState::FSurfaceStateRegistry                    NormalRegistry(NormalProfileTable);
        FSharedSurfaceGeometryData                                   NormalGeometry({{0, {2, 1}}});

        for (std::size_t Index = 0; Index < NormalGeometry.GetTexelCount(); ++Index)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = NormalGeometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<FLocalTexelIndex>(1U - Index);
        }

        NormalGeometry.GetTexels()[0].TransferNormal = {0.0F, 0.0F, 1.0F};
        NormalGeometry.GetTexels()[1].TransferNormal = {std::sqrt(0.75F), 0.0F, 0.5F};
        NormalGeometry.GetTexels()[0].bHasTransferNormal = true;
        NormalGeometry.GetTexels()[1].bHasTransferNormal = true;
        NormalGeometry.SetProfileMap({0, 0});

        const std::vector<float> MappedNormalWeights = BuildSurfaceGPUTransferWeights(NormalGeometry, glm::mat4(1.0F));
        Check(std::abs(MappedNormalWeights[0] - 0.5F) < 1.0e-5F &&
                  std::abs(MappedNormalWeights[SurfaceNeighborCount] - 0.5F) < 1.0e-5F,
              "precomputed Normal Map transfer normals should drive the cached NormalWeight");
        const glm::mat4          NonUniformScale = glm::scale(glm::mat4(1.0F), glm::vec3(2.0F, 1.0F, 1.0F));
        const std::vector<float> ScaledMappedWeights = BuildSurfaceGPUTransferWeights(NormalGeometry, NonUniformScale);
        Check(std::abs(ScaledMappedWeights[0] - (1.0F / std::sqrt(1.75F))) < 1.0e-5F,
              "Normal Map transfer normals should use the instance inverse-transpose under non-uniform scale");
        const std::vector<float> NormalWeightDisabled =
            BuildSurfaceGPUTransferWeights(NormalGeometry, glm::mat4(1.0F), nullptr, false);
        Check(std::abs(NormalWeightDisabled[0] - 1.0F) < 1.0e-5F,
              "disabling the debug NormalWeight contribution should restore a neutral value");

        FSharedSurfaceGeometryData FallbackGeometry = NormalGeometry;

        for (SurfaceState::FSurfaceTexelGeometry& Texel : FallbackGeometry.GetTexels())
        {
            Texel.bHasTransferNormal = false;
        }

        FallbackGeometry.GetTexels()[1].Normal = {std::sqrt(0.75F), 0.0F, 0.5F};
        const std::vector<float> GeometricFallbackWeights =
            BuildSurfaceGPUTransferWeights(FallbackGeometry, glm::mat4(1.0F));
        Check(std::abs(GeometricFallbackWeights[0] - 0.5F) < 1.0e-5F &&
                  std::abs(GeometricFallbackWeights[SurfaceNeighborCount] - 0.5F) < 1.0e-5F,
              "missing Normal Map transfer normals should fall back to geometric normals");

        SurfaceState::FSurfaceSharedGeometryGPUResources NormalSharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), NormalGeometry);
        SurfaceState::FSurfaceProfileGPUResources NormalProfiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), NormalProfileTable, NormalRegistry);
        SurfaceState::FSurfaceInstanceGPUResources NormalInstance(
            Vulkan.GetPhysicalDevice(),
            Vulkan.GetDevice(),
            2,
            1,
            BuildSurfaceGPUTransferWeights(NormalGeometry, glm::mat4(1.0F)));
        SurfaceState::FSurfaceStateDescriptorResources NormalDescriptors(
            Vulkan.GetDevice(), NormalSharedGeometry, NormalProfiles, NormalInstance);
        SurfaceState::FSurfaceStateSolver NormalSolver(Vulkan.GetDevice(), NormalDescriptors.GetLayout());

        const std::array<float, 2> NormalInitialState{1.0F, 0.0F};
        NormalInstance.GetStateABuffer().Upload(NormalInitialState.data(), sizeof(NormalInitialState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                NormalSolver.RecordStep(
                    CommandBuffer, NormalDescriptors, true, 2, 1, 0.25F, glm::mat4(1.0F), glm::vec3(0.0F, -1.0F, 0.0F));
            });

        std::array<float, 2> NormalResult{};
        NormalInstance.GetStateBBuffer().Download(NormalResult.data(), sizeof(NormalResult));
        Check(std::abs(NormalResult[0] - 0.875F) < 1.0e-4F && std::abs(NormalResult[1] - 0.125F) < 1.0e-4F,
              "GPU solver flux should use the Normal Map-derived NormalWeight from the cache");
    }

#pragma endregion

// ============================================================
// 15. TestStateDrivenGeometryUpdate
// ============================================================
#pragma region "15_TestStateDrivenGeometryUpdate"

    void TestStateDrivenGeometryUpdate(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::FSurfaceResponseProfileData Profile;
        SurfaceState::FSurfaceStateParameters     Layer{};
        Layer.StateCapacity = 1.0F;
        Layer.AccumulationFactor = 10.0F;
        Layer.CavityFillFactor = 0.0F;
        Layer.GeometryTransferFactor = 1.0F;
        Profile.States.emplace("mud", Layer);
        SurfaceState::FSurfaceStateParameters FilmLayer = Layer;
        FilmLayer.ThicknessPerAmount = 0.002F;
        Profile.States.emplace("waterfilm", FilmLayer);
        const std::vector<SurfaceState::FSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::FSurfaceStateRegistry                    Registry(ProfileTable);
        const SurfaceState::FStateId                                 Mud = Registry.GetStateId("mud");
        const SurfaceState::FStateId                                 WaterFilm = Registry.GetStateId("waterfilm");
        Check(Mud != SurfaceState::InvalidStateId && WaterFilm != SurfaceState::InvalidStateId && Mud != WaterFilm,
              "geometry update fixture should resolve two independent accumulation states");

        FSharedSurfaceGeometryData     Geometry({{0, {3, 1}}});
        const std::array<glm::vec3, 3> Positions{
            glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)};
        const std::array<std::array<FLocalTexelIndex, 2>, 3> Neighbors{{{{1, 2}}, {{0, 2}}, {{0, 1}}}};

        for (std::size_t TexelIndex = 0; TexelIndex < Geometry.GetTexelCount(); ++TexelIndex)
        {
            SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[TexelIndex];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = Positions[TexelIndex];
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = Neighbors[TexelIndex][0];
            Texel.NeighborIndices[1] = Neighbors[TexelIndex][1];
        }

        Geometry.SetProfileMap({0, 0, 0});

        SurfaceState::FSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::FSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        const auto StaticWeights = BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F));
        SurfaceState::FSurfaceInstanceGPUResources Instance(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 3, 2, StaticWeights);
        SurfaceState::FSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::FSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        std::array<float, 6> InitialState{};
        InitialState[Mud] = 0.5F;
        InitialState[WaterFilm] = 0.5F;
        Instance.GetStateABuffer().Upload(InitialState.data(), sizeof(InitialState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(
                    CommandBuffer, Descriptors, true, 3, 2, 0.01F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F), 0U);
            });
        std::vector<float> WeightsWithGeometryUpdateOff(StaticWeights.size(), 0.0F);
        Instance.GetTransferWeightBuffer().Download(
            WeightsWithGeometryUpdateOff.data(),
            static_cast<VkDeviceSize>(WeightsWithGeometryUpdateOff.size() * sizeof(float)));
        std::array<SurfaceState::FSurfaceGPUVec4, 6> GeometryWithGeometryUpdateOff{};
        Instance.GetDynamicGeometryBuffer().Download(GeometryWithGeometryUpdateOff.data(),
                                                     sizeof(GeometryWithGeometryUpdateOff));
        Check(std::equal(StaticWeights.begin(),
                         StaticWeights.end(),
                         WeightsWithGeometryUpdateOff.begin(),
                         [](float A, float B) { return std::abs(A - B) < 1.0e-6F; }) &&
                  std::abs(GeometryWithGeometryUpdateOff[0].Z) < 1.0e-7F,
              "State-Driven Geometry가 꺼진 상태 should retain the static transfer cache and skip dynamic geometry "
              "prepasses");
        std::array<float, 6> StateWithGeometryUpdateOff{};
        Instance.GetStateBBuffer().Download(StateWithGeometryUpdateOff.data(), sizeof(StateWithGeometryUpdateOff));
        Check(std::equal(InitialState.begin(),
                         InitialState.end(),
                         StateWithGeometryUpdateOff.begin(),
                         [](float A, float B) { return std::abs(A - B) < 1.0e-6F; }),
              "the static flat geometry should produce no geometry-driven movement in the 형상 갱신 OFF run");

        std::uint32_t Flags =
            SurfaceState::SurfaceSolverStateDrivenGeometryUpdateFlag | SurfaceState::SurfaceSolverDistanceWeightFlag |
            SurfaceState::SurfaceSolverNormalWeightFlag | SurfaceState::SurfaceSolverProfileBoundaryWeightFlag;
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  false,
                                  3,
                                  2,
                                  0.0F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F),
                                  Flags | SurfaceSolverForceFullGeometryFlag);
            });

        std::array<SurfaceState::FSurfaceGPUVec4, 6> DynamicGeometry{};
        Instance.GetDynamicGeometryBuffer().Download(DynamicGeometry.data(), sizeof(DynamicGeometry));
        std::array<float, 3> CachedHeights{};
        Instance.GetAccumulationHeightBuffer().Download(CachedHeights.data(), sizeof(CachedHeights));
        std::vector<float> DynamicWeights(StaticWeights.size(), 0.0F);
        Instance.GetTransferWeightBuffer().Download(DynamicWeights.data(),
                                                    static_cast<VkDeviceSize>(DynamicWeights.size() * sizeof(float)));
        std::array<float, 6> ResultingState{};
        Instance.GetStateABuffer().Download(ResultingState.data(), sizeof(ResultingState));

        Check(std::abs(DynamicGeometry[0].Z - 0.06F) < 1.0e-5F,
              "updated geometry should add State-specific Profile thickness from all supported accumulation states");
        Check(std::abs(CachedHeights[0] - 0.06F) < 1.0e-5F &&
                  std::abs(CachedHeights[0] - DynamicGeometry[0].Z) < 1.0e-6F && std::abs(CachedHeights[1]) < 1.0e-6F &&
                  std::abs(CachedHeights[2]) < 1.0e-6F,
              "geometry update height pass should cache one combined height per texel for geometry and solver reads");
        Check(DynamicGeometry[1].X > 0.0F && DynamicGeometry[1].Y > 0.0F && DynamicGeometry[1].Z > 0.99F,
              "updated geometry should rebuild the local normal from the accumulated height gradient");
        const float Edge01 = std::sqrt(1.0F + 0.06F * 0.06F);
        const float ExpectedMean0 = Edge01;
        const float ExpectedMean1 = 0.5F * (Edge01 + std::sqrt(2.0F));
        Check(std::abs(DynamicGeometry[0].W - ExpectedMean0) < 1.0e-5F &&
                  std::abs(DynamicGeometry[2].W - ExpectedMean1) < 1.0e-5F,
              "updated geometry should cache each texel's displaced mean neighbor distance");
        const float NormalDot01 =
            std::clamp(DynamicGeometry[1].X * DynamicGeometry[3].X + DynamicGeometry[1].Y * DynamicGeometry[3].Y +
                           DynamicGeometry[1].Z * DynamicGeometry[3].Z,
                       0.0F,
                       1.0F);
        const float ExpectedWeight01 =
            std::clamp(0.5F * (ExpectedMean0 + ExpectedMean1) / Edge01, 0.0F, 1.0F) * NormalDot01;
        Check(std::abs(DynamicWeights[0] - ExpectedWeight01) < 1.0e-5F,
              "updated geometry transfer weight should use the cached mean without changing the edge formula");
        Check(std::abs(DynamicWeights[9] - StaticWeights[9]) > 1.0e-4F,
              "geometry update should rebuild edge weights using displaced distances and updated normals");
        // Bootstrap derives geometry without transport; the next step consumes it.
        Vulkan.Execute(
            [&](VkCommandBuffer Command)
            {
                Solver.RecordStep(
                    Command, Descriptors, true, 3, 2, 0.0001F, glm::mat4(1.0F), glm::vec3(0, 0, -1), Flags);
            });
        Instance.GetStateBBuffer().Download(ResultingState.data(), sizeof(ResultingState));
        Instance.GetTransferWeightBuffer().Download(DynamicWeights.data(), DynamicWeights.size() * sizeof(float));
        const auto  Layout = GetSurfaceSparseMetadataLayout(3);
        const float InitialTotal = std::accumulate(InitialState.begin(), InitialState.end(), 0.0F);
        const float ResultTotal = std::accumulate(ResultingState.begin(), ResultingState.end(), 0.0F);
        Check(ResultingState[Mud] < InitialState[Mud] && ResultingState[WaterFilm] < InitialState[WaterFilm] &&
                  (ResultingState[4U + Mud] + ResultingState[4U + WaterFilm]) > 0.0F,
              "accumulation geometry should change subsequent GeometryDrive transport toward the downhill neighbor");
        Check(std::abs(InitialTotal - ResultTotal) < 1.0e-5F,
              "state-driven geometry update transport should conserve State in the no-decay fixture");

        // A zero-delta step over the just-derived current State must leave both
        // geometry passes untouched.
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  false,
                                  3,
                                  2,
                                  0.0F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F),
                                  Flags);
            });
        std::array<float, 3> StableHeightDirty{};
        Instance.GetAccumulationHeightBuffer().Download(
            StableHeightDirty.data(), sizeof(StableHeightDirty), sizeof(StableHeightDirty));
        std::array<std::uint32_t, 3> StableDispatch{};
        Instance.GetAccumulationHeightBuffer().Download(
            StableDispatch.data(), sizeof(StableDispatch), Layout.GeometryCommandWord * sizeof(std::uint32_t));
        std::vector<float> StableWeights(StaticWeights.size(), 0.0F);
        Instance.GetTransferWeightBuffer().Download(StableWeights.data(),
                                                    static_cast<VkDeviceSize>(StableWeights.size() * sizeof(float)));
        Check(std::all_of(StableHeightDirty.begin(), StableHeightDirty.end(), [](float V) { return V == 0.0F; }),
              "unchanged accumulation heights should skip geometry and transfer-weight rebuilds");
        Check(StableDispatch == std::array<std::uint32_t, 3>{0U, 1U, 1U},
              "unchanged geometry update should dispatch no geometry or transfer-weight workgroups");
        Check(std::equal(DynamicWeights.begin(), DynamicWeights.end(), StableWeights.begin()),
              "unchanged updated geometry should preserve the cached transfer weights");

        std::array<FSurfaceGPUVec4, 6> BeforeDirtyGeometry{};
        Instance.GetDynamicGeometryBuffer().Download(BeforeDirtyGeometry.data(), sizeof(BeforeDirtyGeometry));
        auto ChangedState = ResultingState;
        ChangedState[Mud] += 0.1F;
        Instance.GetStateBBuffer().Upload(ChangedState.data(), sizeof(ChangedState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  false,
                                  3,
                                  2,
                                  0.0F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F),
                                  Flags);
            });
        std::array<float, 3> ChangedHeightDirty{};
        Instance.GetAccumulationHeightBuffer().Download(
            ChangedHeightDirty.data(), sizeof(ChangedHeightDirty), sizeof(ChangedHeightDirty));
        std::array<std::uint32_t, 3> ChangedDispatch{};
        Instance.GetAccumulationHeightBuffer().Download(
            ChangedDispatch.data(), sizeof(ChangedDispatch), Layout.GeometryCommandWord * sizeof(std::uint32_t));
        std::array<FSurfaceGPUVec4, 6> AfterDirtyGeometry{};
        Instance.GetDynamicGeometryBuffer().Download(AfterDirtyGeometry.data(), sizeof(AfterDirtyGeometry));
        Check(ChangedHeightDirty[0] == 1.0F && ChangedHeightDirty[1] == 0.0F && ChangedHeightDirty[2] == 0.0F &&
                  std::abs(AfterDirtyGeometry[3].X - BeforeDirtyGeometry[3].X) > 1.0e-6F,
              "a changed texel should rebuild geometry across its neighbor halo");
        Check(ChangedDispatch == std::array<std::uint32_t, 3>{1U, 1U, 1U},
              "changed geometry update should dispatch geometry and transfer-weight workgroups");
    }

#pragma endregion

// ============================================================
// 16. TestSparseGeometryUpdateWeights
// ============================================================
#pragma region "16_TestSparseGeometryUpdateWeights"

    void TestSparseGeometryUpdateWeights(FVulkanTestDevice& Vulkan)
    {
        using namespace MDSS::SurfaceState;
        FSurfaceResponseProfileData Profile;
        FSurfaceStateParameters     Layer{};
        Layer.StateCapacity = 1.0F;
        Layer.AccumulationFactor = 1.0F;
        Layer.ThicknessPerAmount = 0.1F;
        Profile.States.emplace("mud", Layer);
        const std::vector<FSurfaceResponseProfileData> Profiles{Profile};
        const FSurfaceStateRegistry                    Registry(Profiles);
        constexpr std::uint32_t                        Texels = 129;
        FSharedSurfaceGeometryData                     Geometry({{0, {Texels, 1}}});

        for (std::uint32_t I = 0; I < Texels; ++I)
        {
            auto& T = Geometry.GetTexels()[I];
            T.Surface = 0;
            T.Triangle = 0;
            T.Position = {float(I), 0, 0};
            T.Normal = {0, 0, 1};
        }

        Geometry.GetTexels()[0].NeighborIndices[0] = 1;
        Geometry.GetTexels()[1].NeighborIndices[0] = 0;
        Geometry.GetTexels()[127].NeighborIndices[0] = 128;
        Geometry.GetTexels()[128].NeighborIndices[0] = 127;
        Geometry.SetProfileMap(std::vector<FSurfaceProfileIndex>(Texels, 0));
        FSurfaceSharedGeometryGPUResources Shared(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        FSurfaceProfileGPUResources  GPUProfiles(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Profiles, Registry);
        FSurfaceInstanceGPUResources Instance(Vulkan.GetPhysicalDevice(),
                                              Vulkan.GetDevice(),
                                              Texels,
                                              1,
                                              BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1)));
        FSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, GPUProfiles, Instance);
        FSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const auto Flags = SurfaceSolverStateDrivenGeometryUpdateFlag | SurfaceSolverDistanceWeightFlag |
                           SurfaceSolverNormalWeightFlag | SurfaceSolverSparseGeometryFlag;
        const auto Execute = [&](bool bCurrentAB, std::uint32_t Extra)
        {
            Vulkan.Execute(
                [&](VkCommandBuffer Command)
                {
                    Solver.RecordStep(Command,
                                      Descriptors,
                                      bCurrentAB,
                                      Texels,
                                      1,
                                      0,
                                      glm::mat4(1),
                                      glm::vec3(0, 0, -1),
                                      Flags | Extra);
                });
        };
        Execute(true, SurfaceSolverForceFullGeometryFlag);
        constexpr float Sentinel = 0.123F;
        const auto      FarOffset = 128U * SurfaceNeighborCount * sizeof(float);
        Instance.GetTransferWeightBuffer().Upload(&Sentinel, sizeof(Sentinel), FarOffset);
        std::vector<float> State(Texels, 0);
        State[0] = 0.5F;
        Instance.GetStateBBuffer().Upload(State.data(), State.size() * sizeof(float));
        Execute(false, 0);
        const auto                   Layout = GetSurfaceSparseMetadataLayout(Texels);
        std::array<std::uint32_t, 3> Dispatch{}, GroupFlags{};
        Instance.GetAccumulationHeightBuffer().Download(
            Dispatch.data(), sizeof(Dispatch), Layout.GeometryCommandWord * sizeof(std::uint32_t));
        Instance.GetAccumulationHeightBuffer().Download(
            GroupFlags.data(), sizeof(GroupFlags), Layout.GeometryFlagWord * sizeof(std::uint32_t));
        float FarWeight = 0;
        Instance.GetTransferWeightBuffer().Download(&FarWeight, sizeof(FarWeight), FarOffset);
        Check(Dispatch == std::array<std::uint32_t, 3>{1, 1, 1} && GroupFlags == std::array<std::uint32_t, 3>{1, 0, 0},
              "local geometry updates schedule the affected 64-texel workgroup, not distant groups");
        Check(FarWeight == Sentinel, "a transfer weight in another workgroup survives a local update");
        Execute(true, SurfaceSolverForceFullGeometryFlag);
        Instance.GetTransferWeightBuffer().Download(&FarWeight, sizeof(FarWeight), FarOffset);
        Check(std::abs(FarWeight - 1.0F) < 1.0e-5F, "force-full prebuild refreshes the distant cached weight");
        Instance.GetAccumulationHeightBuffer().Download(
            Dispatch.data(), sizeof(Dispatch), Layout.GeometryCommandWord * sizeof(std::uint32_t));
        Check(Dispatch == std::array<std::uint32_t, 3>{0, 1, 1},
              "an unchanged post-step skips the second geometry update after a force-full prebuild");
    }

    std::vector<float> RunReactions(FVulkanTestDevice&                                     V,
                                    const MDSS::SurfaceState::FSurfaceResponseProfileData& P,
                                    const std::vector<float>&                              Initial,
                                    const std::vector<float>&                              Areas,
                                    float                                                  Dt,
                                    unsigned                                               Steps = 1,
                                    std::uint32_t                                          Flags = 0,
                                    const std::vector<float>&                              Input = {},
                                    bool                                                   bChain = false,
                                    std::vector<float>*                                    Heights = nullptr,
                                    std::vector<float>*                                    Diagnostics = nullptr)
    {
        using namespace MDSS::SurfaceState;
        const FSurfaceStateRegistry Registry({P});
        const auto                  K = Registry.GetStateCount(), N = Areas.size();
        FSharedSurfaceGeometryData  G({{0, {static_cast<std::uint32_t>(N), 1}}});
        std::vector<float>          Weights(N * SurfaceNeighborCount, 0);

        for (std::size_t I = 0; I < N; ++I)
        {
            auto& T = G.GetTexels()[I];
            T.Surface = 0;
            T.Triangle = 0;
            T.Position = {float(I), 0, 0};
            T.Normal = {0, 0, 1};

            if (bChain && I + 1 < N)
            {
                T.NeighborIndices[0] = static_cast<FLocalTexelIndex>(I + 1);
                Weights[I * 8] = 1;
            }

            if (bChain && I > 0)
            {
                T.NeighborIndices[1] = static_cast<FLocalTexelIndex>(I - 1);
                Weights[I * 8 + 1] = 1;
            }
        }

        G.SetProfileMap(std::vector<FSurfaceProfileIndex>(N, 0));
        FSurfaceSharedGeometryGPUResources Shared(V.GetPhysicalDevice(), V.GetDevice(), G);
        FSurfaceProfileGPUResources        Profiles(V.GetPhysicalDevice(), V.GetDevice(), {P}, Registry);
        std::vector<float>                 WorldAreas = Areas;

        for (auto& A : WorldAreas)
            A *= SurfaceStateReferenceArea;
        FSurfaceInstanceGPUResources     Instance(V.GetPhysicalDevice(),
                                                  V.GetDevice(),
                                                  N,
                                                  K,
                                                  Weights,
                                                  {},
                                                  WorldAreas,
                                                  nullptr,
                                                  {},
                                                  Profiles.GetMaxReactionCount());
        FSurfaceStateDescriptorResources Descriptors(V.GetDevice(), Shared, Profiles, Instance);
        FSurfaceStateSolver              Solver(V.GetDevice(), Descriptors.GetLayout());
        Instance.GetStateABuffer().Upload(Initial.data(), Initial.size() * sizeof(float));

        if (!Input.empty())
            Instance.GetInputDeltaBuffer().Upload(Input.data(), Input.size() * sizeof(float));
        V.Execute(
            [&](VkCommandBuffer C)
            {
                for (unsigned S = 0; S < Steps; ++S)
                    Solver.RecordStep(C,
                                      Descriptors,
                                      S % 2 == 0,
                                      N,
                                      K,
                                      Dt,
                                      glm::mat4(1),
                                      glm::vec3(0, 0, -1),
                                      Flags | (S == 0 ? SurfaceSolverSeedPersistentActivityFlag : 0U));
            });
        std::vector<float> Out(Initial.size());
        (Steps % 2 ? Instance.GetStateBBuffer() : Instance.GetStateABuffer())
            .Download(Out.data(), Out.size() * sizeof(float));
        if (Heights)
        {
            Heights->resize(N);
            Instance.GetAccumulationHeightBuffer().Download(Heights->data(), N * sizeof(float));
        }

        if (Diagnostics)
        {
            const auto&        Scratch = Instance.GetReactionScratchBuffer();
            std::vector<float> Data(static_cast<std::size_t>(Scratch.GetSize() / sizeof(float)));
            Scratch.Download(Data.data(), Scratch.GetSize());
            Diagnostics->resize(N * 4U);
            const auto Stride = Data.size() / N;

            for (std::size_t T = 0; T < N; ++T)
                std::copy_n(Data.begin() + T * Stride + Stride - 4U, 4U, Diagnostics->begin() + T * 4U);
        }

        return Out;
    }

#pragma endregion

// ============================================================
// 17. TestLocalReactions
// ============================================================
#pragma region "17_TestLocalReactions"

    void TestLocalReactions(FVulkanTestDevice& V)
    {
        using namespace MDSS::SurfaceState;
        FSurfaceResponseProfileData P;
        P.States["ice"] = {};
        P.States["waterfilm"] = {};
        P.States["heat"] = {};
        P.States["ice"].AccumulationFactor = 1;
        P.States["ice"].StateCapacity = 20;
        P.States["ice"].ThicknessPerAmount = .01F;
        FSurfaceReaction Melt;
        Melt.ID = "melt";
        Melt.Consumes = {{"ice", 1}, {"heat", .25F}};
        Melt.Produces = {{"waterfilm", 1}};
        Melt.RateModel = EReactionRateModel::StateProportional;
        Melt.RateState = "ice";
        Melt.RateCoefficient = .4F;
        Melt.Modifiers = {{"heat", EReactionFunction::RampAbove, .5F, 1}};
        P.Reactions = {Melt};
        FSurfaceStateRegistry    R({P});
        const auto               H = R.GetStateId("heat"), I = R.GetStateId("ice"), W = R.GetStateId("waterfilm");
        const auto               K = R.GetStateCount();
        std::vector<float>       Initial(3 * K, 0);
        const std::vector<float> Areas{1, .25F, 4};

        for (unsigned T = 0; T < 3; ++T)
        {
            Initial[T * K + H] = 2 * Areas[T];
            Initial[T * K + I] = 10 * Areas[T];
        }

        std::vector<float> Heights;
        const auto         Out = RunReactions(V, P, Initial, Areas, .1F, 1, 0, {}, false, &Heights);

        for (unsigned T = 0; T < 3; ++T)
        {
            Check(Out[T * K + W] > 0 && Out[T * K + H] < Initial[T * K + H],
                  "melting produces water and consumes heat");
            Check(std::abs(Out[T * K + I] + Out[T * K + W] - 10 * Areas[T]) < 1e-4F,
                  "reaction preserves ice + water mass");
            Check(std::abs(Out[T * K + W] / Areas[T] - Out[W]) < 1e-4F, "threshold and scale both follow texel area");
            Check(std::abs(Heights[T] - Out[T * K + I] / Areas[T] * .01F) < 1e-4F,
                  "height derives from final reacted State");
        }

        const auto Half = RunReactions(V, P, Initial, Areas, .05F, 2);
        Check(std::abs(Half[W] - Out[W]) < 2e-4F, "reaction integration converges under timestep subdivision");
        auto Limited = Initial;
        Limited[H] = .01F;
        auto Q = P;
        Q.Reactions[0].Modifiers.clear();
        Q.Reactions[0].RateCoefficient = 100;
        const auto Low = RunReactions(V, Q, Limited, Areas, .5F);
        Check(Low[H] >= 0 && Low[W] <= .04001F, "multi-source reaction is limited by available heat");

        FSurfaceReaction Other = Melt;
        Other.ID = "other";
        Other.Consumes = {{"ice", 1}};
        Other.Produces = {{"heat", 1}};
        Other.Modifiers.clear();
        Other.RateModel = EReactionRateModel::Constant;
        Other.RateCoefficient = 7;
        Q = P;
        Q.Reactions[0].Consumes = {{"ice", 1}};
        Q.Reactions[0].Modifiers.clear();
        Q.Reactions[0].RateModel = EReactionRateModel::Constant;
        Q.Reactions[0].RateCoefficient = 8;
        Q.Reactions.push_back(Other);
        std::vector<float> Ten(K, 0);
        Ten[I] = 10;
        std::vector<float> Diagnostics;
        const auto         Competition = RunReactions(V, Q, Ten, {1}, 1, 1, 0, {}, false, nullptr, &Diagnostics);
        Check(Diagnostics[2] == 1 && Diagnostics[3] == 0,
              "stiff reaction fallback reports its exhausted integration budget without arithmetic failure");
        Check(std::abs(Competition[I]) < 1e-4F && std::abs(Competition[W] + Competition[H] - 10) < 2e-4F,
              "competing conversions never overspend their common source");
        Check(std::abs(Competition[W] / Competition[H] - 8.0F / 7.0F) < 1e-4F,
              "competing rules share consumption proportionally");
        std::reverse(Q.Reactions.begin(), Q.Reactions.end());
        const auto Reordered = RunReactions(V, Q, Ten, {1}, 1);

        for (std::size_t C = 0; C < K; ++C)
            Check(std::abs(Reordered[C] - Competition[C]) < 2e-4F, "reaction rule order does not change the result");

        Q = P;
        Q.Reactions[0].Consumes.clear();
        Q.Reactions[0].RateModel = EReactionRateModel::Constant;
        Q.Reactions[0].Modifiers.clear();
        Q.Reactions[0].RateCoefficient = 3;
        Q.States["waterfilm"].StateCapacity = .1F;
        const auto Generated = RunReactions(V,
                                            Q,
                                            std::vector<float>(K, 0),
                                            {1},
                                            .5F,
                                            3,
                                            SurfaceSolverSparseSolverFlag | SurfaceSolverActiveChannelMaskFlag |
                                                SurfaceSolverPerWorkgroupChannelMaskFlag);
        Check(std::abs(Generated[W] - 4.5F) < 2e-4F,
              "pure generation activates empty channels and exceeds Capacity without loss");

        Q = P;
        Q.Reactions[0].Consumes.clear();
        Q.Reactions[0].RateModel = EReactionRateModel::MassAction;
        Q.Reactions[0].Factors = {{"ice", 2, 1}, {"heat", 4, 1}};
        Q.Reactions[0].Modifiers.clear();
        Q.Reactions[0].RateCoefficient = 1;
        Ten.assign(K, 0);
        Ten[I] = 2;
        Ten[H] = 4;
        const auto MassAction = RunReactions(V, Q, Ten, {1}, .2F);
        Check(std::abs(MassAction[W] - .2F) < 1e-5F && MassAction[I] == 2 && MassAction[H] == 4,
              "mass-action factors drive a rule without being consumed");
        Q.Reactions[0].Modifiers = {{"heat", EReactionFunction::Inhibit, 0, 4}};
        const auto Inhibited = RunReactions(V, Q, Ten, {1}, .2F);
        Check(std::abs(Inhibited[W] - .1F) < 1e-5F, "inhibitor multiplies the reaction rate");

        for (const auto& [Function, Threshold, Scale, Expected] :
             {std::tuple{EReactionFunction::Above, 4.0F, 1.0F, .2F},
              std::tuple{EReactionFunction::Below, 3.0F, 1.0F, 0.0F},
              std::tuple{EReactionFunction::RampAbove, 3.0F, 2.0F, .1F},
              std::tuple{EReactionFunction::RampBelow, 5.0F, 2.0F, .1F},
              std::tuple{EReactionFunction::Linear, 0.0F, 4.0F, .2F}})
        {
            Q.Reactions[0].Modifiers = {{"heat", Function, Threshold, Scale}};
            const auto Value = RunReactions(V, Q, Ten, {1}, .2F);
            Check(std::abs(Value[W] - Expected) < 1e-5F,
                  "condition functions use the documented threshold and scale semantics");
        }

        Q = P;
        Q.Reactions[0].Mode = EReactionMode::InflowFraction;
        Q.Reactions[0].Modifiers.clear();
        Q.Reactions[0].Consumes = {{"waterfilm", 1}};
        Q.Reactions[0].Produces = {{"ice", 1}};
        Q.Reactions[0].InflowState = "waterfilm";
        Q.Reactions[0].Fraction = .25F;
        Ten.assign(K, 0);
        Ten[W] = 10;
        std::vector<float> Input(K, 0);
        Input[W] = 8;
        const auto Deposited = RunReactions(V, Q, Ten, {1}, .25F, 2, 0, Input);
        Check(std::abs(Deposited[W] - 16) < 1e-5F && std::abs(Deposited[I] - 2) < 1e-5F,
              "inflow conversion consumes fresh input once, independently of substeps and existing water");
        Q.Reactions[0].InflowSources = 2;
        Q.States["waterfilm"].SaturationTransferFactor = .5F;
        Ten.assign(2 * K, 0);
        Ten[W] = 4;
        const auto Arrived = RunReactions(V, Q, Ten, {1, 1}, .25F, 1, 0, {}, true);
        Check(std::abs(Arrived[K + I] - .125F) < 1e-5F && std::abs(Arrived[K + W] - .375F) < 1e-5F && Arrived[I] == 0,
              "only actual incoming flux deposits wetness at the receiving texel");

        Q = P;
        Q.Reactions.clear();
        Q.Transitions = {{"heat", "waterfilm", .7F, .2F}};
        Q.States["heat"].StateCapacity = 2;
        Ten.assign(K, 0);
        Ten[H] = 1.6F;
        const auto Legacy = RunReactions(V, Q, Ten, {1}, 1);
        Check(std::abs(Legacy[W] - .2F) < 1e-5F && Legacy[H] == Ten[H],
              "legacy transition preserves Saturation threshold and does not consume its driver");

        Q = P;
        Q.States["waterfilm"].SaturationTransferFactor = .2F;
        const unsigned     N = 130;
        std::vector<float> Many(N * K, 0);
        Many[63 * K + I] = 10;
        Many[63 * K + H] = 2;
        const auto Dense = RunReactions(V, Q, Many, std::vector<float>(N, 1), .025F, 5, 0, {}, true);
        const auto Sparse =
            RunReactions(V,
                         Q,
                         Many,
                         std::vector<float>(N, 1),
                         .025F,
                         5,
                         SurfaceSolverSparseSolverFlag | SurfaceSolverActiveChannelMaskFlag |
                             SurfaceSolverPerWorkgroupChannelMaskFlag | SurfaceSolverSparseAccumulationHeightFlag,
                         {},
                         true);
        for (std::size_t C = 0; C < Dense.size(); ++C)
            Check(std::abs(Dense[C] - Sparse[C]) < 1e-5F,
                  "sparse reactions include newly generated channels and cross-workgroup incoming");
        Check(Sparse[64 * K + W] > 0, "newly generated water participates in next-step transport");

        for (unsigned C = 0; C < 34; ++C)
            Q.States["extra_" + std::to_string(C)] = {};
        const FSurfaceStateRegistry Dynamic({Q});
        std::vector<float>          DynamicInitial(Dynamic.GetStateCount(), 0);
        DynamicInitial[Dynamic.GetStateId("ice")] = 10;
        DynamicInitial[Dynamic.GetStateId("heat")] = 2;
        const auto DynamicOut = RunReactions(V, Q, DynamicInitial, {1}, .1F);
        Check(DynamicOut[Dynamic.GetStateId("waterfilm")] > 0,
              "reaction storage supports runtime channel counts above 32");
    }

} // namespace

#pragma endregion

// ============================================================
// 18. Test Entry Point
// ============================================================
#pragma region "18_Test_Entry_Point"

int main(int argc, char** argv)
{
    try
    {
        if (argc > 1 && std::string(argv[1]) == "--meso-peak-only")
        {
            TestMesoPeakPacking();

            return FailureCount == 0 ? 0 : 1;
        }

        FVulkanTestDevice Vulkan;

        if (argc > 1 && std::string(argv[1]) == "--reactions-only")
        {
            TestLocalReactions(Vulkan);
            return FailureCount == 0 ? 0 : 1;
        }

        TestCompactScratchStorage(Vulkan);

        if (argc > 1 && std::string(argv[1]) == "--storage-and-mesh-only")
        {
            TestLazyMeshStorage(Vulkan);
            return FailureCount == 0 ? 0 : 1;
        }

        if (argc == 1 || std::string(argv[1]) != "--compact-storage-only")
            TestRetiredBufferReuse(Vulkan);
        if (argc == 1 || (std::string(argv[1]) != "--reuse-only" && std::string(argv[1]) != "--compact-storage-only"))
        {
            TestGPUResources(Vulkan);
            TestGPUSolver(Vulkan);
            TestLocalReactions(Vulkan);
            TestPerWorkgroupChannelMask(Vulkan);
            TestDirectionalCavityRetention(Vulkan);
            TestAreaAndMobility(Vulkan);
            TestCalibratedGeometrySpeed(Vulkan);
            TestGeometryDrivenSolver(Vulkan);
            TestMesoGeometryDrive(Vulkan);
            TestSourceGeometryChannelReuse(Vulkan);
            TestTransferWeightSolver(Vulkan);
            TestStateDrivenGeometryUpdate(Vulkan);
            TestSparseGeometryUpdateWeights(Vulkan);
        }
    }
    catch (const FVulkanUnavailable& Exception)
    {
        std::cout << "SKIP: " << Exception.what() << '\n';

        return 77;
    }
    catch (const std::exception& Exception)
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
