#pragma once
#include <cstdint>

namespace MDSS::SurfaceState
{
    enum class TSurfaceGPUDescriptorBinding : std::uint32_t
    {
        TexelSurfaceIndices = 0,
        TexelProfileIndices,
        Positions,
        Normals,
        GeometryScalars,
        NeighborIndices,
        ProfileParameters,
        ProfileSupported,
        CurrentState,
        NextState,
        OutgoingFluxScale,
        InputDelta,
        SurfaceRanges,
        TexelChartIndices,
        TransferWeights,
        RawOutgoing,
        TransferWeightDebugAverages,
        MesoNormals,
        ReverseNeighborDirectionIndices,
        WorldTexelAreas,
        DynamicGeometry,
        AccumulationHeights,
        DynamicConcavityWeights,
        Count
    };

} // namespace MDSS::SurfaceState
