#pragma once

#include "AssetManager/Asset.h"
#include "Scene/Transform.h"

namespace MDSS
{
    class StaticMeshInstance
    {
    public:
        StaticMeshInstance() = default;
        StaticMeshInstance(MeshAssetHandle Mesh, Transform InstanceTransform = {});

        [[nodiscard]] Transform&       GetTransform() noexcept;
        [[nodiscard]] const Transform& GetTransform() const noexcept;
        [[nodiscard]] MeshAssetHandle  GetMesh() const noexcept;

    private:
        MeshAssetHandle Mesh = InvalidAssetHandle;
        Transform       InstanceTransform;
    };
} // namespace MDSS
