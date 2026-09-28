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

#include <algorithm>
#include <iterator>
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
            const TSurfaceRuntimeDataHandle SurfaceDataHandle = MeshInstance.GetSurfaceData();
            if (Assets.HasSurfaceData(SurfaceDataHandle))
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

                auto SharedIt = SharedSurfaceData.find(SurfaceDataHandle);
                if (SharedIt == SharedSurfaceData.end())
                {
                    const TSurfaceRuntimeData& RuntimeData = Assets.GetSurfaceData(SurfaceDataHandle);
                    const std::vector<TSRProfileAssetHandle>& ProfileHandles =
                        Assets.GetSurfaceProfileTable(SurfaceDataHandle);
                    std::vector<TSurfaceResponseProfileData> Profiles;
                    Profiles.reserve(ProfileHandles.size());
                    for (TSRProfileAssetHandle ProfileHandle : ProfileHandles)
                    {
                        Profiles.push_back(Assets.GetSRProfile(ProfileHandle).GetData());
                    }

                    TSharedSurfaceResources Resources;
                    Resources.Geometry = std::make_unique<TSurfaceSharedGeometryGPUResources>(
                        PhysicalDevice, Device, *RuntimeData.GetSharedGeometry());
                    Resources.Profiles = std::make_unique<TSurfaceProfileGPUResources>(
                        PhysicalDevice, Device, Profiles, Registry);
                    Resources.ProfileHandles = ProfileHandles;
                    Resources.CPUGeometry = RuntimeData.GetSharedGeometry().get();
                    SharedIt = SharedSurfaceData.emplace(SurfaceDataHandle, std::move(Resources)).first;
                }

                const std::size_t TexelCount = SharedIt->second.Geometry->GetTexelCount();
                const glm::mat4 ModelMatrix = MeshInstance.GetTransform().GetMatrix();
                std::vector<TSurfaceGPUVec4> TransferWeightDebugAverages;
                const std::vector<float> TransferWeights = BuildSurfaceGPUTransferWeights(
                    *SharedIt->second.CPUGeometry, ModelMatrix, &TransferWeightDebugAverages);
                auto State = std::make_unique<TSurfaceInstanceGPUResources>(
                    PhysicalDevice,
                    Device,
                    TexelCount,
                    Registry.GetStateCount(),
                    TransferWeights,
                    TransferWeightDebugAverages);
                auto Descriptors = std::make_unique<TSurfaceStateDescriptorResources>(
                    Device, *SharedIt->second.Geometry, *SharedIt->second.Profiles, *State);

                Instance = std::make_unique<TInstanceResources>();
                Instance->State = std::move(State);
                Instance->Descriptors = std::move(Descriptors);
                Instance->SurfaceDataHandle = SurfaceDataHandle;
                Instance->ValidTexelCount = static_cast<std::size_t>(std::count_if(
                    SharedIt->second.CPUGeometry->GetTexels().begin(),
                    SharedIt->second.CPUGeometry->GetTexels().end(),
                    [](const TSurfaceTexelGeometry& Texel) { return Texel.IsValid(); }));
                Instance->TransferWeightModelMatrix = ModelMatrix;
                Instance->bTransferWeightCacheValid = true;
            }
            InstanceResources.push_back(std::move(Instance));
        }

        TLogger::Info("TSurfaceGPUResourceManager",
                      "Created resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                          " Surface data variant(s) and " +
                          std::to_string(GetManagedInstanceCount()) + " Surface instances.");
    }

    TSurfaceGPUResourceManager::~TSurfaceGPUResourceManager()
    {
        TLogger::Debug("TSurfaceGPUResourceManager",
                       "Releasing resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                           " Surface data variant(s) and " +
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

    std::size_t TSurfaceGPUResourceManager::GetSharedSurfaceDataCount() const noexcept
    {
        return SharedSurfaceData.size();
    }

    std::size_t TSurfaceGPUResourceManager::GetSceneInstanceCount() const noexcept
    {
        return InstanceResources.size();
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

    const TSurfaceStateDescriptorResources* TSurfaceGPUResourceManager::GetAnyInstanceDescriptors() const noexcept
    {
        for (const std::unique_ptr<TInstanceResources>& Instance : InstanceResources)
        {
            if (Instance && Instance->Descriptors)
            {
                return Instance->Descriptors.get();
            }
        }
        return nullptr;
    }

    const TGPUBuffer& TSurfaceGPUResourceManager::GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU input buffer.");
        }
        return InstanceResources[SceneIndex]->State->GetInputDeltaBuffer();
    }

    bool TSurfaceGPUResourceManager::UpdateProfileParameters(TSRProfileAssetHandle ProfileHandle,
                                                              TStateId State,
                                                              const TSurfaceStateParameters& Parameters)
    {
        bool bUpdated = false;
        for (auto& [SurfaceDataHandle, Resources] : SharedSurfaceData)
        {
            (void)SurfaceDataHandle;
            const auto ProfileIt = std::find(Resources.ProfileHandles.begin(),
                                             Resources.ProfileHandles.end(),
                                             ProfileHandle);
            if (ProfileIt == Resources.ProfileHandles.end())
            {
                continue;
            }

            Resources.Profiles->UpdateParameters(
                static_cast<std::size_t>(std::distance(Resources.ProfileHandles.begin(), ProfileIt)),
                State,
                Parameters);
            bUpdated = true;
        }
        return bUpdated;
    }

    std::size_t TSurfaceGPUResourceManager::GetInstanceTexelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->State->GetTexelCount();
    }

    std::size_t TSurfaceGPUResourceManager::GetInstanceValidTexelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->ValidTexelCount;
    }

    std::size_t TSurfaceGPUResourceManager::GetInstanceChannelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->State->GetChannelCount();
    }

    bool TSurfaceGPUResourceManager::IsCurrentStateAB(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->bCurrentStateAB;
    }

    void TSurfaceGPUResourceManager::ResetStates()
    {
        for (const std::unique_ptr<TInstanceResources>& Instance : InstanceResources)
        {
            if (!Instance)
            {
                continue;
            }
            const auto SharedIt = SharedSurfaceData.find(Instance->SurfaceDataHandle);
            if (SharedIt == SharedSurfaceData.end() || SharedIt->second.CPUGeometry == nullptr)
            {
                throw std::logic_error("Surface GPU instance lost its shared CPU geometry during reset.");
            }

            const TSharedSurfaceGeometryData& Geometry = *SharedIt->second.CPUGeometry;
            const TSurfaceProfileGPUResources& Profiles = *SharedIt->second.Profiles;
            const std::size_t ChannelCount = Instance->State->GetChannelCount();
            std::vector<float> InitialOutgoingFluxScale(Geometry.GetTexelCount() * ChannelCount, 0.0F);
            const std::vector<TSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
            for (std::size_t TexelIndex = 0; TexelIndex < Texels.size(); ++TexelIndex)
            {
                if (!Texels[TexelIndex].IsValid())
                {
                    continue;
                }
                const TSurfaceProfileIndex ProfileIndex =
                    Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(TexelIndex));
                if (ProfileIndex == InvalidSurfaceProfileIndex)
                {
                    continue;
                }
                for (std::size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
                {
                    if (Profiles.IsSupported(ProfileIndex, ChannelIndex))
                    {
                        InitialOutgoingFluxScale[TexelIndex * ChannelCount + ChannelIndex] = 1.0F;
                    }
                }
            }

            Instance->State->ResetState(InitialOutgoingFluxScale);
            Instance->bCurrentStateAB = true;
        }
    }

    bool TSurfaceGPUResourceManager::NeedsTransferWeightCacheUpdate(std::size_t SceneIndex,
                                                                     const glm::mat4& ModelMatrix) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        const TInstanceResources& Instance = *InstanceResources[SceneIndex];
        if (!Instance.bTransferWeightCacheValid)
        {
            return true;
        }
        for (glm::length_t Column = 0; Column < 3; ++Column)
        {
            for (glm::length_t Row = 0; Row < 3; ++Row)
            {
                if (Instance.TransferWeightModelMatrix[Column][Row] != ModelMatrix[Column][Row])
                {
                    return true;
                }
            }
        }
        return false;
    }

    void TSurfaceGPUResourceManager::UpdateTransferWeightCache(std::size_t SceneIndex,
                                                                const glm::mat4& ModelMatrix,
                                                                bool bUseNormalWeight,
                                                                bool bUseDistanceWeight,
                                                                bool bUseProfileBoundaryWeight,
                                                                bool bUseCurvatureWeight)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        TInstanceResources& Instance = *InstanceResources[SceneIndex];
        auto SharedIt = SharedSurfaceData.find(Instance.SurfaceDataHandle);
        if (SharedIt == SharedSurfaceData.end() || SharedIt->second.CPUGeometry == nullptr)
        {
            throw std::logic_error("Surface TransferWeight cache has no source Geometry.");
        }
        std::vector<TSurfaceGPUVec4> TransferWeightDebugAverages;
        const std::vector<float> TransferWeights =
            BuildSurfaceGPUTransferWeights(*SharedIt->second.CPUGeometry,
                                           ModelMatrix,
                                           &TransferWeightDebugAverages,
                                           bUseNormalWeight,
                                           bUseDistanceWeight,
                                           bUseProfileBoundaryWeight,
                                           bUseCurvatureWeight);
        Instance.State->UpdateTransferWeights(TransferWeights, TransferWeightDebugAverages);
        Instance.TransferWeightModelMatrix = ModelMatrix;
        Instance.bTransferWeightCacheValid = true;
    }

    void TSurfaceGPUResourceManager::InvalidateTransferWeightCache(std::size_t SceneIndex)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        InstanceResources[SceneIndex]->bTransferWeightCacheValid = false;
    }

    void TSurfaceGPUResourceManager::AdvanceCurrentState(std::size_t SceneIndex)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        InstanceResources[SceneIndex]->bCurrentStateAB = !InstanceResources[SceneIndex]->bCurrentStateAB;
    }
} // namespace MDSS
