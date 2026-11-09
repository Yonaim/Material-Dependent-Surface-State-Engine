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
// ============================================================
// 00. Lifecycle
// ============================================================
#pragma region 00_Lifecycle

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

#pragma endregion

// ============================================================
// 01. State and contact management
// ============================================================
#pragma region 01_State_and_contact_management

        // State and contact management
        void SubmitContact(FSurfaceContactInput Contact);
        void ResetState();
        void RestartState();
        void SetDebugProfileParameters(Asset::FSRProfileAssetHandle   Profile,
                                       FStateId                       State,
                                       const FSurfaceStateParameters& Parameters,
                                       bool                           bKeepRuntimeOverride = true);

#pragma endregion

// ============================================================
// 02. Simulation and GPU recording
// ============================================================
#pragma region 02_Simulation_and_GPU_recording

        // Simulation and GPU recording
        [[nodiscard]] float GetMaximumStableDeltaTime();
        void                PrepareTransferWeightCachesForSettingChange();
        void                RecordStep(VkCommandBuffer CommandBuffer,
                                       float           DeltaTime,
                                       VkQueryPool     TimestampQueryPool = VK_NULL_HANDLE,
                                       std::uint32_t   FirstStepQuery = 0,
                                       bool            bPrepareRenderHeight = true);

#pragma endregion

// ============================================================
// 03. Debug controls and resource access
// ============================================================
#pragma region 03_Debug_controls_and_resource_access

        // Debug controls and resource access
        [[nodiscard]] std::size_t                        GetSolverInstanceCount() const noexcept;
        [[nodiscard]] const FSurfaceSolverDebugSettings& GetDebugSolverSettings() const noexcept;
        void               SetDebugSolverTermEnabled(ESurfaceSolverTerm Term, bool bEnabled) noexcept;
        void               SetStateDrivenGeometryUpdateEnabled(bool bEnabled) noexcept;
        void               SetDebugGeometryDriveEnabled(bool bEnabled) noexcept;
        void               SetDebugNormalWeightEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept
        {
            return Execution.bRawFluxCacheEnabled;
        }
        void SetRawFluxCacheEnabled(bool bEnabled) noexcept
        {
            Execution.bRawFluxCacheEnabled = bEnabled;
        }
        [[nodiscard]] bool IsCoalescedRawFluxLayoutEnabled() const noexcept
        {
            return Execution.bCoalescedRawFluxLayoutEnabled;
        }
        void SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept
        {
            Execution.bCoalescedRawFluxLayoutEnabled = bEnabled;
        }
        [[nodiscard]] bool IsHalfRawFluxCacheEnabled() const noexcept
        {
            return Execution.bHalfRawFluxCacheEnabled;
        }
        void SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept
        {
            Execution.bHalfRawFluxCacheEnabled = bEnabled;
        }
        [[nodiscard]] bool IsHalfDynamicWeightsEnabled() const noexcept
        {
            return Execution.bHalfDynamicWeightsEnabled;
        }
        void SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept;
        void TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay)
        {
            if (GPUResources)
                GPUResources->TrimRenderMeshVariants(Surface, Overlay);
        }
        void SetAdaptiveGeometryEnabled(bool bEnabled) noexcept
        {
            Execution.bAdaptiveGeometryEnabled = bEnabled;
        }
        [[nodiscard]] bool IsAdaptiveGeometryEnabled() const noexcept
        {
            return Execution.bAdaptiveGeometryEnabled;
        }
        void PrepareScratchStorage()
        {
            if (GPUResources && GPUResources->ConfigureScratchStorage(Context,
                                                                      Execution.bRawFluxCacheEnabled,
                                                                      Execution.bHalfRawFluxCacheEnabled,
                                                                      Execution.bHalfDynamicWeightsEnabled,
                                                                      Execution.bCompactScratchStorageEnabled))
            {
                Execution.bTransferWeightSettingsDirty = true;
                Execution.bForceFullGeometryOnNextStep = true;
            }
        }
        void SetCompactScratchStorageEnabled(bool bEnabled) noexcept
        {
            Execution.bCompactScratchStorageEnabled = bEnabled;
        }
        [[nodiscard]] bool IsCompactScratchStorageEnabled() const noexcept
        {
            return Execution.bCompactScratchStorageEnabled;
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
            return Execution.bSparseSolverEnabled;
        }
        void               SetSparseSolverEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseAccumulationHeightEnabled() const noexcept
        {
            return Execution.bSparseAccumulationHeightEnabled;
        }
        void SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept
        {
            Execution.bSparseAccumulationHeightEnabled = bEnabled;
        }
        [[nodiscard]] bool IsActiveChannelMaskEnabled() const noexcept
        {
            return Execution.bActiveChannelMaskEnabled;
        }
        void               SetActiveChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsPerWorkgroupChannelMaskEnabled() const noexcept
        {
            return Execution.bPerWorkgroupChannelMaskEnabled;
        }
        void               SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept;
        [[nodiscard]] bool IsSparseSimulationGeometryEnabled() const noexcept
        {
            return Execution.bSparseSimulationGeometryEnabled;
        }
        void SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept
        {
            Execution.bSparseSimulationGeometryEnabled = bEnabled;
        }
        [[nodiscard]] const FSurfaceGPUResourceManager& GetGPUResources() const noexcept;
        [[nodiscard]] FSurfaceGPUBufferPool&            GetReusableBufferPool() noexcept
        {
            return BufferPool;
        }

    private:
#pragma endregion

// ============================================================
// 04. Pending contact application
// ============================================================
#pragma region 04_Pending_contact_application

        // Pending contact application
        void QueueInitialContacts();
        void ApplyPendingContacts();

#pragma endregion

// ============================================================
// 05. Resource Ownership and Runtime State
// ============================================================
#pragma region 05_Resource_Ownership_and_Runtime_State

        const GPU::FVulkanContext&                  Context;
        const Asset::FAssetManager&                 Assets;
        const FSurfaceDataManager&                  SurfaceData;
        const FScene&                               Scene;
        FSurfaceGPUBufferPool                       BufferPool;
        std::unique_ptr<FSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<FSurfaceStateSolver>        Solver;
        std::vector<FSurfaceContactInput>           PendingContacts;
        struct FSolverExecutionState
        {
            FSurfaceSolverDebugSettings DebugSolverSettings;

            bool                   bTransferWeightSettingsDirty = false;
            bool                   bForceFullGeometryOnNextStep = false;
            bool                   bStableDeltaTimeDirty = true;
            bool                   bAdaptiveGeometryEnabled = false;
            bool                   bCompactScratchStorageEnabled = true;
            bool                   bRawFluxCacheEnabled = true;
            bool                   bCoalescedRawFluxLayoutEnabled = true;
            bool                   bHalfRawFluxCacheEnabled = true;
            bool                   bHalfDynamicWeightsEnabled = false;
            bool                   bSparseSolverEnabled = false;
            bool                   bSparseAccumulationHeightEnabled = true;
            bool                   bActiveChannelMaskEnabled = true;
            bool                   bPerWorkgroupChannelMaskEnabled = true;
            bool                   bSparseSimulationGeometryEnabled = true;
            bool                   bSeedPersistentActivityOnNextStep = true;
            std::vector<bool>      PendingInputActivation;
            float                  CachedMaximumStableDeltaTime = 1.0F / 60.0F;
            std::vector<glm::mat3> StableDeltaTimeModelMatrices;
        };
        FSolverExecutionState                                                                Execution;
        std::map<std::pair<Asset::FSRProfileAssetHandle, FStateId>, FSurfaceStateParameters> RuntimeProfileOverrides;

#pragma endregion
    };
} // namespace MDSS::SurfaceState
