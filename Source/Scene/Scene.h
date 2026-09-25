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
    class Scene
    {
    public:
        Scene();

        [[nodiscard]] Camera&       GetMainCamera() noexcept;
        [[nodiscard]] const Camera& GetMainCamera() const noexcept;

        /** @brief Scene 소유 목록에 정적 메시 인스턴스를 추가한다. */
        void                                                 AddStaticMeshInstance(StaticMeshInstance Instance);
        [[nodiscard]] std::vector<StaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<StaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        Camera                          MainCamera;
        std::vector<StaticMeshInstance> StaticMeshInstances;
    };
} // namespace MDSS
