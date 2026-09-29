/**
 * @file SurfaceStateSystem.h
 * @brief Own Surface GPU resources and record one solver step for a scene.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "SurfaceStateSystem/State/SurfaceInput.h"
#include "SurfaceStateSystem/State/SurfaceStateSolver.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace MDSS
{
    class TAssetManager;
    class TScene;
    class TVulkanContext;

    class TSurfaceStateSystem final
    {
    public:
        TSurfaceStateSystem(const TVulkanContext& Context, const TAssetManager& Assets, const TScene& Scene);
        ~TSurfaceStateSystem();

        TSurfaceStateSystem(const TSurfaceStateSystem&) = delete;
        TSurfaceStateSystem& operator=(const TSurfaceStateSystem&) = delete;
        TSurfaceStateSystem(TSurfaceStateSystem&&) = delete;
        TSurfaceStateSystem& operator=(TSurfaceStateSystem&&) = delete;

        void RecordStep(VkCommandBuffer CommandBuffer,
                        float DeltaTime,
                        VkQueryPool TimestampQueryPool = VK_NULL_HANDLE,
                        std::uint32_t FirstInstanceQuery = 0);
        [[nodiscard]] std::size_t GetSolverTimestampSlotCount() const noexcept;
        void ResetState();
        [[nodiscard]] float GetMaximumStableDeltaTime();
        [[nodiscard]] const TSurfaceSolverDebugSettings& GetDebugSolverSettings() const noexcept;
        void SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept;
        void SetRawFluxCacheEnabled(bool bEnabled) noexcept;
        void SetDebugGeometryDriveEnabled(bool bEnabled) noexcept;
        void SetDebugNormalWeightEnabled(bool bEnabled) noexcept;
        void SubmitContact(TSurfaceContactInput Contact);
        void SetDebugProfileParameters(TSRProfileAssetHandle Profile,
                                       TStateId State,
                                       const TSurfaceStateParameters& Parameters,
                                       bool bKeepRuntimeOverride = true);
        [[nodiscard]] const TSurfaceGPUResourceManager& GetGPUResources() const noexcept;

    private:
        void ApplyPendingContacts();

        const TVulkanContext& Context;
        const TAssetManager& Assets;
        const TScene& Scene;
        std::unique_ptr<TSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<TSurfaceStateSolver>        Solver;
        std::vector<TSurfaceContactInput>            PendingContacts;
        TSurfaceSolverDebugSettings DebugSolverSettings;
        bool bTransferWeightSettingsDirty = false;
        bool bStableDeltaTimeDirty = true;
        float CachedMaximumStableDeltaTime = 1.0F / 60.0F;
        std::map<std::pair<TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> RuntimeProfileOverrides;
    };
} // namespace MDSS
