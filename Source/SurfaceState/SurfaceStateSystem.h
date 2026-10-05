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
        void RecordStep(VkCommandBuffer CommandBuffer,
                        float           DeltaTime,
                        VkQueryPool     TimestampQueryPool = VK_NULL_HANDLE,
                        std::uint32_t   FirstStepQuery = 0);
        void RecordCurrentAccumulationHeight(VkCommandBuffer CommandBuffer, std::size_t SceneIndex);
        void PrepareTransferWeightCachesForSettingChange();
        [[nodiscard]] std::size_t                        GetSolverInstanceCount() const noexcept;
        void                                             ResetState();
        void                                             RestartState();
        [[nodiscard]] float                              GetMaximumStableDeltaTime();
        [[nodiscard]] const TSurfaceSolverDebugSettings& GetDebugSolverSettings() const noexcept;
        void SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled) noexcept;
        void SetAccumulationGeometryUpdateEnabled(bool bEnabled) noexcept;
        void SetDebugGeometryDriveEnabled(bool bEnabled) noexcept;
        void SetDebugNormalWeightEnabled(bool bEnabled) noexcept;
        void SubmitContact(TSurfaceContactInput Contact);
        void SetDebugProfileParameters(Asset::TSRProfileAssetHandle   Profile,
                                       TStateId                       State,
                                       const TSurfaceStateParameters& Parameters,
                                       bool                           bKeepRuntimeOverride = true);
        [[nodiscard]] const TSurfaceGPUResourceManager& GetGPUResources() const noexcept;

    private:
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
        float                                       CachedMaximumStableDeltaTime = 1.0F / 60.0F;
        std::vector<glm::mat3>                      StableDeltaTimeModelMatrices;
        std::map<std::pair<Asset::TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> RuntimeProfileOverrides;
    };
} // namespace MDSS::SurfaceState
