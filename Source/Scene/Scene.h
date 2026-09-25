/**
 * @file Scene.h
 * @brief 주 카메라와 정적 메시 인스턴스 구성.
 */

#pragma once

#include "Scene/Camera.h"
#include "Scene/StaticMeshInstance.h"

#include <vector>

namespace MDSS
{
    class TScene
    {
    public:
        TScene();

        [[nodiscard]] TCamera&       GetMainCamera() noexcept;
        [[nodiscard]] const TCamera& GetMainCamera() const noexcept;

        /** @brief TScene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        void                                                 AddStaticMeshInstance(TStaticMeshInstance Instance);
        [[nodiscard]] std::vector<TStaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<TStaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        TCamera                          MainCamera;
        std::vector<TStaticMeshInstance> StaticMeshInstances;
    };
} // namespace MDSS
