uint rawEdgeFluxBase()
{
    return Solver.LocalTexelCount * Solver.StateChannelCount;
}

uint rawEdgeFluxIndex(uint TexelIndex, uint ChannelIndex, uint DirectionIndex)
{
    if (useCoalescedRawFluxLayout())
    {
        // [channel][direction][texel]
        return rawEdgeFluxBase() +
               (ChannelIndex * SurfaceNeighborCount + DirectionIndex) * Solver.LocalTexelCount + TexelIndex;
    }
    // Legacy A/B layout: [texel][channel][direction]
    return rawEdgeFluxBase() +
           (TexelIndex * Solver.StateChannelCount + ChannelIndex) * SurfaceNeighborCount + DirectionIndex;
}

uint rawEdgeFluxHalfWordIndex(uint TexelIndex, uint ChannelIndex, uint DirectionPairIndex)
{
    const uint PairCount = SurfaceNeighborCount / 2u;
    if (useCoalescedRawFluxLayout())
    {
        // [channel][direction-pair][texel] -- two fp16 values per uint word.
        return rawEdgeFluxBase() +
               (ChannelIndex * PairCount + DirectionPairIndex) * Solver.LocalTexelCount + TexelIndex;
    }
    // [texel][channel][direction-pair]
    return rawEdgeFluxBase() +
           (TexelIndex * Solver.StateChannelCount + ChannelIndex) * PairCount + DirectionPairIndex;
}

float loadCachedRawEdgeFlux(uint TexelIndex, uint ChannelIndex, uint DirectionIndex)
{
    if (!useHalfRawFluxCache())
        return loadRawOutgoingFloat(rawEdgeFluxIndex(TexelIndex, ChannelIndex, DirectionIndex));
    uint PairIndex = DirectionIndex >> 1u;
    uint Packed = RawOutgoingBuffer.Values[rawEdgeFluxHalfWordIndex(TexelIndex, ChannelIndex, PairIndex)];
    vec2 Pair = unpackHalf2x16(Packed);
    return (DirectionIndex & 1u) == 0u ? Pair.x : Pair.y;
}

bool solverFinite(float Value)
{
#if MDSS_GPU_VALIDATION
    return !isnan(Value) && !isinf(Value);
#else
    return true;
#endif
}

bool solverFinite(vec3 Value)
{
#if MDSS_GPU_VALIDATION
    return !any(isnan(Value)) && !any(isinf(Value));
#else
    return true;
#endif
}
