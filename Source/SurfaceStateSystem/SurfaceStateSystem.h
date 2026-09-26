/**
 * @file SurfaceStateSystem.h
 * @brief Own Surface GPU resources and record one solver step for a scene.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "SurfaceStateSystem/State/SurfaceStateSolver.h"

#include <memory>

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
        [[nodiscard]] const TSurfaceGPUResourceManager& GetGPUResources() const noexcept;

    private:
        std::unique_ptr<TSurfaceGPUResourceManager> GPUResources;
        std::unique_ptr<TSurfaceStateSolver>        Solver;
    };
} // namespace MDSS
