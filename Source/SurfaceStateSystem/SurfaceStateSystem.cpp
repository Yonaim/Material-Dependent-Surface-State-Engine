/**
 * @file SurfaceStateSystem.cpp
 * @brief Scene-level owner and step recorder for Surface State simulation.
 */

#include "SurfaceStateSystem/SurfaceStateSystem.h"

#include "AssetManager/Core/AssetManager.h"
#include "Scene/Scene.h"
#include "VulkanContext/VulkanContext.h"

namespace MDSS
{
    TSurfaceStateSystem::TSurfaceStateSystem(const TVulkanContext& Context,
                                             const TAssetManager& Assets,
                                             const TScene& Scene)
        : GPUResources(std::make_unique<TSurfaceGPUResourceManager>(Context, Assets, Scene))
    {
        if (const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetAnyInstanceDescriptors())
        {
            Solver = std::make_unique<TSurfaceStateSolver>(Context.GetDevice(), Descriptors->GetLayout());
        }
    }

    TSurfaceStateSystem::~TSurfaceStateSystem() = default;

    void TSurfaceStateSystem::RecordStep(VkCommandBuffer CommandBuffer, float DeltaTime)
    {
        if (!Solver)
        {
            return;
        }

        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            const TSurfaceStateDescriptorResources* Descriptors = GPUResources->GetInstanceDescriptors(SceneIndex);
            if (Descriptors == nullptr)
            {
                continue;
            }

            const bool bCurrentStateAB = GPUResources->IsCurrentStateAB(SceneIndex);
            Solver->RecordStep(CommandBuffer,
                               *Descriptors,
                               bCurrentStateAB,
                               GPUResources->GetInstanceTexelCount(SceneIndex),
                               GPUResources->GetInstanceChannelCount(SceneIndex),
                               DeltaTime);
            GPUResources->AdvanceCurrentState(SceneIndex);
        }
    }

    const TSurfaceGPUResourceManager& TSurfaceStateSystem::GetGPUResources() const noexcept
    {
        return *GPUResources;
    }
} // namespace MDSS
