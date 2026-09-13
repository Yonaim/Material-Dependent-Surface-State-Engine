#pragma once

#include "Scene/Transform.h"

namespace MDSS
{
    class StaticMeshInstance
    {
    public:
        StaticMeshInstance() = default;
        explicit StaticMeshInstance(Transform InstanceTransform);

        [[nodiscard]] Transform&       GetTransform() noexcept;
        [[nodiscard]] const Transform& GetTransform() const noexcept;

    private:
        Transform InstanceTransform;
    };
} // namespace MDSS
