/**
 * @file SurfaceStateSystem.h
 * @brief SurfaceStateSystem의 자료형과 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"
#include "SurfaceState/Solver/SurfaceStateSolver.h"
#include "SurfaceState/State/SurfaceInput.h"

#include <glm/mat3x3.hpp>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace MDSS::Asset
{
    class FAssetManager;
}
namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
}
namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    class FScene;
}

namespace MDSS::SurfaceState
{

    class FSurfaceStateSystem final
    {
    public:
        // Lifecycle
        FSurfaceStateSystem(const GPU::FVulkanContext&  Context,
                            const Asset::FAssetManager& Assets,
                            const FSurfaceDataManager&  SurfaceData,
                            const FScene&               Scene,
                            FSurfaceGPUBufferPool*      ReusePool = nullptr);
        ~FSurfaceStateSystem();

        FSurfaceStateSystem(const FSurfaceStateSystem&) = delete;
        FSurfaceStateSystem& operator=(const FSurfaceStateSystem&) = delete;
        FSurfaceStateSystem(FSurfaceStateSystem&&) = delete;
        FSurfaceStateSystem& operator=(FSurfaceStateSystem&&) = delete;

        /** @brief 같은 Application 소유 객체를 유지한 채 준비된 Scene별 GPU 자원을 교체한다. */
        void ReplaceSceneResources(FSurfaceStateSystem&& Replacement);

        // State and contact management
        void SubmitContact(FSurfaceContactInput Contact);
        void ResetState();
        void RestartState();
        void SetDebugProfileParameters(Asset::FSRProfileAssetHandle   Profile,
                                       FStateId                       State,
                                       const FSurfaceStateParameters& Parameters,
                                       bool                           bKeepRuntimeOverride = true);

        // Simulation and GPU recording
        [[nodiscard]] float GetMaximumStableDeltaTime();
        void                PrepareTransferWeightCachesForSettingChange();
        void                RecordStep(VkCommandBuffer CommandBuffer,
                                       float           DeltaTime,
                                       VkQueryPool     TimestampQueryPool = VK_NULL_HANDLE,
                                       std::uint32_t   FirstStepQuery = 0,
                                       bool            bPrepareRenderHeight = true);

        // Debug controls and resource access
        [[nodiscard]] std::size_t                        GetSolverInstanceCount() const noexcept;
        [[nodiscard]] const FSurfaceSolverDebugSettings& GetDebugSolverSettings() const noexcept;
        void               SetDebugSolverTermEnabled(ESurfaceSolverTerm Term, bool bEnabled) noexcept;
        void               SetStateDrivenGeometryUpdateEnabled(bool bEnabled) noexcept;
        void               SetDebugGeometryDriveEnabled(bool bEnabled) noexcept;
        void               SetDebugNormalWeightEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept
        {
            return bRawFluxCacheEnabled;
        }
        void SetRawFluxCacheEnabled(bool bEnabled) noexcept
        {
            bRawFluxCacheEnabled = bEnabled;
        }
        [[nodiscard]] bool IsCoalescedRawFluxLayoutEnabled() const noexcept
        {
            return bCoalescedRawFluxLayoutEnabled;
        }
        void SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept
        {
            bCoalescedRawFluxLayoutEnabled = bEnabled;
        }
        [[nodiscard]] bool IsHalfRawFluxCacheEnabled() const noexcept
        {
            return bHalfRawFluxCacheEnabled;
        }
        void SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept
        {
            bHalfRawFluxCacheEnabled = bEnabled;
        }
        [[nodiscard]] bool IsHalfDynamicWeightsEnabled() const noexcept
        {
            return bHalfDynamicWeightsEnabled;
        }
        void SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept;
        void TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay)
        {
            if (GPUResources)
                GPUResources->TrimRenderMeshVariants(Surface, Overlay);
        }
        void SetAdaptiveGeometryEnabled(bool bEnabled) noexcept
        {
            bAdaptiveGeometryEnabled = bEnabled;
        }
        [[nodiscard]] bool IsAdaptiveGeometryEnabled() const noexcept
        {
            return bAdaptiveGeometryEnabled;
        }
        void PrepareScratchStorage()
        {
            if (GPUResources && GPUResources->ConfigureScratchStorage(Context,
                                                                      bRawFluxCacheEnabled,
                                                                      bHalfRawFluxCacheEnabled,
                                                                      bHalfDynamicWeightsEnabled,
                                                                      bCompactScratchStorageEnabled))
            {
                bTransferWeightSettingsDirty = true;
                bForceFullGeometryOnNextStep = true;
            }
        }
        void SetCompactScratchStorageEnabled(bool bEnabled) noexcept
        {
            bCompactScratchStorageEnabled = bEnabled;
        }
        [[nodiscard]] bool IsCompactScratchStorageEnabled() const noexcept
        {
            return bCompactScratchStorageEnabled;
        }
        void SetRuntimeSolverFlagsEnabled(bool bEnabled) noexcept
        {
            if (Solver)
                Solver->SetRuntimeFlagsEnabled(bEnabled);
        }
        [[nodiscard]] bool IsRuntimeSolverFlagsEnabled() const noexcept
        {
            return Solver && Solver->IsRuntimeFlagsEnabled();
        }
        [[nodiscard]] bool IsSparseSolverEnabled() const noexcept
        {
            return bSparseSolverEnabled;
        }
        void               SetSparseSolverEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseAccumulationHeightEnabled() const noexcept
        {
            return bSparseAccumulationHeightEnabled;
        }
        void SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept
        {
            bSparseAccumulationHeightEnabled = bEnabled;
        }
        [[nodiscard]] bool IsActiveChannelMaskEnabled() const noexcept
        {
            return bActiveChannelMaskEnabled;
        }
        void               SetActiveChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsPerWorkgroupChannelMaskEnabled() const noexcept
        {
            return bPerWorkgroupChannelMaskEnabled;
        }
        void               SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSimulationGeometryEnabled() const noexcept
        {
            return bSparseSimulationGeometryEnabled;
        }
        void SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept
        {
            bSparseSimulationGeometryEnabled = bEnabled;
        }
        [[nodiscard]] const FSurfaceGPUResourceManager& GetGPUResources() const noexcept;
        [[nodiscard]] FSurfaceGPUBufferPool&            GetReusableBufferPool() noexcept
        {
            return BufferPool;
        }

    private:
        // Pending contact application
        void QueueInitialContacts();
        void ApplyPendingContacts();

        const GPU::FVulkanContext&                  Context;
        const Asset::FAssetManager&                 Assets;
        const FSurfaceDataManager&                  SurfaceData;
        const FScene&                               Scene;
        FSurfaceGPUBufferPool                       BufferPool;
        std::unique_ptr<FSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<FSurfaceStateSolver>        Solver;
        std::vector<FSurfaceContactInput>           PendingContacts;
        FSurfaceSolverDebugSettings                 DebugSolverSettings;
        bool                                        bTransferWeightSettingsDirty = false;
        bool                                        bForceFullGeometryOnNextStep = false;
        bool                                        bStableDeltaTimeDirty = true;
        bool                                        bAdaptiveGeometryEnabled = false;
        bool                                        bCompactScratchStorageEnabled = true;
        bool                                        bRawFluxCacheEnabled = true;
        bool                                        bCoalescedRawFluxLayoutEnabled = true;
        bool                                        bHalfRawFluxCacheEnabled = true;
        bool                                        bHalfDynamicWeightsEnabled = false;
        bool                                        bSparseSolverEnabled = false;
        bool                                        bSparseAccumulationHeightEnabled = true;
        bool                                        bActiveChannelMaskEnabled = true;
        bool                                        bPerWorkgroupChannelMaskEnabled = true;
        bool                                        bSparseSimulationGeometryEnabled = true;
        bool                                        bSeedPersistentActivityOnNextStep = true;
        std::vector<bool>                           PendingInputActivation;
        float                                       CachedMaximumStableDeltaTime = 1.0F / 60.0F;
        std::vector<glm::mat3>                      StableDeltaTimeModelMatrices;
        std::map<std::pair<Asset::FSRProfileAssetHandle, FStateId>, FSurfaceStateParameters> RuntimeProfileOverrides;
    };
} // namespace MDSS::SurfaceState
