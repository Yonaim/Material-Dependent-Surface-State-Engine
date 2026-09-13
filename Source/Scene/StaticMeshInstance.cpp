#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
    StaticMeshInstance::StaticMeshInstance(MeshAssetHandle Mesh, Transform InstanceTransform)
        : Mesh(Mesh), InstanceTransform(std::move(InstanceTransform))
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

    MeshAssetHandle StaticMeshInstance::GetMesh() const noexcept
    {
        return Mesh;
    }
} // namespace MDSS
