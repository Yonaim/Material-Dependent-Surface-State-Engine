/**
 * @file SurfaceStateSystem.cpp
 * @brief SurfaceStateSystem의 구현을 제공한다.
 */

#include "SurfaceState/SurfaceStateSystem.h"

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"
#include "SurfaceState/Types/SurfaceSolverRates.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Lifecycle
// ============================================================
#pragma region 00_Lifecycle

    TSurfaceStateSystem::TSurfaceStateSystem(const GPU::TVulkanContext&  Context,
                                             const Asset::TAssetManager& Assets,
                                             const TSurfaceDataManager&  SurfaceData,
                                             const TScene&               Scene,
                                             TSurfaceGPUBufferPool*      ReusePool)
        : Context(Context), Assets(Assets), SurfaceData(SurfaceData), Scene(Scene)
    {
        GPUResources = std::make_unique<TSurfaceGPUResourceManager>(Context, Assets, SurfaceData, Scene, ReusePool,
            TSurfaceScratchStorageFormat{bRawFluxCacheEnabled, bHalfRawFluxCacheEnabled,
                                        bHalfDynamicWeightsEnabled, bCompactScratchStorageEnabled});
        PrepareScratchStorage();
        if (const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetAnyInstanceDescriptors())
        {
            Solver = std::make_unique<TSurfaceStateSolver>(Context.GetDevice(), Descriptors->GetLayout());
        }
        // Dynamic geometry buffers start empty.  The first state-driven solver step performs one
        // force-full bootstrap before Pass 1, then normal steps use Solver -> Height -> Geometry -> Weights.
        bForceFullGeometryOnNextStep = true;
        bSeedPersistentActivityOnNextStep = true;
        PendingInputActivation.assign(GPUResources->GetSceneInstanceCount(), false);
        QueueInitialContacts();
    }

    void TSurfaceStateSystem::QueueInitialContacts()
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        for (const TSceneInitialContact& Initial : Scene.GetInitialContacts())
        {
            const auto Target =
                std::find_if(Instances.begin(),
                             Instances.end(),
                             [&](const TStaticMeshInstance& Instance) { return Instance.GetId() == Initial.Target; });
            if (Target == Instances.end())
                throw std::invalid_argument("Initial contact target is not in Scene: " + Initial.Target);
            TSurfaceContactInput Contact;
            Contact.TargetInstance = static_cast<TSurfaceInstanceID>(std::distance(Instances.begin(), Target));
            Contact.State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Initial.State);
            Contact.WorldPosition = Initial.WorldPosition;
            Contact.Radius = Initial.Radius;
            Contact.Strength = Initial.Strength;
            Contact.Falloff = Initial.Falloff;
            SubmitContact(Contact);
        }
    }

    TSurfaceStateSystem::~TSurfaceStateSystem() = default;

    void TSurfaceStateSystem::ReplaceSceneResources(TSurfaceStateSystem&& Replacement)
    {
        if (&Context != &Replacement.Context || &Assets != &Replacement.Assets || &Scene != &Replacement.Scene)
        {
            throw std::invalid_argument("Replacement Surface State resources must use the same runtime owners.");
        }

        Solver = std::move(Replacement.Solver);
        if (GPUResources)
            GPUResources->RetireBuffers(BufferPool);
        GPUResources = std::move(Replacement.GPUResources);
        PendingContacts = std::move(Replacement.PendingContacts);
        PendingInputActivation = std::move(Replacement.PendingInputActivation);
        DebugSolverSettings = Replacement.DebugSolverSettings;
        bTransferWeightSettingsDirty = Replacement.bTransferWeightSettingsDirty;
        bForceFullGeometryOnNextStep = Replacement.bForceFullGeometryOnNextStep;
        bSeedPersistentActivityOnNextStep = Replacement.bSeedPersistentActivityOnNextStep;
        bStableDeltaTimeDirty = Replacement.bStableDeltaTimeDirty;
        CachedMaximumStableDeltaTime = Replacement.CachedMaximumStableDeltaTime;
        StableDeltaTimeModelMatrices = std::move(Replacement.StableDeltaTimeModelMatrices);
        RuntimeProfileOverrides = std::move(Replacement.RuntimeProfileOverrides);
    }
#pragma endregion

// ============================================================
// 01. State and Contact Management
// ============================================================
#pragma region 01_State_and_Contact_Management

    void TSurfaceStateSystem::SubmitContact(TSurfaceContactInput Contact)
    {
        PendingContacts.push_back(std::move(Contact));
    }

    void TSurfaceStateSystem::ResetState()
    {
        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before resetting Surface State.");
        }
        PendingContacts.clear();
        GPUResources->ResetStates();
        // Resetting State invalidates the derived simulation geometry.  Rebuild it before the next
        // state-driven solver step rather than tracking a separate revision/cache key.
        bForceFullGeometryOnNextStep = true;
        bSeedPersistentActivityOnNextStep = true;
        std::fill(PendingInputActivation.begin(), PendingInputActivation.end(), false);
    }

    void TSurfaceStateSystem::RestartState()
    {
        ResetState();
        QueueInitialContacts();
        bStableDeltaTimeDirty = true;
        bForceFullGeometryOnNextStep = true;
    }

    void TSurfaceStateSystem::SetDebugProfileParameters(Asset::TSRProfileAssetHandle   Profile,
                                                        TStateId                       State,
                                                        const TSurfaceStateParameters& Parameters,
                                                        bool                           bKeepRuntimeOverride)
    {
        const TSurfaceStateRegistry& Registry = SurfaceData.GetSurfaceStateRegistry();
        if (State >= Registry.GetStateCount())
        {
            throw std::out_of_range("Debug Profile override State is outside the Registry.");
        }

        TSurfaceResponseProfileData ValidationData;
        ValidationData.States.emplace(Registry.GetStateName(State), Parameters);
        ValidateSurfaceResponseProfileData(ValidationData);

        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before updating Profile parameters.");
        }
        if (!GPUResources->UpdateProfileParameters(Profile, State, Parameters))
        {
            throw std::invalid_argument("Debug Profile override does not belong to the current Scene.");
        }

        bStableDeltaTimeDirty = true;
        // Accumulation factors/capacity can change the State -> Height mapping.  A one-shot full
        // rebuild on the next solver step keeps derived data authoritative without a revision cache.
        bForceFullGeometryOnNextStep = true;
        const auto Key = std::make_pair(Profile, State);
        if (bKeepRuntimeOverride)
        {
            RuntimeProfileOverrides[Key] = Parameters;
        }
        else
        {
            RuntimeProfileOverrides.erase(Key);
        }
    }

#pragma endregion

// ============================================================
// 02. Debug and Accessors
// ============================================================
#pragma region 02_Debug_and_Accessors

    std::size_t TSurfaceStateSystem::GetSolverInstanceCount() const noexcept
    {
        if (!GPUResources)
            return 0U;
        std::size_t InstanceCount = 0U;
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            InstanceCount += GPUResources->GetInstanceDescriptors(SceneIndex) != nullptr ? 1U : 0U;
        }
        return InstanceCount;
    }

    void TSurfaceStateSystem::SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept
    {
        if (bHalfDynamicWeightsEnabled == bEnabled)
            return;
        bHalfDynamicWeightsEnabled = bEnabled;
        // Static weights must be re-uploaded in the selected representation; dynamic state-driven geometry then
        // rebuilds from it.
        bTransferWeightSettingsDirty = true;
        bForceFullGeometryOnNextStep = true;
        bStableDeltaTimeDirty = true;
    }

    void TSurfaceStateSystem::SetSparseSolverEnabled(bool bEnabled) noexcept
    {
        if (bSparseSolverEnabled == bEnabled)
            return;
        bSparseSolverEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    void TSurfaceStateSystem::SetActiveChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (bActiveChannelMaskEnabled == bEnabled)
            return;
        bActiveChannelMaskEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    void TSurfaceStateSystem::SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (bPerWorkgroupChannelMaskEnabled == bEnabled)
            return;
        bPerWorkgroupChannelMaskEnabled = bEnabled;
        if (bEnabled)
            bSeedPersistentActivityOnNextStep = true;
    }

    const TSurfaceSolverDebugSettings& TSurfaceStateSystem::GetDebugSolverSettings() const noexcept
    {
        return DebugSolverSettings;
    }

    void TSurfaceStateSystem::SetStateDrivenGeometryUpdateEnabled(bool bEnabled) noexcept
    {
        if (DebugSolverSettings.bStateDrivenGeometryUpdateEnabled != bEnabled)
            bTransferWeightSettingsDirty = true;
        DebugSolverSettings.bStateDrivenGeometryUpdateEnabled = bEnabled;
    }

    void TSurfaceStateSystem::SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept
    {
        if (DebugSolverSettings.IsEnabled(Term) == bEnabled)
        {
            return;
        }
        DebugSolverSettings.SetEnabled(Term, bEnabled);
        bStableDeltaTimeDirty = true;
        if (Term == TSurfaceSolverTerm::DistanceWeight || Term == TSurfaceSolverTerm::NormalWeight ||
            Term == TSurfaceSolverTerm::ProfileBoundaryWeight)
        {
            bTransferWeightSettingsDirty = true;
        }
    }

    void TSurfaceStateSystem::SetDebugGeometryDriveEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    void TSurfaceStateSystem::SetDebugNormalWeightEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(TSurfaceSolverTerm::NormalWeight, bEnabled);
    }

    const TSurfaceGPUResourceManager& TSurfaceStateSystem::GetGPUResources() const noexcept
    {
        return *GPUResources;
    }
#pragma endregion
} // namespace MDSS::SurfaceState
