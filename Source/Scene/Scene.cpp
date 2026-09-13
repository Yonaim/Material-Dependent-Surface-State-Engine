#include "Scene/Scene.h"

#include <utility>

namespace MDSS
{
    Scene::Scene() : MainCamera({2.2F, 1.8F, 2.8F}, {0.0F, 0.0F, 0.0F})
    {
    }

    Camera& Scene::GetMainCamera() noexcept
    {
        return MainCamera;
    }

    const Camera& Scene::GetMainCamera() const noexcept
    {
        return MainCamera;
    }

    void Scene::AddStaticMeshInstance(StaticMeshInstance Instance)
    {
        StaticMeshInstances.push_back(std::move(Instance));
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
