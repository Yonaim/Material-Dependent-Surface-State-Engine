/**
 * @file SurfaceGPUResourceTests.cpp
 * @brief Vulkan resource layout, initialization, and descriptor wiring tests.
 */

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
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

            constexpr float QueuePriority = 1.0F;
            VkDeviceQueueCreateInfo QueueInfo{};
            QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueInfo.queueFamilyIndex = QueueFamily;
            QueueInfo.queueCount = 1;
            QueueInfo.pQueuePriorities = &QueuePriority;

            std::vector<const char*> DeviceExtensions;
            std::uint32_t ExtensionCount = 0;
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
        }

        ~TVulkanTestDevice()
        {
            if (Device != VK_NULL_HANDLE)
            {
                vkDeviceWaitIdle(Device);
                vkDestroyDevice(Device, nullptr);
            }
            if (Instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(Instance, nullptr);
            }
        }

        TVulkanTestDevice(const TVulkanTestDevice&) = delete;
        TVulkanTestDevice& operator=(const TVulkanTestDevice&) = delete;

        [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const noexcept { return PhysicalDevice; }
        [[nodiscard]] VkDevice GetDevice() const noexcept { return Device; }

    private:
        VkInstance Instance = VK_NULL_HANDLE;
        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        std::uint32_t QueueFamily = 0;
        VkDevice Device = VK_NULL_HANDLE;
    };

    MDSS::TSharedSurfaceGeometryData BuildGeometry()
    {
        using namespace MDSS;
        TSharedSurfaceGeometryData Geometry({{0, {2, 2}}});
        for (std::size_t Index = 0; Index < Geometry.GetTexels().size(); ++Index)
        {
            TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            Texel.Surface = 0;
            Texel.Triangle = 0;
            Texel.Position = {static_cast<float>(Index), 2.0F, 3.0F};
            Texel.NeighborIndices[0] = static_cast<TLocalTexelIndex>((Index + 1U) % Geometry.GetTexelCount());
        }
        Geometry.SetProfileMap({0, 0, 0, 0});
        return Geometry;
    }

    void CheckZeroBuffer(const MDSS::TGPUBuffer& Buffer, std::size_t ScalarCount, const std::string& Name)
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
        VkPhysicalDeviceProperties DeviceProperties{};
        vkGetPhysicalDeviceProperties(Vulkan.GetPhysicalDevice(), &DeviceProperties);
        const std::uint32_t RequiredStorageBufferBindings =
            static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count);
        if (DeviceProperties.limits.maxDescriptorSetStorageBuffers < RequiredStorageBufferBindings ||
            DeviceProperties.limits.maxPerStageDescriptorStorageBuffers < RequiredStorageBufferBindings)
        {
            throw TVulkanUnavailable("Vulkan device does not support the Surface descriptor storage-buffer count.");
        }

        TSurfaceResponseProfileData Profile;
        TSurfaceStateParameters Parameters{};
        Parameters.StateCapacity = 3.0F;
        Parameters.InputFactor = 0.75F;
        Profile.States.emplace("wetness", Parameters);
        const std::vector<TSurfaceResponseProfileData> ProfileTable{Profile};
        const TSurfaceStateRegistry Registry(ProfileTable);
        TSharedSurfaceGeometryData Geometry = BuildGeometry();

        TSurfaceSharedGeometryGPUResources SharedGeometry(
            Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), Geometry);
        TSurfaceProfileGPUResources Profiles(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), ProfileTable, Registry);
        TSurfaceInstanceGPUResources InstanceA(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1);
        TSurfaceInstanceGPUResources InstanceB(Vulkan.GetPhysicalDevice(), Vulkan.GetDevice(), 4, 1);
        TSurfaceStateDescriptorResources DescriptorsA(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceA);
        TSurfaceStateDescriptorResources DescriptorsB(
            Vulkan.GetDevice(), SharedGeometry, Profiles, InstanceB);

        Check(SharedGeometry.GetTexelCount() == 4, "shared Geometry should retain four texels");
        Check(SharedGeometry.GetNeighborIndexBuffer().GetSize() == 4U * sizeof(TSurfaceGPUNeighborIndices),
              "neighbor buffer should use the expected per-texel stride");
        Check(DescriptorsA.GetABSet() != VK_NULL_HANDLE && DescriptorsA.GetBASet() != VK_NULL_HANDLE,
              "both A/B descriptor sets should be allocated");
        Check(DescriptorsB.GetABSet() != VK_NULL_HANDLE && DescriptorsB.GetBASet() != VK_NULL_HANDLE,
              "each instance should own its descriptor sets");

        const auto Bound = [](const TSurfaceStateDescriptorResources& Descriptors,
                              TSurfaceGPUDescriptorBinding Binding,
                              bool bAB)
        { return Descriptors.GetBoundBufferHandle(Binding, bAB); };
        Check(Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  SharedGeometry.GetTexelSurfaceIndexBuffer().GetHandle(),
              "descriptor should bind the shared Geometry buffer");
        Check(Bound(DescriptorsB, TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true) ==
                  Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::TexelSurfaceIndices, true),
              "instances should bind the same shared Geometry buffer");
        Check(Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::CurrentState, true) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "AB descriptor should read State A");
        Check(Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::NextState, true) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "AB descriptor should write State B");
        Check(Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::CurrentState, false) ==
                  InstanceA.GetStateBBuffer().GetHandle(),
              "BA descriptor should read State B");
        Check(Bound(DescriptorsA, TSurfaceGPUDescriptorBinding::NextState, false) ==
                  InstanceA.GetStateABuffer().GetHandle(),
              "BA descriptor should write State A");
        Check(InstanceA.GetStateABuffer().GetHandle() != InstanceB.GetStateABuffer().GetHandle() &&
                  InstanceA.GetStateBBuffer().GetHandle() != InstanceB.GetStateBBuffer().GetHandle(),
              "separate instances should own separate State buffers");
        Check(Bound(DescriptorsB, TSurfaceGPUDescriptorBinding::CurrentState, true) ==
                  InstanceB.GetStateABuffer().GetHandle(),
              "second instance descriptor should refer to its own state");

        CheckZeroBuffer(InstanceA.GetStateABuffer(), 4, "State A");
        CheckZeroBuffer(InstanceA.GetStateBBuffer(), 4, "State B");
        CheckZeroBuffer(InstanceA.GetOutgoingFluxScaleBuffer(), 4, "OutgoingFluxScale");
        CheckZeroBuffer(InstanceA.GetInputDeltaBuffer(), 4, "InputDelta");
        CheckZeroBuffer(InstanceB.GetStateABuffer(), 4, "second instance State A");
        CheckZeroBuffer(InstanceB.GetStateBBuffer(), 4, "second instance State B");
        CheckZeroBuffer(InstanceB.GetOutgoingFluxScaleBuffer(), 4, "second instance OutgoingFluxScale");
        CheckZeroBuffer(InstanceB.GetInputDeltaBuffer(), 4, "second instance InputDelta");

        std::array<TSurfaceGPUVec4, 4> UploadedPositions{};
        SharedGeometry.GetPositionBuffer().Download(UploadedPositions.data(), sizeof(UploadedPositions));
        Check(UploadedPositions[2].X == 2.0F && UploadedPositions[2].Y == 2.0F && UploadedPositions[2].Z == 3.0F,
              "position upload should preserve texel-major values");

        std::array<TSurfaceGPUNeighborIndices, 4> UploadedNeighbors{};
        SharedGeometry.GetNeighborIndexBuffer().Download(UploadedNeighbors.data(), sizeof(UploadedNeighbors));
        Check(UploadedNeighbors[2].Indices[0] == 3U,
              "neighbor upload should preserve the local neighbor index");

        std::array<TSurfaceGPUProfileParameters, 1> UploadedProfiles{};
        Profiles.GetParametersBuffer().Download(UploadedProfiles.data(), sizeof(UploadedProfiles));
        Check(UploadedProfiles[0].CapacityInputAndTransfer[0] == 3.0F &&
                  UploadedProfiles[0].CapacityInputAndTransfer[1] == 0.75F,
              "Profile parameters should be packed in the declared ABI slots");
        std::array<std::uint32_t, 1> ProfileSupported{};
        Profiles.GetSupportedBuffer().Download(ProfileSupported.data(), sizeof(ProfileSupported));
        Check(ProfileSupported[0] == 1U, "defined Profile channel should be marked supported");
    }
} // namespace

int main()
{
    try
    {
        TVulkanTestDevice Vulkan;
        TestGPUResources(Vulkan);
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
