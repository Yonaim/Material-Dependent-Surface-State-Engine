/**
 * @file SurfaceGPUResourceManager.cpp
 * @brief Scene별 공유 Surface와 instance별 GPU resource 수명 주기.
 */

#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    namespace
    {
        constexpr std::uint32_t DescriptorBindingCount =
            static_cast<std::uint32_t>(TSurfaceGPUDescriptorBinding::Count);
    } // namespace

    TSurfaceGPUResourceManager::TSurfaceGPUResourceManager(const GPU::TVulkanContext&  Context,
                                                           const Asset::TAssetManager& Assets,
                                                           const TSurfaceDataManager&  SurfaceData,
                                                           const TScene&               Scene)
    {
        const VkPhysicalDevice       PhysicalDevice = Context.GetPhysicalDevice();
        const VkDevice               Device = Context.GetDevice();
        const TSurfaceStateRegistry& Registry = SurfaceData.GetSurfaceStateRegistry();
        bool                         bDescriptorLimitsChecked = false;
        SceneProfileHandles = SurfaceData.GetSceneSurfaceProfiles(Scene);
        if (SceneProfileHandles.size() >= InvalidSurfaceProfileIndex)
        {
            throw std::overflow_error("Scene Profile table exceeds the supported Profile index range.");
        }
        if (!SceneProfileHandles.empty() && Registry.GetStateCount() != 0)
        {
            std::vector<TSurfaceResponseProfileData> Profiles;
            Profiles.reserve(SceneProfileHandles.size());
            for (Asset::TSRProfileAssetHandle Handle : SceneProfileHandles)
            {
                Profiles.push_back(Assets.GetSRProfile(Handle).GetData());
            }
            SceneProfiles = std::make_unique<TSurfaceProfileGPUResources>(PhysicalDevice, Device, Profiles, Registry);
        }

        InstanceResources.reserve(Scene.GetStaticMeshInstances().size());
        for (const TStaticMeshInstance& MeshInstance : Scene.GetStaticMeshInstances())
        {
            std::unique_ptr<TInstanceResources> Instance;
            const TSurfaceRuntimeDataHandle     SurfaceDataHandle = MeshInstance.GetSurfaceData();
            if (SurfaceData.HasSurfaceData(SurfaceDataHandle) && SceneProfiles != nullptr)
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
                    const TSurfaceRuntimeData& RuntimeData = SurfaceData.GetSurfaceData(SurfaceDataHandle);
                    const std::vector<Asset::TSRProfileAssetHandle>& ProfileHandles =
                        SurfaceData.GetSurfaceProfileTable(SurfaceDataHandle);
                    TSharedSurfaceResources Resources;
                    Resources.SceneProfileIndices.reserve(ProfileHandles.size());
                    for (Asset::TSRProfileAssetHandle ProfileHandle : ProfileHandles)
                    {
                        const auto Found =
                            std::lower_bound(SceneProfileHandles.begin(), SceneProfileHandles.end(), ProfileHandle);
                        if (Found == SceneProfileHandles.end() || *Found != ProfileHandle)
                        {
                            throw std::logic_error("Runtime Surface Profile is missing from the Scene table.");
                        }
                        Resources.SceneProfileIndices.push_back(
                            static_cast<TSurfaceProfileIndex>(std::distance(SceneProfileHandles.begin(), Found)));
                    }
                    Resources.Geometry = std::make_unique<TSurfaceSharedGeometryGPUResources>(
                        PhysicalDevice,
                        Device,
                        *RuntimeData.GetSharedGeometry(),
                        Resources.SceneProfileIndices,
                        Assets.GetMesh(MeshInstance.GetMesh()).GetVertices(),
                        Assets.GetMesh(MeshInstance.GetMesh()).GetTriangles());
                    Resources.CPUGeometry = RuntimeData.GetSharedGeometry().get();
                    SharedIt = SharedSurfaceData.emplace(SurfaceDataHandle, std::move(Resources)).first;
                }

                const std::size_t            TexelCount = SharedIt->second.Geometry->GetTexelCount();
                const glm::mat4              ModelMatrix = MeshInstance.GetTransform().GetMatrix();
                std::vector<TSurfaceGPUVec4> TransferWeightDebugAverages;
                const std::vector<float>     TransferWeights = BuildSurfaceGPUTransferWeights(
                    *SharedIt->second.CPUGeometry, ModelMatrix, &TransferWeightDebugAverages);
                auto State = std::make_unique<TSurfaceInstanceGPUResources>(
                    PhysicalDevice,
                    Device,
                    TexelCount,
                    Registry.GetStateCount(),
                    TransferWeights,
                    TransferWeightDebugAverages,
                    BuildSurfaceGPUWorldTexelAreas(*SharedIt->second.CPUGeometry, ModelMatrix));
                auto Descriptors = std::make_unique<TSurfaceStateDescriptorResources>(
                    Device, *SharedIt->second.Geometry, *SceneProfiles, *State);

                Instance = std::make_unique<TInstanceResources>();
                Instance->State = std::move(State);
                Instance->Descriptors = std::move(Descriptors);
                Instance->SurfaceDataHandle = SurfaceDataHandle;
                Instance->ValidTexelCount = static_cast<std::size_t>(
                    std::count_if(SharedIt->second.CPUGeometry->GetTexels().begin(),
                                  SharedIt->second.CPUGeometry->GetTexels().end(),
                                  [](const TSurfaceTexelGeometry& Texel) { return Texel.IsValid(); }));
                Instance->TransferWeightScale = MeshInstance.GetTransform().Scale;
                Instance->bTransferWeightCacheValid = true;
            }
            InstanceResources.push_back(std::move(Instance));
        }

        TLogger::Info("TSurfaceGPUResourceManager",
                      "Created resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                          " Surface data variant(s) and " + std::to_string(GetManagedInstanceCount()) +
                          " Surface instances; one Scene Profile table with " + std::to_string(GetSceneProfileCount()) +
                          " unique Profile(s), " + std::to_string(Registry.GetStateCount()) + " State channel(s).");
    }

    TSurfaceGPUResourceManager::~TSurfaceGPUResourceManager()
    {
        TLogger::Debug("TSurfaceGPUResourceManager",
                       "Releasing resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                           " Surface data variant(s) and " + std::to_string(GetManagedInstanceCount()) +
                           " Surface instances.");
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

    std::size_t TSurfaceGPUResourceManager::GetSceneProfileCount() const noexcept
    {
        return SceneProfiles != nullptr ? SceneProfiles->GetProfileCount() : 0;
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

    const TSurfaceSharedGeometryGPUResources*
    TSurfaceGPUResourceManager::GetInstanceSharedGeometry(std::size_t SceneIndex) const
    {
        const auto& Instance = InstanceResources.at(SceneIndex);
        return Instance ? SharedSurfaceData.at(Instance->SurfaceDataHandle).Geometry.get() : nullptr;
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

    const GPU::TGPUBuffer& TSurfaceGPUResourceManager::GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU input buffer.");
        }
        return InstanceResources[SceneIndex]->State->GetInputDeltaBuffer();
    }

    const GPU::TGPUBuffer& TSurfaceGPUResourceManager::GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        const TInstanceResources& Instance = *InstanceResources[SceneIndex];
        return Instance.bCurrentStateAB ? Instance.State->GetStateABuffer() : Instance.State->GetStateBBuffer();
    }

    const GPU::TGPUBuffer& TSurfaceGPUResourceManager::GetSceneProfileParametersBuffer() const
    {
        if (SceneProfiles == nullptr)
        {
            throw std::out_of_range("Scene has no Profile GPU resources.");
        }
        return SceneProfiles->GetParametersBuffer();
    }

    bool TSurfaceGPUResourceManager::UpdateProfileParameters(Asset::TSRProfileAssetHandle   ProfileHandle,
                                                             TStateId                       State,
                                                             const TSurfaceStateParameters& Parameters)
    {
        const auto Found = std::lower_bound(SceneProfileHandles.begin(), SceneProfileHandles.end(), ProfileHandle);
        if (SceneProfiles == nullptr || Found == SceneProfileHandles.end() || *Found != ProfileHandle)
        {
            return false;
        }
        SceneProfiles->UpdateParameters(
            static_cast<std::size_t>(std::distance(SceneProfileHandles.begin(), Found)), State, Parameters);
        return true;
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

            const TSharedSurfaceGeometryData&  Geometry = *SharedIt->second.CPUGeometry;
            const TSurfaceProfileGPUResources& Profiles = *SceneProfiles;
            const std::size_t                  ChannelCount = Instance->State->GetChannelCount();
            std::vector<float>                 InitialOutgoingFluxScale(Geometry.GetTexelCount() * ChannelCount, 0.0F);
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
                    if (Profiles.IsSupported(SharedIt->second.SceneProfileIndices.at(ProfileIndex), ChannelIndex))
                    {
                        InitialOutgoingFluxScale[TexelIndex * ChannelCount + ChannelIndex] = 1.0F;
                    }
                }
            }

            Instance->State->ResetState(InitialOutgoingFluxScale);
            Instance->bCurrentStateAB = true;
        }
    }

    bool TSurfaceGPUResourceManager::NeedsTransferWeightCacheUpdate(std::size_t       SceneIndex,
                                                                    const TTransform& Transform) const
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
        return Instance.TransferWeightScale.x != Transform.Scale.x ||
               Instance.TransferWeightScale.y != Transform.Scale.y ||
               Instance.TransferWeightScale.z != Transform.Scale.z;
    }

    void TSurfaceGPUResourceManager::UpdateTransferWeightCache(std::size_t       SceneIndex,
                                                               const TTransform& Transform,
                                                               bool              bUseNormalWeight,
                                                               bool              bUseDistanceWeight,
                                                               bool              bUseProfileBoundaryWeight)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        TInstanceResources& Instance = *InstanceResources[SceneIndex];
        auto                SharedIt = SharedSurfaceData.find(Instance.SurfaceDataHandle);
        if (SharedIt == SharedSurfaceData.end() || SharedIt->second.CPUGeometry == nullptr)
        {
            throw std::logic_error("Surface TransferWeight cache has no source Geometry.");
        }
        const glm::mat4              ModelMatrix = Transform.GetMatrix();
        std::vector<TSurfaceGPUVec4> TransferWeightDebugAverages;
        const std::vector<float>     TransferWeights = BuildSurfaceGPUTransferWeights(*SharedIt->second.CPUGeometry,
                                                                                  ModelMatrix,
                                                                                  &TransferWeightDebugAverages,
                                                                                  bUseNormalWeight,
                                                                                  bUseDistanceWeight,
                                                                                  bUseProfileBoundaryWeight);
        Instance.State->UpdateTransferWeights(TransferWeights, TransferWeightDebugAverages);
        Instance.State->UpdateWorldTexelAreas(
            BuildSurfaceGPUWorldTexelAreas(*SharedIt->second.CPUGeometry, ModelMatrix));
        Instance.TransferWeightScale = Transform.Scale;
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
} // namespace MDSS::SurfaceState
