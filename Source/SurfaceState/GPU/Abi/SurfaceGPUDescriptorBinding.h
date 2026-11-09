#pragma once
#include <cstdint>

namespace MDSS::SurfaceState
{
    enum class ESurfaceGPUDescriptorBinding : std::uint32_t
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
        Reactions,
        ReactionScratch,
        Count
    };

} // namespace MDSS::SurfaceState
