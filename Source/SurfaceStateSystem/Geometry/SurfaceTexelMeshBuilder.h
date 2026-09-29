#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"

namespace MDSS
{
    struct TSurfaceTexelMeshRange
    {
        std::uint32_t FirstIndex = 0;
        std::uint32_t IndexCount = 0;
    };

    struct TSurfaceTexelMesh
    {
        // Absolute texel indices become gl_VertexIndex; no duplicate position buffer.
        std::vector<std::uint32_t> Indices;
        std::vector<TSurfaceTexelMeshRange> Surfaces;
    };

    /** @brief Connect valid samples inside each UV chart. Chart boundaries remain open. */
    [[nodiscard]] TSurfaceTexelMesh BuildSurfaceTexelMesh(const TSharedSurfaceGeometryData& Geometry);
}
