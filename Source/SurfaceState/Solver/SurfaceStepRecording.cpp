/**
 * @file SurfaceStateSystem.cpp
 * @brief SurfaceStateSystem의 구현을 제공한다.
 */

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
#include "SurfaceState/SurfaceStateSystem.h"
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
// 00. GPU Recording
// ============================================================
#pragma region 00_GPU_Recording

    void FSurfaceStateSystem::RecordStep(VkCommandBuffer CommandBuffer,
                                         float           DeltaTime,
                                         VkQueryPool     TimestampQueryPool,
                                         std::uint32_t   FirstStepQuery,
                                         bool            bPrepareRenderHeight)
    {
        ApplyPendingContacts();
        if (!Solver)
        {
            return;
        }

        bool bHasDirtyTransferWeightCache = Execution.bTransferWeightSettingsDirty;
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            if (GPUResources->GetInstanceDescriptors(SceneIndex) == nullptr)
            {
                continue;
            }
            bHasDirtyTransferWeightCache = bHasDirtyTransferWeightCache ||
                                           GPUResources->NeedsTransferWeightCacheUpdate(
                                               SceneIndex, Scene.GetStaticMeshInstances()[SceneIndex].GetTransform());
        }
        if (bHasDirtyTransferWeightCache && vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for the graphics queue before updating TransferWeight caches.");
        }

        std::vector<FSurfaceSolverInstanceStep> InstanceSteps;
        std::vector<std::size_t>                ActiveSceneIndices;
        InstanceSteps.reserve(GPUResources->GetSceneInstanceCount());
        ActiveSceneIndices.reserve(GPUResources->GetSceneInstanceCount());
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            const FSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(SceneIndex);
            if (Descriptors == nullptr)
            {
                continue;
            }

            const bool                 bCurrentStateAB = GPUResources->IsCurrentStateAB(SceneIndex);
            const FStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[SceneIndex];
            const glm::mat4            ModelMatrix = Instance.GetTransform().GetMatrix();
            const bool                 bRebuildStaticWeights =
                Execution.bTransferWeightSettingsDirty ||
                GPUResources->NeedsTransferWeightCacheUpdate(SceneIndex, Instance.GetTransform());
            if (bRebuildStaticWeights)
            {
                GPUResources->UpdateTransferWeightCache(
                    SceneIndex,
                    Instance.GetTransform(),
                    Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::NormalWeight),
                    Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::DistanceWeight),
                    Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::ProfileBoundaryWeight),
                    Execution.bHalfDynamicWeightsEnabled);
            }
            const glm::vec3 GravityWorld(0.0F, 0.0F, -1.0F);
            std::uint32_t   SolverFlags = 0U;
            if (!Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::GeometryDrive))
            {
                SolverFlags |= 1U << 0U;
            }
            if (!Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::SaturationDrive))
            {
                SolverFlags |= 1U << 1U;
            }
            if (!Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::Decay))
            {
                SolverFlags |= 1U << 2U;
            }
            if (!Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::ConcavityRetention))
            {
                SolverFlags |= 1U << 3U;
            }
            if (!Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::MesoDirectionNormal))
            {
                SolverFlags |= 1U << 4U;
            }
            if (Execution.bRawFluxCacheEnabled)
            {
                SolverFlags |= SurfaceSolverRawFluxCacheFlag;
                if (Execution.bCoalescedRawFluxLayoutEnabled)
                    SolverFlags |= SurfaceSolverCoalescedRawFluxLayoutFlag;
                if (Execution.bHalfRawFluxCacheEnabled)
                    SolverFlags |= SurfaceSolverHalfRawFluxCacheFlag;
            }
            if (Execution.bHalfDynamicWeightsEnabled)
                SolverFlags |= SurfaceSolverHalfDynamicWeightsFlag;
            if (Execution.bSeedPersistentActivityOnNextStep &&
                (Execution.bSparseSolverEnabled || Execution.bActiveChannelMaskEnabled ||
                 Execution.bPerWorkgroupChannelMaskEnabled))
                SolverFlags |= SurfaceSolverSeedPersistentActivityFlag;
            if (Execution.bSparseSolverEnabled)
                SolverFlags |= SurfaceSolverSparseSolverFlag;
            if (Execution.bSparseAccumulationHeightEnabled)
                SolverFlags |= SurfaceSolverSparseAccumulationHeightFlag;
            if (Execution.bActiveChannelMaskEnabled)
                SolverFlags |= SurfaceSolverActiveChannelMaskFlag;
            if (Execution.bPerWorkgroupChannelMaskEnabled)
                SolverFlags |= SurfaceSolverPerWorkgroupChannelMaskFlag;
            if (Execution.bSparseSimulationGeometryEnabled)
                SolverFlags |= SurfaceSolverSparseGeometryFlag;
            if (Execution.bAdaptiveGeometryEnabled)
                SolverFlags |= SurfaceSolverAdaptiveGeometryFlag;

            // Height validity is independent of state-driven geometry updates. If updates are OFF the full rebuild
            // is deferred until the final render-height step instead of adding revision/cache bookkeeping.
            if (bRebuildStaticWeights || Execution.bForceFullGeometryOnNextStep)
                SolverFlags |= SurfaceSolverForceFullGeometryFlag;

            if (Execution.DebugSolverSettings.bStateDrivenGeometryUpdateEnabled)
            {
                SolverFlags |= SurfaceSolverStateDrivenGeometryUpdateFlag;
                if (Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::DistanceWeight))
                    SolverFlags |= SurfaceSolverDistanceWeightFlag;
                if (Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::NormalWeight))
                    SolverFlags |= SurfaceSolverNormalWeightFlag;
                if (Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::ProfileBoundaryWeight))
                    SolverFlags |= SurfaceSolverProfileBoundaryWeightFlag;
            }
            FSurfaceSolverInstanceStep Step;
            Step.Descriptors = Descriptors;
            Step.bCurrentStateAB = bCurrentStateAB;
            Step.TexelCount = GPUResources->GetInstanceTexelCount(SceneIndex);
            Step.ChannelCount = GPUResources->GetInstanceChannelCount(SceneIndex);
            Step.DeltaTime = DeltaTime;
            Step.ModelMatrix = ModelMatrix;
            Step.GravityWorld = GravityWorld;
            Step.SolverFlags = SolverFlags;
            Step.bPrepareAccumulationHeight = bPrepareRenderHeight;
            Step.bHasInputActivation =
                SceneIndex < Execution.PendingInputActivation.size() && Execution.PendingInputActivation[SceneIndex];
            InstanceSteps.push_back(Step);
            ActiveSceneIndices.push_back(SceneIndex);
        }
        Solver->RecordSteps(CommandBuffer, InstanceSteps, TimestampQueryPool, FirstStepQuery);
        for (const std::size_t SceneIndex : ActiveSceneIndices)
        {
            GPUResources->AdvanceCurrentState(SceneIndex);
            if (SceneIndex < Execution.PendingInputActivation.size())
                Execution.PendingInputActivation[SceneIndex] = false;
        }
        if (!InstanceSteps.empty())
            Execution.bSeedPersistentActivityOnNextStep = false;
        Execution.bTransferWeightSettingsDirty = false;
        // State-driven geometry updates always derive Height after the step. When disabled, several fixed substeps may
        // run; keep the invalidation alive until the final substep actually prepares render height.
        if (Execution.DebugSolverSettings.bStateDrivenGeometryUpdateEnabled || bPrepareRenderHeight)
            Execution.bForceFullGeometryOnNextStep = false;
    }

#pragma endregion
}