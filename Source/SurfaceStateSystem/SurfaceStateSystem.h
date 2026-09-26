/**
 * @file SurfaceStateSystem.h
 * @brief Own Surface GPU resources and record one solver step for a scene.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "SurfaceStateSystem/State/SurfaceInput.h"
#include "SurfaceStateSystem/State/SurfaceStateSolver.h"

#include <memory>
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

        void RecordStep(VkCommandBuffer CommandBuffer, float DeltaTime);
        void SubmitContact(TSurfaceContactInput Contact);
        [[nodiscard]] const TSurfaceGPUResourceManager& GetGPUResources() const noexcept;

    private:
        void ApplyPendingContacts();

        const TVulkanContext& Context;
        const TAssetManager& Assets;
        const TScene& Scene;
        std::unique_ptr<TSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<TSurfaceStateSolver>        Solver;
        std::vector<TSurfaceContactInput>            PendingContacts;
    };
} // namespace MDSS
