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
#include <functional>
#include <map>
#include <optional>
#include <glm/mat4x4.hpp>
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
    class FSharedSurfaceGeometryData;
    struct FSurfaceTexelGeometry;
}

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    class FScene;
    class FStaticMeshInstance;
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
// 객체 초기화와 종료에서 소유 자원·참조의 유효 기간을 맞춘다.

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
// State 초기화·접촉 예약과 일회성 초기 입력 관찰자를 제어한다.

        // State and contact management
        void SubmitContact(FSurfaceContactInput Contact);
        void ResetState();
        void RestartState();

        /**
         * @brief 다음 RecordStep의 CPU contact 적용 후 Solver dispatch 전에 실행할 일회성 관찰자를 설정한다.
         * @param Observer 호출 스레드에서 동기 실행되는 읽기용 callback. 비어 있으면 예약을 취소한다.
         * @note callback이 참조하는 객체는 호출 시점까지 유효해야 한다. 실행 전에 예약을 해제한다.
         */
        void SetInitialInputReadbackObserver(std::function<void()> Observer)
        {
            InitialInputReadbackObserver = std::move(Observer);
        }

        void SetDebugProfileParameters(Asset::FSRProfileAssetHandle   Profile,
                                       FStateId                       State,
                                       const FSurfaceStateParameters& Parameters,
                                       bool                           bKeepRuntimeOverride = true);

#pragma endregion

// ============================================================
// 02. Simulation and GPU recording
// ============================================================
#pragma region 02_Simulation_and_GPU_recording
// 프레임이 요청한 시간 진행과 Solver 명령 기록을 연결한다.

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
// debug 실행 옵션과 관련 GPU 자원을 조회·제어한다.

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
// 다음 Solver에 전달할 접촉 입력을 모아 rasterization 경계를 관리한다.

        // Pending contact application
        void QueueInitialContacts();
        void ApplyPendingContacts();

        struct FContactInputBatch
        {
            std::size_t InstanceCount = 0;
            std::size_t ChannelCount = 0;
            std::vector<std::vector<float>> InputDeltas;
            std::vector<std::vector<std::uint8_t>> InputGroupFlags;
            std::vector<std::vector<std::uint32_t>> InputGroupMasks;
            std::vector<std::uint32_t> InputChannelMasks;
            std::vector<bool> bInstanceHasInput;
        };

        void RasterizePendingContact(const FSurfaceContactInput& Contact, FContactInputBatch& Batch);
        void RasterizeValidatedContact(const FSurfaceContactInput& Contact, FContactInputBatch& Batch);
        [[nodiscard]] std::optional<glm::vec3> ResolveMappedContactInfluenceCenter(
            const FSurfaceContactInput& Contact, const FStaticMeshInstance& Instance,
            const FSharedSurfaceGeometryData& Geometry, const glm::mat4& Model,
            const std::vector<FSurfaceTexelGeometry>& Texels) const;
        void UploadRasterizedContacts(const FContactInputBatch& Batch);

#pragma endregion

// ============================================================
// 05. Resource Ownership and Runtime State
// ============================================================
#pragma region 05_Resource_Ownership_and_Runtime_State
// GPU 소유 객체와 실행 cache를 분리해 시스템 수명에 연결한다.

        std::function<void()>                       InitialInputReadbackObserver;
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
