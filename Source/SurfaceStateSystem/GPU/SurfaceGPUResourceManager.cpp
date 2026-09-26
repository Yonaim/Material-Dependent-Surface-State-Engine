/**
 * @file SurfaceGPUResourceManager.cpp
 * @brief Scene별 공유 Surface와 instance별 GPU resource 수명 주기.
 */

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"

#include "AssetManager/Core/AssetManager.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/VulkanContext.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS
{
    namespace
    {
        constexpr std::uint32_t DescriptorBindingCount =
            static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count);
    } // namespace

    TSurfaceGPUResourceManager::TSurfaceGPUResourceManager(const TVulkanContext& Context,
                                                           const TAssetManager& Assets,
                                                           const TScene& Scene)
    {
        const VkPhysicalDevice PhysicalDevice = Context.GetPhysicalDevice();
        const VkDevice         Device = Context.GetDevice();
        const TSurfaceStateRegistry& Registry = Assets.GetSurfaceStateRegistry();
        bool bDescriptorLimitsChecked = false;

        InstanceResources.reserve(Scene.GetStaticMeshInstances().size());
        for (const TStaticMeshInstance& MeshInstance : Scene.GetStaticMeshInstances())
        {
            std::unique_ptr<TInstanceResources> Instance;
            const TMeshAssetHandle MeshHandle = MeshInstance.GetMesh();
            if (Assets.HasSurfaceData(MeshHandle))
            {
                if (!bDescriptorLimitsChecked)
                {
                    VkPhysicalDeviceProperties Properties{};
                    vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
                    if (Properties.limits.maxDescriptorSetStorageBuffers < DescriptorBindingCount ||
                        Properties.limits.maxPerStageDescriptorStorageBuffers < DescriptorBindingCount)
                    {
                        throw std::runtime_error(
                            "Selected Vulkan device does not support the Surface descriptor storage-buffer count.");
                    }
                    bDescriptorLimitsChecked = true;
                }

                auto SharedIt = SharedMeshes.find(MeshHandle);
                if (SharedIt == SharedMeshes.end())
                {
                    const TSurfaceRuntimeData& RuntimeData = Assets.GetSurfaceData(MeshHandle);
                    const std::vector<TSRProfileAssetHandle>& ProfileHandles = Assets.GetSurfaceProfileTable(MeshHandle);
                    std::vector<TSurfaceResponseProfileData> Profiles;
                    Profiles.reserve(ProfileHandles.size());
                    for (TSRProfileAssetHandle ProfileHandle : ProfileHandles)
                    {
                        Profiles.push_back(Assets.GetSRProfile(ProfileHandle).GetData());
                    }

                    TSharedMeshResources Resources;
                    Resources.Geometry = std::make_unique<TSurfaceSharedGeometryGPUResources>(
                        PhysicalDevice, Device, *RuntimeData.GetSharedGeometry());
                    Resources.Profiles = std::make_unique<TSurfaceProfileGPUResources>(
                        PhysicalDevice, Device, Profiles, Registry);
                    SharedIt = SharedMeshes.emplace(MeshHandle, std::move(Resources)).first;
                }

                const std::size_t TexelCount = SharedIt->second.Geometry->GetTexelCount();
                auto State = std::make_unique<TSurfaceInstanceGPUResources>(
                    PhysicalDevice, Device, TexelCount, Registry.GetStateCount());
                auto Descriptors = std::make_unique<TSurfaceStateDescriptorResources>(
                    Device, *SharedIt->second.Geometry, *SharedIt->second.Profiles, *State);

                Instance = std::make_unique<TInstanceResources>();
                Instance->State = std::move(State);
                Instance->Descriptors = std::move(Descriptors);
            }
            InstanceResources.push_back(std::move(Instance));
        }

        TLogger::Info("TSurfaceGPUResourceManager",
                      "Created resources for " + std::to_string(GetManagedMeshCount()) + " Mesh and " +
                          std::to_string(GetManagedInstanceCount()) + " Surface instances.");
    }

    TSurfaceGPUResourceManager::~TSurfaceGPUResourceManager()
    {
        TLogger::Debug("TSurfaceGPUResourceManager",
                       "Releasing resources for " + std::to_string(GetManagedMeshCount()) + " Mesh and " +
                           std::to_string(GetManagedInstanceCount()) + " Surface instances.");
    }

    std::size_t TSurfaceGPUResourceManager::GetManagedInstanceCount() const noexcept
    {
        std::size_t Count = 0;
        for (const std::unique_ptr<TInstanceResources>& Instance : InstanceResources)
        {
            Count += static_cast<std::size_t>(static_cast<bool>(Instance));
        }
        return Count;
    }

    std::size_t TSurfaceGPUResourceManager::GetManagedMeshCount() const noexcept
    {
        return SharedMeshes.size();
    }

    const TSurfaceStateDescriptorResources*
    TSurfaceGPUResourceManager::GetInstanceDescriptors(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size())
        {
            throw std::out_of_range("Scene instance index is outside Surface GPU resource range.");
        }
        const std::unique_ptr<TInstanceResources>& Instance = InstanceResources[SceneIndex];
        return Instance ? Instance->Descriptors.get() : nullptr;
    }
} // namespace MDSS
