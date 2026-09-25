/**
 * @file Scene.cpp
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#include "Scene/Scene.h"

#include "Logger/Logger.h"

#include <utility>

namespace MDSS
{
    Scene::Scene() : MainCamera({2.2F, 1.8F, 2.8F}, {0.0F, 0.0F, 0.0F})
    {
        Logger::Debug("Scene", "Main camera created at default position.");
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
        Logger::Debug("Scene",
                      "Static mesh instance added. Scene instance count=" + std::to_string(StaticMeshInstances.size()) +
                          ".");
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
