#include "Scene/Scene.h"

namespace MDSS
{
    Scene::Scene() : MainCamera({2.2F, 1.8F, 2.8F}, {0.0F, 0.0F, 0.0F})
    {
        Transform CubeTransform{};
        CubeTransform.RotationDegrees = {20.0F, 35.0F, 0.0F};
        StaticMeshInstances.emplace_back(CubeTransform);
    }

    Camera& Scene::GetMainCamera() noexcept
    {
        return MainCamera;
    }

    const Camera& Scene::GetMainCamera() const noexcept
    {
        return MainCamera;
    }

    std::vector<StaticMeshInstance>& Scene::GetStaticMeshInstances() noexcept
    {
        return StaticMeshInstances;
    }

    const std::vector<StaticMeshInstance>& Scene::GetStaticMeshInstances() const noexcept
    {
        return StaticMeshInstances;
    }
} // namespace MDSS
