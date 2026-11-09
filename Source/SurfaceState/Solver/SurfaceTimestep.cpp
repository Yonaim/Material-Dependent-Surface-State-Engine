/**
 * @file SurfaceStateSystem.cpp
 * @brief SurfaceStateSystem의 구현을 제공한다.
 */

#include "AssetManager/Assets/MeshAsset.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"
#include "SurfaceState/SurfaceStateSystem.h"
#include "SurfaceState/Types/SurfaceSolverRates.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Timestep and Transfer Caches
// ============================================================
#pragma region 00_Timestep_and_Transfer_Caches

    float FSurfaceStateSystem::GetMaximumStableDeltaTime()
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        if (Execution.StableDeltaTimeModelMatrices.size() != Instances.size())
            Execution.bStableDeltaTimeDirty = true;
        for (std::size_t I = 0; I < Instances.size(); ++I)
        {
            if (!GPUResources->GetInstanceDescriptors(I) || I >= Execution.StableDeltaTimeModelMatrices.size())
                continue;
            const glm::mat3 Model(Instances[I].GetTransform().GetMatrix());
            for (glm::length_t Column = 0; Column < 3; ++Column)
                for (glm::length_t Row = 0; Row < 3; ++Row)
                    if (Execution.StableDeltaTimeModelMatrices[I][Column][Row] != Model[Column][Row])
                        Execution.bStableDeltaTimeDirty = true;
        }
        if (!Execution.bStableDeltaTimeDirty)
            return Execution.CachedMaximumStableDeltaTime;

        double     Limit = 1.0 / 60.0;
        const bool bSatEnabled = Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::SaturationDrive);
        const bool bGeoEnabled = Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::GeometryDrive);
        for (std::size_t I = 0; I < Instances.size(); ++I)
        {
            if (!GPUResources->GetInstanceDescriptors(I))
                continue;
            const auto      Handle = Instances[I].GetSurfaceData();
            const auto&     Geometry = *SurfaceData.GetSurfaceData(Handle).GetSharedGeometry();
            const auto&     Texels = Geometry.GetTexels();
            const auto&     Profiles = SurfaceData.GetSurfaceProfileTable(Handle);
            const glm::mat4 Model = Instances[I].GetTransform().GetMatrix();
            const glm::mat3 Linear(Model);
            const auto      Weights = BuildSurfaceGPUTransferWeights(
                Geometry,
                Model,
                nullptr,
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::NormalWeight),
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::DistanceWeight),
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::ProfileBoundaryWeight));
            for (std::size_t T = 0; T < Texels.size(); ++T)
            {
                const auto Profile = Geometry.GetProfileIndex(static_cast<FLocalTexelIndex>(T));
                if (!Texels[T].IsValid() || Profile >= Profiles.size())
                    continue;
                const double AreaScale = GetSurfaceWorldTexelArea(Texels[T], Model) / SurfaceStateReferenceArea;
                if (AreaScale <= 0.0)
                    continue;
                double          WeightSum = 0.0, HeightWeightSum = 0.0;
                const glm::vec3 Position = Texels[T].Position + Texels[T].Normal * Texels[T].Geometry.MesoVirtualHeight;
                for (std::size_t D = 0; D < SurfaceNeighborCount; ++D)
                {
                    const auto N = Texels[T].NeighborIndices[D];
                    if (N >= Texels.size())
                        continue;
                    const double W = Weights[T * SurfaceNeighborCount + D];
                    WeightSum += W;
                    const glm::vec3 Target =
                        Texels[N].Position + Texels[N].Normal * Texels[N].Geometry.MesoVirtualHeight;
                    // DirectionDrive <= 1; use an upper bound including both gravity directions.
                    HeightWeightSum += W * std::abs((Linear * (Target - Position)).z);
                }
                for (const auto& [Name, Original] : Assets.GetSRProfile(Profiles[Profile]).GetData().States)
                {
                    const FStateId State = SurfaceData.GetSurfaceStateRegistry().GetStateId(Name);
                    const auto     Override = RuntimeProfileOverrides.find({Profiles[Profile], State});
                    const auto&    P = Override == RuntimeProfileOverrides.end() ? Original : Override->second;
                    // SaturationDrive <= source saturation; Geometry uses the same unclamped mobility.
                    const double RateBound =
                        (bSatEnabled ? BaseSaturationTransferRate * P.SaturationTransferFactor * WeightSum : 0.0) +
                        (bGeoEnabled ? BaseGeometryTransferRate * P.GeometryTransferFactor * HeightWeightSum : 0.0);
                    if (RateBound > 0.0)
                        Limit = std::min(Limit, 0.9 * P.StateCapacity * AreaScale / RateBound);
                }
            }
        }
        Execution.CachedMaximumStableDeltaTime = static_cast<float>(Limit);
        Execution.StableDeltaTimeModelMatrices.resize(Instances.size());
        for (std::size_t I = 0; I < Instances.size(); ++I)
            Execution.StableDeltaTimeModelMatrices[I] = glm::mat3(Instances[I].GetTransform().GetMatrix());
        Execution.bStableDeltaTimeDirty = false;
        return Execution.CachedMaximumStableDeltaTime;
    }

    void FSurfaceStateSystem::PrepareTransferWeightCachesForSettingChange()
    {
        if (!Execution.bTransferWeightSettingsDirty)
            return;
        if (vkQueueWaitIdle(Context.GetQueues().GetGraphics()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for the graphics queue before preparing TransferWeight caches.");
        for (std::size_t SceneIndex = 0; SceneIndex < GPUResources->GetSceneInstanceCount(); ++SceneIndex)
        {
            if (GPUResources->GetInstanceDescriptors(SceneIndex) == nullptr)
                continue;
            GPUResources->UpdateTransferWeightCache(
                SceneIndex,
                Scene.GetStaticMeshInstances()[SceneIndex].GetTransform(),
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::NormalWeight),
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::DistanceWeight),
                Execution.DebugSolverSettings.IsEnabled(ESurfaceSolverTerm::ProfileBoundaryWeight),
                Execution.bHalfDynamicWeightsEnabled);
        }
        Execution.bTransferWeightSettingsDirty = false;
        Execution.bForceFullGeometryOnNextStep = true;
    }

#pragma endregion
}