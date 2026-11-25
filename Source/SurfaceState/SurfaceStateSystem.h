/**
 * @file SurfaceStateSystem.h
 * @brief SurfaceStateSystem의 자료형과 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h"
#include "SurfaceState/GPU/Resources/SurfaceGPUResources.h"
#include "SurfaceState/State/SurfaceInput.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
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
    enum class ESurfaceSolverTerm : std::uint8_t
    {
        SaturationDrive,
        GeometryDrive,
        Decay,
        ConcavityRetention,
        DistanceWeight,
        NormalWeight,
        ProfileBoundaryWeight,
        MesoDirectionNormal,
        Count
    };

    struct FSurfaceSolverDebugSettings
    {
        bool bStateDrivenGeometryUpdateEnabled = true;
        std::array<bool, static_cast<std::size_t>(ESurfaceSolverTerm::Count)>
            Enabled{true, true, true, true, true, true, true, true};

        [[nodiscard]] bool IsEnabled(ESurfaceSolverTerm Term) const noexcept
        {
            return Enabled[static_cast<std::size_t>(Term)];
        }

        void SetEnabled(ESurfaceSolverTerm Term, bool bEnabled) noexcept
        {
            Enabled[static_cast<std::size_t>(Term)] = bEnabled;
        }
    };

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
// 프레임이 요청한 시간 진행과 Solver 명령 기록을 연결한다.

        // Simulation and GPU recording
        [[nodiscard]] float GetMaximumStableDeltaTime();
        void                PrepareTransferWeightCachesForSettingChange();

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
#pragma endregion

// ============================================================
// 05. Resource Ownership and Runtime State
// ============================================================
#pragma region 05_Resource_Ownership_and_Runtime_State
// GPU 소유 객체와 실행 cache를 분리해 시스템 수명에 연결한다.

        const GPU::FVulkanContext                  &Context;
        const Asset::FAssetManager                 &Assets;
        const FSurfaceDataManager                  &SurfaceData;
        const FScene                               &Scene;
        FSurfaceGPUBufferPool                       BufferPool;
        std::unique_ptr<FSurfaceGPUResourceManager> GPUResources;
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
            float                  CachedMaximumStableDeltaTime = 1.0F / 60.0F;
            std::vector<glm::mat3> StableDeltaTimeModelMatrices;
        };
        FSolverExecutionState                                                                Execution;
        std::map<std::pair<Asset::FSRProfileAssetHandle, FStateId>, FSurfaceStateParameters> RuntimeProfileOverrides;

#pragma endregion
    };
} // namespace MDSS::SurfaceState
