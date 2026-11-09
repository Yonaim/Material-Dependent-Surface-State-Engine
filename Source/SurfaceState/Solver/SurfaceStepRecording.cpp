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

    void TSurfaceStateSystem::RecordStep(VkCommandBuffer CommandBuffer,
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

        bool bHasDirtyTransferWeightCache = bTransferWeightSettingsDirty;
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

        std::vector<TSurfaceSolverInstanceStep> InstanceSteps;
        std::vector<std::size_t>                ActiveSceneIndices;
        InstanceSteps.reserve(GPUResources->GetSceneInstanceCount());
        ActiveSceneIndices.reserve(GPUResources->GetSceneInstanceCount());
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(SceneIndex);
            if (Descriptors == nullptr)
            {
                continue;
            }

            const bool                 bCurrentStateAB = GPUResources->IsCurrentStateAB(SceneIndex);
            const TStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[SceneIndex];
            const glm::mat4            ModelMatrix = Instance.GetTransform().GetMatrix();
            const bool                 bRebuildStaticWeights =
                bTransferWeightSettingsDirty ||
                GPUResources->NeedsTransferWeightCacheUpdate(SceneIndex, Instance.GetTransform());
            if (bRebuildStaticWeights)
            {
                GPUResources->UpdateTransferWeightCache(
                    SceneIndex,
                    Instance.GetTransform(),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight),
                    DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight),
                    bHalfDynamicWeightsEnabled);
            }
            const glm::vec3 GravityWorld(0.0F, 0.0F, -1.0F);
            std::uint32_t   SolverFlags = 0U;
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::GeometryDrive))
            {
                SolverFlags |= 1U << 0U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::SaturationDrive))
            {
                SolverFlags |= 1U << 1U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::Decay))
            {
                SolverFlags |= 1U << 2U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ConcavityRetention))
            {
                SolverFlags |= 1U << 3U;
            }
            if (!DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::MesoDirectionNormal))
            {
                SolverFlags |= 1U << 4U;
            }
            if (bRawFluxCacheEnabled)
            {
                SolverFlags |= SurfaceSolverRawFluxCacheFlag;
                if (bCoalescedRawFluxLayoutEnabled)
                    SolverFlags |= SurfaceSolverCoalescedRawFluxLayoutFlag;
                if (bHalfRawFluxCacheEnabled)
                    SolverFlags |= SurfaceSolverHalfRawFluxCacheFlag;
            }
            if (bHalfDynamicWeightsEnabled)
                SolverFlags |= SurfaceSolverHalfDynamicWeightsFlag;
            if (bSeedPersistentActivityOnNextStep &&
                (bSparseSolverEnabled || bActiveChannelMaskEnabled || bPerWorkgroupChannelMaskEnabled))
                SolverFlags |= SurfaceSolverSeedPersistentActivityFlag;
            if (bSparseSolverEnabled)
                SolverFlags |= SurfaceSolverSparseSolverFlag;
            if (bSparseAccumulationHeightEnabled)
                SolverFlags |= SurfaceSolverSparseAccumulationHeightFlag;
            if (bActiveChannelMaskEnabled)
                SolverFlags |= SurfaceSolverActiveChannelMaskFlag;
            if (bPerWorkgroupChannelMaskEnabled)
                SolverFlags |= SurfaceSolverPerWorkgroupChannelMaskFlag;
            if (bSparseSimulationGeometryEnabled)
                SolverFlags |= SurfaceSolverSparseGeometryFlag;
            if (bAdaptiveGeometryEnabled)
                SolverFlags |= SurfaceSolverAdaptiveGeometryFlag;

            // Height validity is independent of state-driven geometry updates. If updates are OFF the full rebuild
            // is deferred until the final render-height step instead of adding revision/cache bookkeeping.
            if (bRebuildStaticWeights || bForceFullGeometryOnNextStep)
                SolverFlags |= SurfaceSolverForceFullGeometryFlag;

            if (DebugSolverSettings.bStateDrivenGeometryUpdateEnabled)
            {
                SolverFlags |= SurfaceSolverStateDrivenGeometryUpdateFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::DistanceWeight))
                    SolverFlags |= SurfaceSolverDistanceWeightFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::NormalWeight))
                    SolverFlags |= SurfaceSolverNormalWeightFlag;
                if (DebugSolverSettings.IsEnabled(TSurfaceSolverTerm::ProfileBoundaryWeight))
                    SolverFlags |= SurfaceSolverProfileBoundaryWeightFlag;
            }
            TSurfaceSolverInstanceStep Step;
            Step.Descriptors = Descriptors;
            Step.bCurrentStateAB = bCurrentStateAB;
            Step.TexelCount = GPUResources->GetInstanceTexelCount(SceneIndex);
            Step.ChannelCount = GPUResources->GetInstanceChannelCount(SceneIndex);
            Step.DeltaTime = DeltaTime;
            Step.ModelMatrix = ModelMatrix;
            Step.GravityWorld = GravityWorld;
            Step.SolverFlags = SolverFlags;
            Step.bPrepareAccumulationHeight = bPrepareRenderHeight;
            Step.bHasInputActivation = SceneIndex < PendingInputActivation.size() && PendingInputActivation[SceneIndex];
            InstanceSteps.push_back(Step);
            ActiveSceneIndices.push_back(SceneIndex);
        }
        Solver->RecordSteps(CommandBuffer, InstanceSteps, TimestampQueryPool, FirstStepQuery);
        for (const std::size_t SceneIndex : ActiveSceneIndices)
        {
            GPUResources->AdvanceCurrentState(SceneIndex);
            if (SceneIndex < PendingInputActivation.size())
                PendingInputActivation[SceneIndex] = false;
        }
        if (!InstanceSteps.empty())
            bSeedPersistentActivityOnNextStep = false;
        bTransferWeightSettingsDirty = false;
        // State-driven geometry updates always derive Height after the step. When disabled, several fixed substeps may
        // run; keep the invalidation alive until the final substep actually prepares render height.
        if (DebugSolverSettings.bStateDrivenGeometryUpdateEnabled || bPrepareRenderHeight)
            bForceFullGeometryOnNextStep = false;
    }

#pragma endregion
}