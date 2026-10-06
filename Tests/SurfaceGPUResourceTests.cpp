/**
 * @file SurfaceGPUResourceTests.cpp
 * @brief Vulkan resource layout, 초기화와 descriptor 연결을 검증한다.
 */

#include "SurfaceState/GPU/SurfaceGPUResources.h"
#include "SurfaceState/State/SurfaceStateSolver.h"
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

namespace
{
    int FailureCount = 0;

    class TVulkanUnavailable final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    void Check(bool Condition, const std::string& Message)
    {
        if (!Condition)
        {
            ++FailureCount;
            std::cerr << "FAIL: " << Message << '\n';
        }
    }

    class TVulkanTestDevice final
    {
    public:
        TVulkanTestDevice()
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
                throw TVulkanUnavailable("Could not create Vulkan instance for GPU resource tests.");
            }

            std::uint32_t PhysicalDeviceCount = 0;
            if (vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, nullptr) != VK_SUCCESS ||
                PhysicalDeviceCount == 0)
            {
                throw TVulkanUnavailable("No Vulkan physical device is available for GPU resource tests.");
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
                throw TVulkanUnavailable("No compute-capable Vulkan queue is available for GPU resource tests.");
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

        ~TVulkanTestDevice()
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

        TVulkanTestDevice(const TVulkanTestDevice&) = delete;
        TVulkanTestDevice& operator=(const TVulkanTestDevice&) = delete;

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

    MDSS::SurfaceState::TSharedSurfaceGeometryData BuildGeometry()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}});
        for (std::size_t Index = 0; Index < Geometry.GetTexels().size(); ++Index)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 2.0F, 3.0F};
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>((Index + 1U) % Geometry.GetTexelCount());
        }
        Geometry.SetProfileMap({0, 0, 0, 0});
        return Geometry;
    }

    void CheckZeroBuffer(const MDSS::GPU::TGPUBuffer& Buffer, std::size_t ScalarCount, const std::string& Name)
    {
        std::vector<float> Values(ScalarCount, -1.0F);
        Buffer.Download(Values.data(), static_cast<VkDeviceSize>(Values.size() * sizeof(float)));
        for (float Value : Values)
        {
            Check(Value == 0.0F, Name + " should start with zero values");
        }
    }

    void TestGPUResources(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        VkPhysicalDeviceProperties DeviceProperties{};
        vkGetPhysicalDeviceProperties(Vulkan.GetPhysicalDevice(), &DeviceProperties);
        const std::uint32_t RequiredStorageBufferBindings =
            static_cast<std::uint32_t>(SurfaceState::TSurfaceGPUDescriptorBinding::Count);
        if (DeviceProperties.limits.maxDescriptorSetStorageBuffers < RequiredStorageBufferBindings ||
            DeviceProperties.limits.maxPerStageDescriptorStorageBuffers < RequiredStorageBufferBindings)
        {
            throw TVulkanUnavailable("Vulkan device does not support the Surface descriptor storage-buffer count.");
        }

        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 3.0F;
        Parameters.InputFactor = 0.75F;
        Parameters.SaturationTransferFactor = 0.2F;
        Parameters.GeometryTransferFactor = 0.5F;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);
        TSharedSurfaceGeometryData                                   Geometry = BuildGeometry();

        SurfaceState::TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        const std::vector<float> GeometryWeights = BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F));
        SurfaceState::TSurfaceInstanceGPUResources InstanceA(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, GeometryWeights);
        SurfaceState::TSurfaceInstanceGPUResources InstanceB(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1, GeometryWeights);
        SurfaceState::TSurfaceStateDescriptorResources DescriptorsA(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceA);
        SurfaceState::TSurfaceStateDescriptorResources DescriptorsB(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceB);

        Check(SharedGeometry.GetTexelCount() == 4, "shared Geometry should retain four texels");
        Check(SharedGeometry.GetNeighborIndexBuffer().GetSize() ==
                  4U * sizeof(SurfaceState::TSurfaceGPUNeighborIndices),
              "neighbor buffer should use the expected per-texel stride");
        Check(DescriptorsA.GetABSet() != VK_NULL_HANDLE && DescriptorsA.GetBASet() != VK_NULL_HANDLE,
              "both A/B descriptor sets should be allocated");
        Check(DescriptorsB.GetABSet() != VK_NULL_HANDLE && DescriptorsB.GetBASet() != VK_NULL_HANDLE,
              "each instance should own its descriptor sets");

        const auto Bound = [](const SurfaceState::TSurfaceStateDescriptorResources& Descriptors,
                              SurfaceState::TSurfaceGPUDescriptorBinding            Binding,
                              bool bAB) { return Descriptors.GetBoundBufferHandle(Binding, bAB); };
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  SharedGeometry.GetTexelSurfaceIndexBuffer().GetHandle(),
              "descriptor should bind the shared Geometry buffer");
        Check(Bound(DescriptorsB, SurfaceState::TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true),
              "instances should bind the same shared Geometry buffer");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "AB descriptor should read State A");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::NextState, true) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "AB descriptor should write State B");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, false) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "BA descriptor should read State B");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::NextState, false) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "BA descriptor should write State A");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::TransferWeights, true) ==
                      InstanceA.GetTransferWeightBuffer().GetHandle() &&
                  Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::RawOutgoing, true) ==
                      InstanceA.GetRawOutgoingBuffer().GetHandle(),
              "instance descriptors should bind the TransferWeight and RawOutgoing caches");
        Check(InstanceA.GetStateABuffer().GetHandle() != InstanceB.GetStateABuffer().GetHandle() &&
                  InstanceA.GetStateBBuffer().GetHandle() != InstanceB.GetStateBBuffer().GetHandle(),
              "separate instances should own separate State buffers");
        Check(Bound(DescriptorsB, SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true) ==
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
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::ReverseNeighborDirectionIndices, true) ==
                      SharedGeometry.GetReverseNeighborDirectionIndexBuffer().GetHandle() &&
                  Bound(DescriptorsB,
                        SurfaceState::TSurfaceGPUDescriptorBinding::ReverseNeighborDirectionIndices,
                        false) ==
                      SharedGeometry.GetReverseNeighborDirectionIndexBuffer().GetHandle(),
              "reverse direction indices should be shared with Geometry across instances and AB/BA sets");
        Check(Bound(DescriptorsA, SurfaceState::TSurfaceGPUDescriptorBinding::WorldTexelAreas, true) ==
                      InstanceA.GetWorldTexelAreaBuffer().GetHandle(),
              "WorldTexelAreas should follow the retained reverse-direction binding without a RawFlux binding");

        std::array<SurfaceState::TSurfaceGPUVec4, 4> UploadedPositions{};
        SharedGeometry.GetPositionBuffer().Download(UploadedPositions.data(), sizeof(UploadedPositions));
        Check(UploadedPositions[2].X == 2.0F && UploadedPositions[2].Y == 2.0F && UploadedPositions[2].Z == 3.0F,
              "position upload should preserve texel-major values");

        std::array<SurfaceState::TSurfaceGPUNeighborIndices, 4> UploadedNeighbors{};
        SharedGeometry.GetNeighborIndexBuffer().Download(UploadedNeighbors.data(), sizeof(UploadedNeighbors));
        Check(UploadedNeighbors[2].Indices[0] == 3U, "neighbor upload should preserve the local neighbor index");

        std::array<SurfaceState::TSurfaceGPUProfileParameters, 1> UploadedProfiles{};
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

    void TestDirectionalCavityRetention(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceStateParameters Parameters{};
        Parameters.SaturationTransferFactor = 1.0F;
        Parameters.CavityTransportRetentionFactor = 1.0F;
        SurfaceState::TSurfaceResponseProfileData Profile;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(Table);
        TSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});
        for (std::size_t I = 0; I < 2; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = Texel.Triangle = Texel.Chart = 0;
            Texel.Position = {float(I), 0.0F, 0.0F};
            Texel.Normal = {0.0F, 0.0F, 1.0F};
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - I);
        }
        Geometry.GetTexels()[0].Geometry.ConcavityWeight = 1.0F;
        Geometry.SetProfileMap({0, 0});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
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

    void TestGPUSolver(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 1.0F;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);

        TSharedSurfaceGeometryData Geometry({{0, {2, 1}}});
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - Index);
        }
        Geometry.SetProfileMap({0, 0});

        SurfaceState::TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

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
            const GPU::TGPUBuffer& StateBuffer =
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

    void TestAreaAndMobility(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceStateParameters Parameters{};
        Parameters.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        // Nonzero Saturation factor verifies that its debug flag is independent of Geometry mobility.
        Parameters.SaturationTransferFactor = 1.0F;
        SurfaceState::TSurfaceResponseProfileData Profile;
        Profile.States.emplace("test_flow", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(Table);
        TSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});
        for (std::size_t I = 0; I < 2; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {0, 0, 1.0F - float(I)};
            Texel.Normal = {1, 0, 0};
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1 - I);
        }
        Geometry.SetProfileMap({0, 0});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::TSurfaceInstanceGPUResources Instance(
            Vulkan.GetPhysicalDevice(),
            Vulkan.GetDevice(),
            2,
            1,
            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1)),
            {},
            {2 * SurfaceStateReferenceArea, 4 * SurfaceStateReferenceArea});
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
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
            {
                Solver.RecordStep(
                    Commands, Descriptors, true, 2, 1, 0.1F, glm::mat4(1), glm::vec3(0, 0, -1));
            });
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
            const std::array<SurfaceState::TSurfaceGPUVec4, 2> Positions{{{0, 0, H, 0}, {0, 0, 0, 0}}};
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

    void TestCalibratedGeometrySpeed(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.GeometryTransferFactor = 0.5F;
        Profile.States.emplace("flow", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(Table);
        constexpr std::size_t Width = 5, Count = Width * Width, Source = Count / 2;
        constexpr float       Dt = 0.001F;
        for (float Resolution : {128.0F, 256.0F, 512.0F})
        {
            const float                H = 1.0F / Resolution;
            TSharedSurfaceGeometryData Geometry({{0, {Width, Width}}});
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
                                Texel.NeighborIndices[DirectionIndex] = static_cast<TLocalTexelIndex>((Y + DY) * Width + X + DX);
                            ++DirectionIndex;
                        }
                }
            Geometry.SetProfileMap(std::vector<SurfaceState::TSurfaceProfileIndex>(Count, 0));
            SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
                Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
            SurfaceState::TSurfaceProfileGPUResources Profiles(
                Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
            SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                                Vulkan.GetDevice(),
                                                                Count,
                                                                1,
                                                                BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1)),
                                                                    {},
                                                                BuildSurfaceGPUWorldTexelAreas(Geometry, glm::mat4(1)));
            SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
            SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
            std::array<float, Count>                       Initial{}, Result{};
            const double Mass = 2.0 * H * H / SurfaceStateReferenceArea; // Saturation=2, deliberately above 1.
            Initial[Source] = static_cast<float>(Mass);
            Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
                {
                    Solver.RecordStep(
                        Commands, Descriptors, true, Count, 1, Dt, glm::mat4(1), glm::vec3(0, 0, -1));
                });
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

    void TestGeometryDrivenSolver(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 0.0F;
        Parameters.GeometryTransferFactor = 8.0F / BaseGeometryTransferRate;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);

        TSharedSurfaceGeometryData Geometry({{0, {2, 1}}});
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = Index == 0 ? glm::vec3(1.0F, 0.0F, 0.0F) : glm::vec3(0.0F);
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - Index);
        }
        Geometry.SetProfileMap({0, 0});

        SurfaceState::TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        const glm::mat4 ModelMatrix = glm::rotate(glm::mat4(1.0F), glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
        auto            Run = [&](std::array<float, 2> InitialState, glm::vec3 Gravity, float DeltaTime)
        {
            Instance.GetStateABuffer().Upload(InitialState.data(), sizeof(InitialState));
            Vulkan.Execute(
                [&](VkCommandBuffer CommandBuffer)
                {
                    Solver.RecordStep(
                        CommandBuffer, Descriptors, true, 2, 1, DeltaTime, ModelMatrix, Gravity);
                });

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

    void TestMesoGeometryDrive(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        Profile.States.emplace("deposit", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);
        TSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});
        for (std::size_t Index = 0; Index < 2; ++Index)
        {
            auto& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {1.0F - static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = {0.0F, 0.0F, 1.0F};
            Texel.MesoNormal = glm::normalize(glm::vec3(-1.0F, 0.0F, 1.0F));
            Texel.HasMesoNormal = true;
            Texel.Geometry.MesoVirtualHeight = Index == 0 ? 0.2F : 0.0F;
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - Index);
        }
        Geometry.SetProfileMap({0, 0});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const std::array<float, 2>                     EmptyState{0.0F, 0.0F};
        const std::array<float, 2>                     EventInput{0.125F, 0.0F};
        Instance.GetStateABuffer().Upload(EmptyState.data(), sizeof(EmptyState));
        Instance.GetInputDeltaBuffer().Upload(EventInput.data(), sizeof(EventInput));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(Commands,
                                  Descriptors,
                                  true,
                                  2,
                                  1,
                                  0.25F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F));
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
                Solver.RecordStep(Commands,
                                  Descriptors,
                                  false,
                                  2,
                                  1,
                                  0.25F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F));
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
                Solver.RecordStep(Commands,
                                  Descriptors,
                                  true,
                                  2,
                                  1,
                                  0.25F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F));
            });
        Instance.GetStateBBuffer().Download(EventResult.data(), sizeof(EventResult));
        Check(std::abs(EventResult[0] - DepletedInput[0]) < 1.0e-5F &&
                  std::abs(EventResult[1] - DepletedInput[1]) < 1.0e-5F,
              "decay-depleted sources must skip outgoing but still apply external input to both texels");
        Parameters.DecayRate = 0.0F;
        Profiles.UpdateParameters(0, 0, Parameters);

        Check(SurfaceState::TSurfaceSolverDebugSettings{}.IsEnabled(
                  SurfaceState::TSurfaceSolverTerm::MesoDirectionNormal),
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
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
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
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
                {
                    Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, Gravity, 1U << 4U);
                });
            Instance.GetStateBBuffer().Download(Result.data(), sizeof(Result));
            Check(std::abs(Result[0] - Initial[0]) < 1.0e-5F && std::abs(Result[1] - Initial[1]) < 1.0e-5F,
                  "macro direction normal should suppress geometry flux on a horizontal base surface in both passes");
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
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
            Vulkan.Execute(
                [&](VkCommandBuffer Commands)
                {
                    Solver.RecordStep(Commands, Descriptors, true, 2, 1, 0.25F, Model, TestGravity, Flags);
                });
            Instance.GetStateBBuffer().Download(EventResult.data(), sizeof(EventResult));
            Check(std::abs(EventResult[0] - 1.0F) < 1.0e-5F && EventResult[1] == 0.0F,
                  "CPU geometry constants must suppress singular transforms, invalid gravity and disabled geometry");
        }
    }

    void TestSourceGeometryChannelReuse(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Static{}, Flow{}, FastFlow{};
        Flow.GeometryTransferFactor = 1.0F / BaseGeometryTransferRate;
        FastFlow.GeometryTransferFactor = 2.0F / BaseGeometryTransferRate;
        FastFlow.StateCapacity = 2.0F;
        Profile.States.emplace("a_static", Static);
        Profile.States.emplace("b_flow", Flow);
        Profile.States.emplace("c_flow", FastFlow);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> Table{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(Table);
        const auto                                                   StaticChannel = Registry.GetStateId("a_static");
        const auto                                                   FlowChannel = Registry.GetStateId("b_flow");
        const auto                                                   FastChannel = Registry.GetStateId("c_flow");
        TSharedSurfaceGeometryData                                   Geometry({{0, {2, 1}}});
        for (std::size_t Index = 0; Index < 2; ++Index)
        {
            auto& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {1.0F - static_cast<float>(Index), 0.0F, 0.0F};
            Texel.MesoNormal = glm::normalize(glm::vec3(-1.0F, 0.0F, 1.0F));
            Texel.HasMesoNormal = true;
            Texel.Geometry.MesoVirtualHeight = Index == 0 ? 0.2F : 0.0F;
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - Index);
        }
        Geometry.SetProfileMap({0, 0});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Table, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            2,
                                                            3,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        std::array<float, 6>                           Initial{};
        Initial[StaticChannel] = 0.7F;
        Initial[FlowChannel] = 0.5F;
        Initial[FastChannel] = 1.0F;
        Instance.GetStateABuffer().Upload(Initial.data(), sizeof(Initial));
        Vulkan.Execute(
            [&](VkCommandBuffer Commands)
            {
                Solver.RecordStep(
                    Commands, Descriptors, true, 2, 3, 0.25F, glm::mat4(1.0F), glm::vec3(0, 0, -1));
            });
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

    void TestTransferWeightSolver(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        SurfaceState::TSurfaceResponseProfileData ProfileA;
        SurfaceState::TSurfaceStateParameters     Parameters{};
        Parameters.StateCapacity = 1.0F;
        Parameters.SaturationTransferFactor = 1.0F;
        ProfileA.States.emplace("wetness", Parameters);
        SurfaceState::TSurfaceResponseProfileData                    ProfileB = ProfileA;
        SurfaceState::TSurfaceResponseProfileData                    ProfileWithoutWetness;
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{
            ProfileA, ProfileB, ProfileWithoutWetness};
        const SurfaceState::TSurfaceStateRegistry Registry(ProfileTable);

        TSharedSurfaceGeometryData     Geometry({{0, {3, 1}}});
        const std::array<glm::vec3, 3> Positions{
            glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(4.0F, 0.0F, 0.0F), glm::vec3(1.0F, 0.0F, 0.0F)};
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
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

        SurfaceState::TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            3,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

        const std::array<float, 3> InitialState{1.0F, 0.0F, 0.0F};
        Instance.GetStateABuffer().Upload(InitialState.data(), sizeof(InitialState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  true,
                                  3,
                                  1,
                                  0.25F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, -1.0F, 0.0F));
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

        SurfaceState::TSurfaceResponseProfileData NormalProfile;
        NormalProfile.States.emplace("wetness", Parameters);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> NormalProfileTable{NormalProfile};
        const SurfaceState::TSurfaceStateRegistry                    NormalRegistry(NormalProfileTable);
        TSharedSurfaceGeometryData                                   NormalGeometry({{0, {2, 1}}});
        for (std::size_t Index = 0; Index < NormalGeometry.GetTexelCount(); ++Index)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = NormalGeometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 0.0F, 0.0F};
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(1U - Index);
        }
        NormalGeometry.GetTexels()[0].TransferNormal = {0.0F, 0.0F, 1.0F};
        NormalGeometry.GetTexels()[1].TransferNormal = {std::sqrt(0.75F), 0.0F, 0.5F};
        NormalGeometry.GetTexels()[0].HasTransferNormal = true;
        NormalGeometry.GetTexels()[1].HasTransferNormal = true;
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

        TSharedSurfaceGeometryData FallbackGeometry = NormalGeometry;
        for (SurfaceState::TSurfaceTexelGeometry& Texel : FallbackGeometry.GetTexels())
        {
            Texel.HasTransferNormal = false;
        }
        FallbackGeometry.GetTexels()[1].Normal = {std::sqrt(0.75F), 0.0F, 0.5F};
        const std::vector<float> GeometricFallbackWeights =
            BuildSurfaceGPUTransferWeights(FallbackGeometry, glm::mat4(1.0F));
        Check(std::abs(GeometricFallbackWeights[0] - 0.5F) < 1.0e-5F &&
                  std::abs(GeometricFallbackWeights[SurfaceNeighborCount] - 0.5F) < 1.0e-5F,
              "missing Normal Map transfer normals should fall back to geometric normals");

        SurfaceState::TSurfaceSharedGeometryGPUResources NormalSharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), NormalGeometry);
        SurfaceState::TSurfaceProfileGPUResources NormalProfiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), NormalProfileTable, NormalRegistry);
        SurfaceState::TSurfaceInstanceGPUResources NormalInstance(
            Vulkan.GetPhysicalDevice(),
            Vulkan.GetDevice(),
            2,
            1,
            BuildSurfaceGPUTransferWeights(NormalGeometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources NormalDescriptors(
            Vulkan.GetDevice(), NormalSharedGeometry, NormalProfiles, NormalInstance);
        SurfaceState::TSurfaceStateSolver NormalSolver(Vulkan.GetDevice(), NormalDescriptors.GetLayout());

        const std::array<float, 2> NormalInitialState{1.0F, 0.0F};
        NormalInstance.GetStateABuffer().Upload(NormalInitialState.data(), sizeof(NormalInitialState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                NormalSolver.RecordStep(CommandBuffer,
                                        NormalDescriptors,
                                        true,
                                        2,
                                        1,
                                        0.25F,
                                        glm::mat4(1.0F),
                                        glm::vec3(0.0F, -1.0F, 0.0F));
            });

        std::array<float, 2> NormalResult{};
        NormalInstance.GetStateBBuffer().Download(NormalResult.data(), sizeof(NormalResult));
        Check(std::abs(NormalResult[0] - 0.875F) < 1.0e-4F && std::abs(NormalResult[1] - 0.125F) < 1.0e-4F,
              "GPU solver flux should use the Normal Map-derived NormalWeight from the cache");
    }

    void TestAccumulationGeometryUpdate(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Layer{};
        Layer.StateCapacity = 1.0F;
        Layer.AccumulationFactor = 10.0F;
        Layer.CavityFillFactor = 0.0F;
        Layer.GeometryTransferFactor = 1.0F;
        Profile.States.emplace("mud", Layer);
        SurfaceState::TSurfaceStateParameters FilmLayer = Layer;
        FilmLayer.ThicknessPerAmount = 0.002F;
        Profile.States.emplace("waterfilm", FilmLayer);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> ProfileTable{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(ProfileTable);
        const SurfaceState::TStateId                                 Mud = Registry.GetStateId("mud");
        const SurfaceState::TStateId                                 WaterFilm = Registry.GetStateId("waterfilm");
        Check(Mud != SurfaceState::InvalidStateId && WaterFilm != SurfaceState::InvalidStateId && Mud != WaterFilm,
              "geometry update fixture should resolve two independent accumulation states");

        TSharedSurfaceGeometryData     Geometry({{0, {3, 1}}});
        const std::array<glm::vec3, 3> Positions{
            glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F)};
        const std::array<std::array<TLocalTexelIndex, 2>, 3> Neighbors{{{{1, 2}}, {{0, 2}}, {{0, 1}}}};
        for (std::size_t TexelIndex = 0; TexelIndex < Geometry.GetTexelCount(); ++TexelIndex)
        {
            SurfaceState::TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[TexelIndex];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = Positions[TexelIndex];
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            Texel.NeighborIndices[0] = Neighbors[TexelIndex][0];
            Texel.NeighborIndices[1] = Neighbors[TexelIndex][1];
        }
        Geometry.SetProfileMap({0, 0, 0});

        SurfaceState::TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources Profiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        const auto StaticWeights = BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F));
        SurfaceState::TSurfaceInstanceGPUResources Instance(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 3, 2, StaticWeights);
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(
            Vulkan.GetDevice(), SharedGeometry, Profiles, Instance);
        SurfaceState::TSurfaceStateSolver Solver(Vulkan.GetDevice(), Descriptors.GetLayout());

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
            WeightsWithGeometryUpdateOff.data(), static_cast<VkDeviceSize>(WeightsWithGeometryUpdateOff.size() * sizeof(float)));
        std::array<SurfaceState::TSurfaceGPUVec4, 6> GeometryWithGeometryUpdateOff{};
        Instance.GetDynamicGeometryBuffer().Download(GeometryWithGeometryUpdateOff.data(), sizeof(GeometryWithGeometryUpdateOff));
        Check(std::equal(StaticWeights.begin(),
                         StaticWeights.end(),
                         WeightsWithGeometryUpdateOff.begin(),
                         [](float A, float B) { return std::abs(A - B) < 1.0e-6F; }) &&
                  std::abs(GeometryWithGeometryUpdateOff[0].Z) < 1.0e-7F,
              "Accumulation Geometry Update가 꺼진 상태 should retain the static transfer cache and skip dynamic geometry prepasses");
        std::array<float, 6> StateWithGeometryUpdateOff{};
        Instance.GetStateBBuffer().Download(StateWithGeometryUpdateOff.data(), sizeof(StateWithGeometryUpdateOff));
        Check(std::equal(InitialState.begin(),
                         InitialState.end(),
                         StateWithGeometryUpdateOff.begin(),
                         [](float A, float B) { return std::abs(A - B) < 1.0e-6F; }),
              "the static flat geometry should produce no geometry-driven movement in the 형상 갱신 OFF run");

        std::uint32_t Flags =
            SurfaceState::SurfaceSolverAccumulationGeometryUpdateFlag | SurfaceState::SurfaceSolverDistanceWeightFlag |
            SurfaceState::SurfaceSolverNormalWeightFlag | SurfaceState::SurfaceSolverProfileBoundaryWeightFlag;
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  false,
                                  3,
                                  2,
                                  0.0001F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F),
                                  Flags);
            });

        std::array<SurfaceState::TSurfaceGPUVec4, 6> DynamicGeometry{};
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
        const float InitialTotal = std::accumulate(InitialState.begin(), InitialState.end(), 0.0F);
        const float ResultTotal = std::accumulate(ResultingState.begin(), ResultingState.end(), 0.0F);
        Check(ResultingState[Mud] < InitialState[Mud] && ResultingState[WaterFilm] < InitialState[WaterFilm] &&
                  (ResultingState[4U + Mud] + ResultingState[4U + WaterFilm]) > 0.0F,
              "accumulation geometry should change subsequent GeometryDrive transport toward the downhill neighbor");
        Check(std::abs(InitialTotal - ResultTotal) < 1.0e-5F,
              "accumulation geometry update transport should conserve State in the no-decay fixture");

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
        std::array<float, 3> StableGeometryDirty{};
        Instance.GetAccumulationHeightBuffer().Download(
            StableHeightDirty.data(), sizeof(StableHeightDirty), sizeof(StableHeightDirty));
        Instance.GetAccumulationHeightBuffer().Download(
            StableGeometryDirty.data(), sizeof(StableGeometryDirty), sizeof(StableHeightDirty) * 2U);
        std::array<std::uint32_t, 3> StableDispatch{};
        Instance.GetAccumulationHeightBuffer().Download(
            StableDispatch.data(), sizeof(StableDispatch), (3U * 3U + 1U) * sizeof(float));
        std::vector<float> StableWeights(StaticWeights.size(), 0.0F);
        Instance.GetTransferWeightBuffer().Download(StableWeights.data(),
                                                    static_cast<VkDeviceSize>(StableWeights.size() * sizeof(float)));
        Check(
            std::all_of(StableHeightDirty.begin(), StableHeightDirty.end(), [](float V) { return V == 0.0F; }) &&
                std::all_of(StableGeometryDirty.begin(), StableGeometryDirty.end(), [](float V) { return V == 0.0F; }),
            "unchanged accumulation heights should skip geometry and transfer-weight rebuilds");
        Check(StableDispatch == std::array<std::uint32_t, 3>{0U, 1U, 1U},
              "unchanged geometry update should dispatch no geometry or transfer-weight workgroups");
        Check(std::equal(DynamicWeights.begin(), DynamicWeights.end(), StableWeights.begin()),
              "unchanged updated geometry should preserve the cached transfer weights");

        auto ChangedState = InitialState;
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
        std::array<float, 3> ChangedGeometryDirty{};
        Instance.GetAccumulationHeightBuffer().Download(
            ChangedHeightDirty.data(), sizeof(ChangedHeightDirty), sizeof(ChangedHeightDirty));
        Instance.GetAccumulationHeightBuffer().Download(
            ChangedGeometryDirty.data(), sizeof(ChangedGeometryDirty), sizeof(ChangedHeightDirty) * 2U);
        std::array<std::uint32_t, 3> ChangedDispatch{};
        Instance.GetAccumulationHeightBuffer().Download(
            ChangedDispatch.data(), sizeof(ChangedDispatch), (3U * 3U + 1U) * sizeof(float));
        Check(ChangedHeightDirty[0] == 1.0F && ChangedHeightDirty[1] == 0.0F && ChangedHeightDirty[2] == 0.0F &&
                  std::all_of(
                      ChangedGeometryDirty.begin(), ChangedGeometryDirty.end(), [](float V) { return V == 1.0F; }),
              "a changed texel should rebuild geometry across its neighbor halo");
        Check(ChangedDispatch == std::array<std::uint32_t, 3>{1U, 1U, 1U},
              "changed geometry update should dispatch geometry and transfer-weight workgroups");
    }

    void TestSparseGeometryUpdateWeights(TVulkanTestDevice& Vulkan)
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        SurfaceState::TSurfaceResponseProfileData Profile;
        SurfaceState::TSurfaceStateParameters     Layer{};
        Layer.StateCapacity = 1.0F;
        Layer.AccumulationFactor = 1.0F;
        Layer.ThicknessPerAmount = 0.1F;
        Profile.States.emplace("mud", Layer);
        const std::vector<SurfaceState::TSurfaceResponseProfileData> Profiles{Profile};
        const SurfaceState::TSurfaceStateRegistry                    Registry(Profiles);
        TSharedSurfaceGeometryData                                   Geometry({{0, {6, 1}}});
        for (std::size_t I = 0; I < 6; ++I)
        {
            auto& Texel = Geometry.GetTexels()[I];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = glm::vec3(static_cast<float>(I), 0.0F, 0.0F);
            Texel.Normal = glm::vec3(0.0F, 0.0F, 1.0F);
            if (I > 0)
                Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>(I - 1);
            if (I + 1 < 6)
                Texel.NeighborIndices[1] = static_cast<TLocalTexelIndex>(I + 1);
        }
        Geometry.SetProfileMap({0, 0, 0, 0, 0, 0});
        SurfaceState::TSurfaceSharedGeometryGPUResources Shared(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        SurfaceState::TSurfaceProfileGPUResources GPUProfiles(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Profiles, Registry);
        SurfaceState::TSurfaceInstanceGPUResources     Instance(Vulkan.GetPhysicalDevice(),
                                                            Vulkan.GetDevice(),
                                                            6,
                                                            1,
                                                            BuildSurfaceGPUTransferWeights(Geometry, glm::mat4(1.0F)));
        SurfaceState::TSurfaceStateDescriptorResources Descriptors(Vulkan.GetDevice(), Shared, GPUProfiles, Instance);
        SurfaceState::TSurfaceStateSolver              Solver(Vulkan.GetDevice(), Descriptors.GetLayout());
        const std::uint32_t                            Flags = SurfaceState::SurfaceSolverAccumulationGeometryUpdateFlag |
                                    SurfaceState::SurfaceSolverDistanceWeightFlag |
                                    SurfaceState::SurfaceSolverNormalWeightFlag;
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(
                    CommandBuffer, Descriptors, true, 6, 1, 0.0F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F), Flags);
            });

        // A distant cached edge must survive a local height change unchanged.
        constexpr float Sentinel = 0.123F;
        Instance.GetTransferWeightBuffer().Upload(
            &Sentinel, sizeof(Sentinel), 4U * SurfaceNeighborCount * sizeof(float));
        const std::array<float, 6> ChangedState{0.5F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F};
        Instance.GetStateABuffer().Upload(ChangedState.data(), sizeof(ChangedState));
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(
                    CommandBuffer, Descriptors, true, 6, 1, 0.0F, glm::mat4(1.0F), glm::vec3(0.0F, 0.0F, -1.0F), Flags);
            });
        std::array<float, 6> HeightDirty{};
        std::array<float, 6> GeometryDirty{};
        Instance.GetAccumulationHeightBuffer().Download(HeightDirty.data(), sizeof(HeightDirty), sizeof(HeightDirty));
        Instance.GetAccumulationHeightBuffer().Download(
            GeometryDirty.data(), sizeof(GeometryDirty), sizeof(HeightDirty) * 2U);
        float FarWeight = 0.0F;
        Instance.GetTransferWeightBuffer().Download(
            &FarWeight, sizeof(FarWeight), 4U * SurfaceNeighborCount * sizeof(float));
        Check(HeightDirty == std::array<float, 6>{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F} &&
                  GeometryDirty == std::array<float, 6>{1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F},
              "sparse geometry update should update the changed texel and its geometry neighbor only");
        Check(FarWeight == Sentinel, "sparse geometry update should retain distant transfer weights without rewriting them");
        Vulkan.Execute(
            [&](VkCommandBuffer CommandBuffer)
            {
                Solver.RecordStep(CommandBuffer,
                                  Descriptors,
                                  true,
                                  6,
                                  1,
                                  0.0F,
                                  glm::mat4(1.0F),
                                  glm::vec3(0.0F, 0.0F, -1.0F),
                                  Flags | SurfaceState::SurfaceSolverForceFullGeometryFlag);
            });
        std::array<std::uint32_t, 3> ForcedDispatch{};
        Instance.GetAccumulationHeightBuffer().Download(
            ForcedDispatch.data(), sizeof(ForcedDispatch), (3U * 6U + 1U) * sizeof(float));
        Instance.GetTransferWeightBuffer().Download(
            &FarWeight, sizeof(FarWeight), 4U * SurfaceNeighborCount * sizeof(float));
        Check(ForcedDispatch == std::array<std::uint32_t, 3>{1U, 1U, 1U},
              "forced full geometry should dispatch even when heights are unchanged");
        Check(std::abs(FarWeight - 1.0F) < 1.0e-5F,
              "transform and weight-setting invalidation should rebuild distant cached weights");
    }
} // namespace

int main()
{
    try
    {
        TVulkanTestDevice Vulkan;
        TestGPUResources(Vulkan);
        TestGPUSolver(Vulkan);
        TestDirectionalCavityRetention(Vulkan);
        TestAreaAndMobility(Vulkan);
        TestCalibratedGeometrySpeed(Vulkan);
        TestGeometryDrivenSolver(Vulkan);
        TestMesoGeometryDrive(Vulkan);
        TestSourceGeometryChannelReuse(Vulkan);
        TestTransferWeightSolver(Vulkan);
        TestAccumulationGeometryUpdate(Vulkan);
        TestSparseGeometryUpdateWeights(Vulkan);
    }
    catch (const TVulkanUnavailable& Exception)
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
