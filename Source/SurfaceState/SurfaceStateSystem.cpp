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

    FSurfaceStateSystem::FSurfaceStateSystem(const GPU::FVulkanContext&  Context,
                                             const Asset::FAssetManager& Assets,
                                             const FSurfaceDataManager&  SurfaceData,
                                             const FScene&               Scene,
                                             FSurfaceGPUBufferPool*      ReusePool)
        : Context(Context), Assets(Assets), SurfaceData(SurfaceData), Scene(Scene)
    {
        GPUResources = std::make_unique<FSurfaceGPUResourceManager>(
            Context,
            Assets,
            SurfaceData,
            Scene,
            ReusePool,
            FSurfaceScratchStorageFormat{Execution.bRawFluxCacheEnabled,
                                         Execution.bHalfRawFluxCacheEnabled,
                                         Execution.bHalfDynamicWeightsEnabled,
                                         Execution.bCompactScratchStorageEnabled});
        PrepareScratchStorage();
        if (const FSurfaceStateDescriptorResources* Descriptors = GPUResources->GetAnyInstanceDescriptors())
        {
            Solver = std::make_unique<FSurfaceStateSolver>(Context.GetDevice(), Descriptors->GetLayout());
        }
        // Dynamic geometry buffers start empty.  The first state-driven solver step performs one
        // force-full bootstrap before Pass 1, then normal steps use Solver -> Height -> Geometry -> Weights.
        Execution.bForceFullGeometryOnNextStep = true;
        Execution.bSeedPersistentActivityOnNextStep = true;
        Execution.PendingInputActivation.assign(GPUResources->GetSceneInstanceCount(), false);
        QueueInitialContacts();
    }

    void FSurfaceStateSystem::QueueInitialContacts()
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        for (const FSceneInitialContact& Initial : Scene.GetInitialContacts())
        {
            const auto Target =
                std::find_if(Instances.begin(),
                             Instances.end(),
                             [&](const FStaticMeshInstance& Instance) { return Instance.GetId() == Initial.Target; });
            if (Target == Instances.end())
                throw std::invalid_argument("Initial contact target is not in Scene: " + Initial.Target);
            FSurfaceContactInput Contact;
            Contact.TargetInstance = static_cast<FSurfaceInstanceID>(std::distance(Instances.begin(), Target));
            Contact.State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Initial.State);
            Contact.WorldPosition = Initial.WorldPosition;
            Contact.Radius = Initial.Radius;
            Contact.Strength = Initial.Strength;
            Contact.Falloff = Initial.Falloff;
            SubmitContact(Contact);
        }
    }

    FSurfaceStateSystem::~FSurfaceStateSystem() = default;

    void FSurfaceStateSystem::ReplaceSceneResources(FSurfaceStateSystem&& Replacement)
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
        Execution.PendingInputActivation = std::move(Replacement.Execution.PendingInputActivation);
        Execution.DebugSolverSettings = Replacement.Execution.DebugSolverSettings;
        Execution.bTransferWeightSettingsDirty = Replacement.Execution.bTransferWeightSettingsDirty;
        Execution.bForceFullGeometryOnNextStep = Replacement.Execution.bForceFullGeometryOnNextStep;
        Execution.bSeedPersistentActivityOnNextStep = Replacement.Execution.bSeedPersistentActivityOnNextStep;
        Execution.bStableDeltaTimeDirty = Replacement.Execution.bStableDeltaTimeDirty;
        Execution.CachedMaximumStableDeltaTime = Replacement.Execution.CachedMaximumStableDeltaTime;
        Execution.StableDeltaTimeModelMatrices = std::move(Replacement.Execution.StableDeltaTimeModelMatrices);
        RuntimeProfileOverrides = std::move(Replacement.RuntimeProfileOverrides);
    }
#pragma endregion

// ============================================================
// 01. State and Contact Management
// ============================================================
#pragma region 01_State_and_Contact_Management

    void FSurfaceStateSystem::SubmitContact(FSurfaceContactInput Contact)
    {
        PendingContacts.push_back(std::move(Contact));
    }

    void FSurfaceStateSystem::ResetState()
    {
        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before resetting Surface State.");
        }
        PendingContacts.clear();
        GPUResources->ResetStates();
        // Resetting State invalidates the derived simulation geometry.  Rebuild it before the next
        // state-driven solver step rather than tracking a separate revision/cache key.
        Execution.bForceFullGeometryOnNextStep = true;
        Execution.bSeedPersistentActivityOnNextStep = true;
        std::fill(Execution.PendingInputActivation.begin(), Execution.PendingInputActivation.end(), false);
    }

    void FSurfaceStateSystem::RestartState()
    {
        ResetState();
        QueueInitialContacts();
        Execution.bStableDeltaTimeDirty = true;
        Execution.bForceFullGeometryOnNextStep = true;
    }

    void FSurfaceStateSystem::SetDebugProfileParameters(Asset::FSRProfileAssetHandle   Profile,
                                                        FStateId                       State,
                                                        const FSurfaceStateParameters& Parameters,
                                                        bool                           bKeepRuntimeOverride)
    {
        const FSurfaceStateRegistry& Registry = SurfaceData.GetSurfaceStateRegistry();
        if (State >= Registry.GetStateCount())
        {
            throw std::out_of_range("Debug Profile override State is outside the Registry.");
        }

        FSurfaceResponseProfileData ValidationData;
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

        Execution.bStableDeltaTimeDirty = true;
        // Accumulation factors/capacity can change the State -> Height mapping.  A one-shot full
        // rebuild on the next solver step keeps derived data authoritative without a revision cache.
        Execution.bForceFullGeometryOnNextStep = true;
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

    std::size_t FSurfaceStateSystem::GetSolverInstanceCount() const noexcept
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

    void FSurfaceStateSystem::SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept
    {
        if (Execution.bHalfDynamicWeightsEnabled == bEnabled)
            return;
        Execution.bHalfDynamicWeightsEnabled = bEnabled;
        // Static weights must be re-uploaded in the selected representation; dynamic state-driven geometry then
        // rebuilds from it.
        Execution.bTransferWeightSettingsDirty = true;
        Execution.bForceFullGeometryOnNextStep = true;
        Execution.bStableDeltaTimeDirty = true;
    }

    void FSurfaceStateSystem::SetSparseSolverEnabled(bool bEnabled) noexcept
    {
        if (Execution.bSparseSolverEnabled == bEnabled)
            return;
        Execution.bSparseSolverEnabled = bEnabled;
        if (bEnabled)
            Execution.bSeedPersistentActivityOnNextStep = true;
    }

    void FSurfaceStateSystem::SetActiveChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (Execution.bActiveChannelMaskEnabled == bEnabled)
            return;
        Execution.bActiveChannelMaskEnabled = bEnabled;
        if (bEnabled)
            Execution.bSeedPersistentActivityOnNextStep = true;
    }

    void FSurfaceStateSystem::SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept
    {
        if (Execution.bPerWorkgroupChannelMaskEnabled == bEnabled)
            return;
        Execution.bPerWorkgroupChannelMaskEnabled = bEnabled;
        if (bEnabled)
            Execution.bSeedPersistentActivityOnNextStep = true;
    }

    const FSurfaceSolverDebugSettings& FSurfaceStateSystem::GetDebugSolverSettings() const noexcept
    {
        return Execution.DebugSolverSettings;
    }

    void FSurfaceStateSystem::SetStateDrivenGeometryUpdateEnabled(bool bEnabled) noexcept
    {
        if (Execution.DebugSolverSettings.bStateDrivenGeometryUpdateEnabled != bEnabled)
            Execution.bTransferWeightSettingsDirty = true;
        Execution.DebugSolverSettings.bStateDrivenGeometryUpdateEnabled = bEnabled;
    }

    void FSurfaceStateSystem::SetDebugSolverTermEnabled(ESurfaceSolverTerm Term, bool bEnabled) noexcept
    {
        if (Execution.DebugSolverSettings.IsEnabled(Term) == bEnabled)
        {
            return;
        }
        Execution.DebugSolverSettings.SetEnabled(Term, bEnabled);
        Execution.bStableDeltaTimeDirty = true;
        if (Term == ESurfaceSolverTerm::DistanceWeight || Term == ESurfaceSolverTerm::NormalWeight ||
            Term == ESurfaceSolverTerm::ProfileBoundaryWeight)
        {
            Execution.bTransferWeightSettingsDirty = true;
        }
    }

    void FSurfaceStateSystem::SetDebugGeometryDriveEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(ESurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    void FSurfaceStateSystem::SetDebugNormalWeightEnabled(bool bEnabled) noexcept
    {
        SetDebugSolverTermEnabled(ESurfaceSolverTerm::NormalWeight, bEnabled);
    }

    const FSurfaceGPUResourceManager& FSurfaceStateSystem::GetGPUResources() const noexcept
    {
        return *GPUResources;
    }
#pragma endregion
} // namespace MDSS::SurfaceState
