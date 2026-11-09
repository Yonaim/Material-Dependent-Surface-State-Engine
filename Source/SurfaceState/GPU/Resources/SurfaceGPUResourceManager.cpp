/**
 * @file SurfaceGPUResourceManager.cpp
 * @brief Scene별 공유 Surface와 instance별 GPU resource 수명 주기.
 */

#include "SurfaceState/GPU/Resources/SurfaceGPUResourceManager.h"

#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    const GPU::FGPUBuffer& FSurfaceGPUResourceManager::GetInstanceReactionScratchBuffer(std::size_t Index) const
    {
        if (Index >= InstanceResources.size() || !InstanceResources[Index])
            throw std::out_of_range("Invalid Surface instance.");
        return InstanceResources[Index]->State->GetReactionScratchBuffer();
    }
    namespace
    {
        constexpr std::uint32_t DescriptorBindingCount =
            static_cast<std::uint32_t>(ESurfaceGPUDescriptorBinding::Count);
    } // namespace

// ============================================================
// 00. Resource Lifecycle
// ============================================================
#pragma region 00_Resource_Lifecycle

    FSurfaceGPUResourceManager::FSurfaceGPUResourceManager(const GPU::FVulkanContext&   Context,
                                                           const Asset::FAssetManager&  Assets,
                                                           const FSurfaceDataManager&   SurfaceData,
                                                           const FScene&                Scene,
                                                           FSurfaceGPUBufferPool*       Pool,
                                                           FSurfaceScratchStorageFormat Storage)
    {
        ScratchStorageFormat =
            std::array<bool, 4>{Storage.bCacheRawFlux, Storage.bHalfRawFlux, Storage.bHalfWeights, Storage.bCompact};
        const VkPhysicalDevice       PhysicalDevice = Context.GetPhysicalDevice();
        const VkDevice               Device = Context.GetDevice();
        const FSurfaceStateRegistry& Registry = SurfaceData.GetSurfaceStateRegistry();
        bool                         bDescriptorLimitsChecked = false;
        SceneProfileHandles = SurfaceData.GetSceneSurfaceProfiles(Scene);
        if (SceneProfileHandles.size() >= InvalidSurfaceProfileIndex)
        {
            throw std::overflow_error("Scene Profile table exceeds the supported Profile index range.");
        }
        if (!SceneProfileHandles.empty() && Registry.GetStateCount() != 0)
        {
            std::vector<FSurfaceResponseProfileData> Profiles;
            Profiles.reserve(SceneProfileHandles.size());
            for (Asset::FSRProfileAssetHandle Handle : SceneProfileHandles)
            {
                Profiles.push_back(Assets.GetSRProfile(Handle).GetData());
            }
            SceneProfiles =
                std::make_unique<FSurfaceProfileGPUResources>(PhysicalDevice, Device, Profiles, Registry, Pool);
        }

        InstanceResources.reserve(Scene.GetStaticMeshInstances().size());
        for (const FStaticMeshInstance& MeshInstance : Scene.GetStaticMeshInstances())
        {
            std::unique_ptr<FInstanceResources> Instance;
            const FSurfaceRuntimeDataHandle     SurfaceDataHandle = MeshInstance.GetSurfaceData();
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
                    const FSurfaceRuntimeData& RuntimeData = SurfaceData.GetSurfaceData(SurfaceDataHandle);
                    const std::vector<Asset::FSRProfileAssetHandle>& ProfileHandles =
                        SurfaceData.GetSurfaceProfileTable(SurfaceDataHandle);
                    FSharedSurfaceResources Resources;
                    Resources.SceneProfileIndices.reserve(ProfileHandles.size());
                    for (Asset::FSRProfileAssetHandle ProfileHandle : ProfileHandles)
                    {
                        const auto Found =
                            std::lower_bound(SceneProfileHandles.begin(), SceneProfileHandles.end(), ProfileHandle);
                        if (Found == SceneProfileHandles.end() || *Found != ProfileHandle)
                        {
                            throw std::logic_error("Runtime Surface Profile is missing from the Scene table.");
                        }
                        Resources.SceneProfileIndices.push_back(
                            static_cast<FSurfaceProfileIndex>(std::distance(SceneProfileHandles.begin(), Found)));
                    }
                    Resources.Geometry = std::make_unique<FSurfaceSharedGeometryGPUResources>(
                        PhysicalDevice,
                        Device,
                        *RuntimeData.GetSharedGeometry(),
                        Resources.SceneProfileIndices,
                        Assets.GetMesh(MeshInstance.GetMesh()).GetVertices(),
                        Assets.GetMesh(MeshInstance.GetMesh()).GetTriangles(),
                        Pool);
                    Resources.CPUGeometry = RuntimeData.GetSharedGeometry().get();
                    SharedIt = SharedSurfaceData.emplace(SurfaceDataHandle, std::move(Resources)).first;
                }

                const std::size_t            TexelCount = SharedIt->second.Geometry->GetTexelCount();
                const glm::mat4              ModelMatrix = MeshInstance.GetTransform().GetMatrix();
                std::vector<FSurfaceGPUVec4> TransferWeightDebugAverages;
                const std::vector<float>     TransferWeights = BuildSurfaceGPUTransferWeights(
                    *SharedIt->second.CPUGeometry, ModelMatrix, &TransferWeightDebugAverages);
                auto State = std::make_unique<FSurfaceInstanceGPUResources>(
                    PhysicalDevice,
                    Device,
                    TexelCount,
                    Registry.GetStateCount(),
                    TransferWeights,
                    TransferWeightDebugAverages,
                    BuildSurfaceGPUWorldTexelAreas(*SharedIt->second.CPUGeometry, ModelMatrix),
                    Pool,
                    Storage,
                    SceneProfiles->GetMaxReactionCount());
                auto Descriptors = std::make_unique<FSurfaceStateDescriptorResources>(
                    Device, *SharedIt->second.Geometry, *SceneProfiles, *State);

                Instance = std::make_unique<FInstanceResources>();
                Instance->State = std::move(State);
                Instance->Descriptors = std::move(Descriptors);
                Instance->SurfaceDataHandle = SurfaceDataHandle;
                Instance->ValidTexelCount = static_cast<std::size_t>(
                    std::count_if(SharedIt->second.CPUGeometry->GetTexels().begin(),
                                  SharedIt->second.CPUGeometry->GetTexels().end(),
                                  [](const FSurfaceTexelGeometry& Texel) { return Texel.IsValid(); }));
                Instance->TransferWeightScale = MeshInstance.GetTransform().Scale;
                Instance->bTransferWeightCacheValid = true;
            }
            InstanceResources.push_back(std::move(Instance));
        }

        FLogger::Info("TSurfaceGPUResourceManager",
                      "Created resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                          " Surface data variant(s) and " + std::to_string(GetManagedInstanceCount()) +
                          " Surface instances; one Scene Profile table with " + std::to_string(GetSceneProfileCount()) +
                          " unique Profile(s), " + std::to_string(Registry.GetStateCount()) + " State channel(s).");
    }

    FSurfaceGPUResourceManager::~FSurfaceGPUResourceManager()
    {
        FLogger::Debug("TSurfaceGPUResourceManager",
                       "Releasing resources for " + std::to_string(GetSharedSurfaceDataCount()) +
                           " Surface data variant(s) and " + std::to_string(GetManagedInstanceCount()) +
                           " Surface instances.");
    }

    void FSurfaceGPUResourceManager::RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept
    {
        // Descriptors must be gone before any buffer can be loaned to a later Scene.
        for (auto& Instance : InstanceResources)
            if (Instance)
                Instance->Descriptors.reset();
        for (auto& Instance : InstanceResources)
            if (Instance && Instance->State)
                Instance->State->RetireBuffers(Pool);
        for (auto& [Handle, Shared] : SharedSurfaceData)
            if (Shared.Geometry)
                Shared.Geometry->RetireBuffers(Pool);
        if (SceneProfiles)
            SceneProfiles->RetireBuffers(Pool);
    }
#pragma endregion

// ============================================================
// 01. Resource Accessors and Profile Updates
// ============================================================
#pragma region 01_Resource_Accessors_and_Profile_Updates

    bool FSurfaceGPUResourceManager::ConfigureScratchStorage(
        const GPU::FVulkanContext& Context, bool bCache, bool bHalfRaw, bool bHalfWeights, bool bCompact)
    {
        const std::array<bool, 4> Format{bCache, bHalfRaw, bHalfWeights, bCompact};
        if (ScratchStorageFormat == Format)
            return false;
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait before changing solver storage format.");
        bool bChanged = false;
        for (auto& Instance : InstanceResources)
        {
            if (!Instance)
                continue;
            if (Instance->State->ConfigureScratchStorage(
                    Context.GetPhysicalDevice(), Context.GetDevice(), bCache, bHalfRaw, bHalfWeights, bCompact))
            {
                auto Replacement = std::make_unique<FSurfaceStateDescriptorResources>(
                    Context.GetDevice(),
                    *SharedSurfaceData.at(Instance->SurfaceDataHandle).Geometry,
                    *SceneProfiles,
                    *Instance->State);
                Instance->Descriptors = std::move(Replacement);
                bChanged = true;
            }
        }
        ScratchStorageFormat = Format;
        return bChanged;
    }

    void FSurfaceGPUResourceManager::TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay)
    {
        for (auto& [Handle, Resources] : SharedSurfaceData)
            Resources.Geometry->TrimRenderMeshVariants(Surface, Overlay);
    }

    std::size_t FSurfaceGPUResourceManager::GetManagedInstanceCount() const noexcept
    {
        std::size_t Count = 0;
        for (const std::unique_ptr<FInstanceResources>& Instance : InstanceResources)
        {
            Count += static_cast<std::size_t>(static_cast<bool>(Instance));
        }
        return Count;
    }

    std::size_t FSurfaceGPUResourceManager::GetSharedSurfaceDataCount() const noexcept
    {
        return SharedSurfaceData.size();
    }

    std::size_t FSurfaceGPUResourceManager::GetSceneProfileCount() const noexcept
    {
        return SceneProfiles != nullptr ? SceneProfiles->GetProfileCount() : 0;
    }

    std::size_t FSurfaceGPUResourceManager::GetSceneInstanceCount() const noexcept
    {
        return InstanceResources.size();
    }

    const FSurfaceStateDescriptorResources*
    FSurfaceGPUResourceManager::GetInstanceDescriptors(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size())
        {
            throw std::out_of_range("Scene instance index is outside Surface GPU resource range.");
        }
        const std::unique_ptr<FInstanceResources>& Instance = InstanceResources[SceneIndex];
        return Instance ? Instance->Descriptors.get() : nullptr;
    }

    const FSurfaceSharedGeometryGPUResources*
    FSurfaceGPUResourceManager::GetInstanceSharedGeometry(std::size_t SceneIndex) const
    {
        const auto& Instance = InstanceResources.at(SceneIndex);
        return Instance ? SharedSurfaceData.at(Instance->SurfaceDataHandle).Geometry.get() : nullptr;
    }

    const FSurfaceStateDescriptorResources* FSurfaceGPUResourceManager::GetAnyInstanceDescriptors() const noexcept
    {
        for (const std::unique_ptr<FInstanceResources>& Instance : InstanceResources)
        {
            if (Instance && Instance->Descriptors)
            {
                return Instance->Descriptors.get();
            }
        }
        return nullptr;
    }

    const GPU::FGPUBuffer& FSurfaceGPUResourceManager::GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU input buffer.");
        }
        return InstanceResources[SceneIndex]->State->GetInputDeltaBuffer();
    }

    const GPU::FGPUBuffer& FSurfaceGPUResourceManager::GetInstanceAccumulationHeightBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU accumulation buffer.");
        }
        return InstanceResources[SceneIndex]->State->GetAccumulationHeightBuffer();
    }

    const GPU::FGPUBuffer& FSurfaceGPUResourceManager::GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        const FInstanceResources& Instance = *InstanceResources[SceneIndex];
        return Instance.bCurrentStateAB ? Instance.State->GetStateABuffer() : Instance.State->GetStateBBuffer();
    }

    const GPU::FGPUBuffer& FSurfaceGPUResourceManager::GetSceneProfileParametersBuffer() const
    {
        if (SceneProfiles == nullptr)
        {
            throw std::out_of_range("Scene has no Profile GPU resources.");
        }
        return SceneProfiles->GetParametersBuffer();
    }

    bool FSurfaceGPUResourceManager::UpdateProfileParameters(Asset::FSRProfileAssetHandle   ProfileHandle,
                                                             FStateId                       State,
                                                             const FSurfaceStateParameters& Parameters)
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
#pragma endregion

// ============================================================
// 02. State Reset and Transfer Weight Cache
// ============================================================
#pragma region 02_State_Reset_and_Transfer_Weight_Cache

    std::size_t FSurfaceGPUResourceManager::GetInstanceTexelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->State->GetTexelCount();
    }

    std::size_t FSurfaceGPUResourceManager::GetInstanceValidTexelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->ValidTexelCount;
    }

    std::size_t FSurfaceGPUResourceManager::GetInstanceChannelCount(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->State->GetChannelCount();
    }

    bool FSurfaceGPUResourceManager::IsCurrentStateAB(std::size_t SceneIndex) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        return InstanceResources[SceneIndex]->bCurrentStateAB;
    }

    void FSurfaceGPUResourceManager::ResetStates()
    {
        for (const std::unique_ptr<FInstanceResources>& Instance : InstanceResources)
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

            const FSharedSurfaceGeometryData&  Geometry = *SharedIt->second.CPUGeometry;
            const FSurfaceProfileGPUResources& Profiles = *SceneProfiles;
            const std::size_t                  ChannelCount = Instance->State->GetChannelCount();
            std::vector<float>                 InitialOutgoingFluxScale(Geometry.GetTexelCount() * ChannelCount, 0.0F);
            const std::vector<FSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
            for (std::size_t TexelIndex = 0; TexelIndex < Texels.size(); ++TexelIndex)
            {
                if (!Texels[TexelIndex].IsValid())
                {
                    continue;
                }
                const FSurfaceProfileIndex ProfileIndex =
                    Geometry.GetProfileIndex(static_cast<FLocalTexelIndex>(TexelIndex));
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

    bool FSurfaceGPUResourceManager::NeedsTransferWeightCacheUpdate(std::size_t       SceneIndex,
                                                                    const FTransform& Transform) const
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        const FInstanceResources& Instance = *InstanceResources[SceneIndex];
        if (!Instance.bTransferWeightCacheValid)
        {
            return true;
        }
        return Instance.TransferWeightScale.x != Transform.Scale.x ||
               Instance.TransferWeightScale.y != Transform.Scale.y ||
               Instance.TransferWeightScale.z != Transform.Scale.z;
    }

    void FSurfaceGPUResourceManager::UpdateTransferWeightCache(std::size_t       SceneIndex,
                                                               const FTransform& Transform,
                                                               bool              bUseNormalWeight,
                                                               bool              bUseDistanceWeight,
                                                               bool              bUseProfileBoundaryWeight,
                                                               bool              bHalfPrecision)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        FInstanceResources& Instance = *InstanceResources[SceneIndex];
        auto                SharedIt = SharedSurfaceData.find(Instance.SurfaceDataHandle);
        if (SharedIt == SharedSurfaceData.end() || SharedIt->second.CPUGeometry == nullptr)
        {
            throw std::logic_error("Surface TransferWeight cache has no source Geometry.");
        }
        const glm::mat4              ModelMatrix = Transform.GetMatrix();
        std::vector<FSurfaceGPUVec4> TransferWeightDebugAverages;
        const std::vector<float>     TransferWeights = BuildSurfaceGPUTransferWeights(*SharedIt->second.CPUGeometry,
                                                                                  ModelMatrix,
                                                                                  &TransferWeightDebugAverages,
                                                                                  bUseNormalWeight,
                                                                                  bUseDistanceWeight,
                                                                                  bUseProfileBoundaryWeight);
        Instance.State->UpdateTransferWeights(TransferWeights, TransferWeightDebugAverages, bHalfPrecision);
        Instance.State->UpdateWorldTexelAreas(
            BuildSurfaceGPUWorldTexelAreas(*SharedIt->second.CPUGeometry, ModelMatrix));
        Instance.TransferWeightScale = Transform.Scale;
        Instance.bTransferWeightCacheValid = true;
    }

    void FSurfaceGPUResourceManager::InvalidateTransferWeightCache(std::size_t SceneIndex)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU State resources.");
        }
        InstanceResources[SceneIndex]->bTransferWeightCacheValid = false;
    }

    void FSurfaceGPUResourceManager::AdvanceCurrentState(std::size_t SceneIndex)
    {
        if (SceneIndex >= InstanceResources.size() || !InstanceResources[SceneIndex])
        {
            throw std::out_of_range("Scene instance has no Surface GPU state resources.");
        }
        InstanceResources[SceneIndex]->bCurrentStateAB = !InstanceResources[SceneIndex]->bCurrentStateAB;
    }
#pragma endregion
} // namespace MDSS::SurfaceState
