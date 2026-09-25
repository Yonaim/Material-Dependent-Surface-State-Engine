/**
 * @file Scene.cpp
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#include "Scene/Scene.h"

#include "Logger/Logger.h"

#include <utility>

namespace MDSS
{
    TScene::TScene() : MainCamera({2.2F, 1.8F, 2.8F}, {0.0F, 0.0F, 0.0F})
    {
        TLogger::Debug("TScene", "Main camera created at default position.");
    }

    TCamera& TScene::GetMainCamera() noexcept
    {
        return MainCamera;
    }

    const TCamera& TScene::GetMainCamera() const noexcept
    {
        return MainCamera;
    }

    void TScene::AddStaticMeshInstance(TStaticMeshInstance Instance)
    {
        StaticMeshInstances.push_back(std::move(Instance));
        TLogger::Debug("TScene",
                      "Static mesh instance added. TScene instance count=" + std::to_string(StaticMeshInstances.size()) +
                          ".");
    }

    std::vector<TStaticMeshInstance>& TScene::GetStaticMeshInstances() noexcept
    {
        return StaticMeshInstances;
    }

    const std::vector<TStaticMeshInstance>& TScene::GetStaticMeshInstances() const noexcept
    {
        return StaticMeshInstances;
    }
} // namespace MDSS
