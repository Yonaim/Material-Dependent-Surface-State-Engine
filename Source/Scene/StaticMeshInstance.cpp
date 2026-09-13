#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
    StaticMeshInstance::StaticMeshInstance(Transform InstanceTransform)
        : InstanceTransform(std::move(InstanceTransform))
    {
    }

    Transform& StaticMeshInstance::GetTransform() noexcept
    {
        return InstanceTransform;
    }

    const Transform& StaticMeshInstance::GetTransform() const noexcept
    {
        return InstanceTransform;
    }
} // namespace MDSS
