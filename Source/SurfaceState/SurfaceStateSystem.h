/**
 * @file SurfaceStateSystem.h
 * @brief SurfaceStateSystem의 자료형과 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/GPU/SurfaceGPUResources.h"
#include "SurfaceState/State/SurfaceInput.h"
#include "SurfaceState/State/SurfaceStateSolver.h"

#include <glm/mat3x3.hpp>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace MDSS::Asset
{
    class TAssetManager;
}
namespace MDSS::SurfaceState
{
    class TSurfaceDataManager;
}
namespace MDSS::GPU
{
    class TVulkanContext;
}

namespace MDSS
{
    class TScene;
}

namespace MDSS::SurfaceState
{

    class TSurfaceStateSystem final
    {
    public:
        // Lifecycle
        TSurfaceStateSystem(const GPU::TVulkanContext&  Context,
                            const Asset::TAssetManager& Assets,
                            const TSurfaceDataManager&  SurfaceData,
                            const TScene&               Scene);
        ~TSurfaceStateSystem();

        TSurfaceStateSystem(const TSurfaceStateSystem&) = delete;
        TSurfaceStateSystem& operator=(const TSurfaceStateSystem&) = delete;
        TSurfaceStateSystem(TSurfaceStateSystem&&) = delete;
        TSurfaceStateSystem& operator=(TSurfaceStateSystem&&) = delete;

        /** @brief 같은 Application 소유 객체를 유지한 채 준비된 Scene별 GPU 자원을 교체한다. */
        void ReplaceSceneResources(TSurfaceStateSystem&& Replacement);

        // State and contact management
        void SubmitContact(TSurfaceContactInput Contact);
        void ResetState();
        void RestartState();
        void SetDebugProfileParameters(Asset::TSRProfileAssetHandle   Profile,
                                       TStateId                       State,
                                       const TSurfaceStateParameters& Parameters,
                                       bool                           bKeepRuntimeOverride = true);

        // Simulation and GPU recording
        [[nodiscard]] float GetMaximumStableDeltaTime();
        void PrepareTransferWeightCachesForSettingChange();
        void RecordStep(VkCommandBuffer CommandBuffer,
                        float           DeltaTime,
                        VkQueryPool     TimestampQueryPool = VK_NULL_HANDLE,
                        std::uint32_t   FirstStepQuery = 0,
                        bool            bPrepareRenderHeight = true);

        // Debug controls and resource access
        [[nodiscard]] std::size_t                        GetSolverInstanceCount() const noexcept;
        [[nodiscard]] const TSurfaceSolverDebugSettings& GetDebugSolverSettings() const noexcept;
        void SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept;
        void SetAccumulationGeometryUpdateEnabled(bool bEnabled) noexcept;
        void SetDebugGeometryDriveEnabled(bool bEnabled) noexcept;
        void SetDebugNormalWeightEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept { return bRawFluxCacheEnabled; }
        void SetRawFluxCacheEnabled(bool bEnabled) noexcept { bRawFluxCacheEnabled = bEnabled; }
        [[nodiscard]] bool IsCoalescedRawFluxLayoutEnabled() const noexcept { return bCoalescedRawFluxLayoutEnabled; }
        void SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept { bCoalescedRawFluxLayoutEnabled = bEnabled; }
        [[nodiscard]] bool IsHalfRawFluxCacheEnabled() const noexcept { return bHalfRawFluxCacheEnabled; }
        void SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept { bHalfRawFluxCacheEnabled = bEnabled; }
        [[nodiscard]] bool IsHalfDynamicWeightsEnabled() const noexcept { return bHalfDynamicWeightsEnabled; }
        void SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSolverEnabled() const noexcept { return bSparseSolverEnabled; }
        void SetSparseSolverEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseAccumulationHeightEnabled() const noexcept { return bSparseAccumulationHeightEnabled; }
        void SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept { bSparseAccumulationHeightEnabled = bEnabled; }
        [[nodiscard]] bool IsActiveChannelMaskEnabled() const noexcept { return bActiveChannelMaskEnabled; }
        void SetActiveChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSimulationGeometryEnabled() const noexcept { return bSparseSimulationGeometryEnabled; }
        void SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept { bSparseSimulationGeometryEnabled = bEnabled; }
        [[nodiscard]] const TSurfaceGPUResourceManager& GetGPUResources() const noexcept;

    private:
        // Pending contact application
        void QueueInitialContacts();
        void ApplyPendingContacts();

        const GPU::TVulkanContext&                  Context;
        const Asset::TAssetManager&                 Assets;
        const TSurfaceDataManager&                  SurfaceData;
        const TScene&                               Scene;
        std::unique_ptr<TSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<TSurfaceStateSolver>        Solver;
        std::vector<TSurfaceContactInput>           PendingContacts;
        TSurfaceSolverDebugSettings                 DebugSolverSettings;
        bool                                        bTransferWeightSettingsDirty = false;
        bool                                        bForceFullGeometryOnNextStep = false;
        bool                                        bStableDeltaTimeDirty = true;
        bool                                        bRawFluxCacheEnabled = true;
        bool                                        bCoalescedRawFluxLayoutEnabled = true;
        bool                                        bHalfRawFluxCacheEnabled = false;
        bool                                        bHalfDynamicWeightsEnabled = false;
        bool                                        bSparseSolverEnabled = true;
        bool                                        bSparseAccumulationHeightEnabled = true;
        bool                                        bActiveChannelMaskEnabled = true;
        bool                                        bSparseSimulationGeometryEnabled = true;
        bool                                        bSeedPersistentActivityOnNextStep = true;
        std::vector<bool>                           PendingInputActivation;
        float                                       CachedMaximumStableDeltaTime = 1.0F / 60.0F;
        std::vector<glm::mat3>                      StableDeltaTimeModelMatrices;
        std::map<std::pair<Asset::TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> RuntimeProfileOverrides;
    };
} // namespace MDSS::SurfaceState
