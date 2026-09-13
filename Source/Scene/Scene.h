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

        void                                                 AddStaticMeshInstance(StaticMeshInstance Instance);
        [[nodiscard]] std::vector<StaticMeshInstance>&       GetStaticMeshInstances() noexcept;
        [[nodiscard]] const std::vector<StaticMeshInstance>& GetStaticMeshInstances() const noexcept;

    private:
        Camera                          MainCamera;
        std::vector<StaticMeshInstance> StaticMeshInstances;
    };
} // namespace MDSS
