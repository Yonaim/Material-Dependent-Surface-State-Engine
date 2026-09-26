#ifndef MDSS_SURFACE_SOLVER_COMMON_GLSL
#define MDSS_SURFACE_SOLVER_COMMON_GLSL

const uint InvalidSurfaceId = 0xffffffffu;
const uint InvalidTexelIndex = 0xffffffffu;
const uint SurfaceNeighborCount = 8u;

struct TSurfaceGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
};

struct TSurfaceGPUGeometryScalar
{
    float MesoVirtualHeight;
    float ConcavityWeight;
};

layout(std430, set = 0, binding = 0) readonly buffer TSurfaceTexelSurfaceIndices
{
    uint Values[];
} TexelSurfaceIndices;
layout(std430, set = 0, binding = 1) readonly buffer TSurfaceTexelProfileIndices
{
    uint Values[];
} TexelProfileIndices;
layout(std430, set = 0, binding = 2) readonly buffer TSurfacePositions
{
    vec4 Values[];
} Positions;
layout(std430, set = 0, binding = 3) readonly buffer TSurfaceNormals
{
    vec4 Values[];
} Normals;
layout(std430, set = 0, binding = 4) readonly buffer TSurfaceGeometryScalars
{
    TSurfaceGPUGeometryScalar Values[];
} GeometryScalars;
layout(std430, set = 0, binding = 5) readonly buffer TSurfaceNeighborIndices
{
    uint Values[];
} NeighborIndices;
layout(std430, set = 0, binding = 6) readonly buffer TSurfaceProfileParameters
{
    TSurfaceGPUProfileParameters Values[];
} ProfileParameters;
layout(std430, set = 0, binding = 7) readonly buffer TSurfaceProfileSupported
{
    uint Values[];
} ProfileSupported;
layout(std430, set = 0, binding = 8) readonly buffer TSurfaceCurrentState
{
    float Values[];
} CurrentState;
layout(std430, set = 0, binding = 9) writeonly buffer TSurfaceNextState
{
    float Values[];
} NextState;
layout(std430, set = 0, binding = 10) buffer TSurfaceOutgoingFluxScale
{
    float Values[];
} OutgoingFluxScale;
layout(std430, set = 0, binding = 11) buffer TSurfaceInputDelta
{
    float Values[];
} InputDelta;

layout(push_constant) uniform TSurfaceSolverPushConstants
{
    float DeltaTime;
    uint StateChannelCount;
    uint LocalTexelCount;
    uint Flags;
    vec4 GravityLocal;
} Solver;

uint stateIndex(uint TexelIndex, uint ChannelIndex)
{
    return TexelIndex * Solver.StateChannelCount + ChannelIndex;
}

uint profileRecordIndex(uint TexelIndex, uint ChannelIndex)
{
    return TexelProfileIndices.Values[TexelIndex] * Solver.StateChannelCount + ChannelIndex;
}

bool isValidTexel(uint TexelIndex)
{
    return TexelIndex < Solver.LocalTexelCount &&
           TexelSurfaceIndices.Values[TexelIndex] != InvalidSurfaceId &&
           TexelProfileIndices.Values[TexelIndex] != InvalidSurfaceId;
}

bool supportsChannel(uint TexelIndex, uint ChannelIndex)
{
    if (!isValidTexel(TexelIndex) || ChannelIndex >= Solver.StateChannelCount)
    {
        return false;
    }
    return ProfileSupported.Values[profileRecordIndex(TexelIndex, ChannelIndex)] != 0u;
}

float stateCapacity(uint TexelIndex, uint ChannelIndex)
{
    return ProfileParameters.Values[profileRecordIndex(TexelIndex, ChannelIndex)]
        .CapacityInputAndTransfer.x;
}

float saturation(uint TexelIndex, uint ChannelIndex)
{
    float Capacity = stateCapacity(TexelIndex, ChannelIndex);
    return clamp(CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)] / Capacity, 0.0, 1.0);
}

float decayAmount(uint TexelIndex, uint ChannelIndex)
{
    uint RecordIndex = profileRecordIndex(TexelIndex, ChannelIndex);
    float DecayRate = ProfileParameters.Values[RecordIndex].DecayAndGeometry.x;
    float CavityRetentionFactor = ProfileParameters.Values[RecordIndex].DecayAndGeometry.y;
    float ConcavityWeight = GeometryScalars.Values[TexelIndex].ConcavityWeight;
    float Retention = 1.0 - ConcavityWeight * CavityRetentionFactor;
    float Current = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
    return min(Current, max(0.0, DecayRate * Retention * Solver.DeltaTime));
}

float rawSaturationFlux(uint SourceTexel, uint TargetTexel, uint ChannelIndex)
{
    if (!supportsChannel(SourceTexel, ChannelIndex) || !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    uint SourceRecordIndex = profileRecordIndex(SourceTexel, ChannelIndex);
    float TransferRate = ProfileParameters.Values[SourceRecordIndex].CapacityInputAndTransfer.z;
    float SaturationDrive = max(saturation(SourceTexel, ChannelIndex) -
                                saturation(TargetTexel, ChannelIndex), 0.0);
    return SaturationDrive * TransferRate * Solver.DeltaTime;
}

uint neighborIndex(uint TexelIndex, uint DirectionIndex)
{
    return NeighborIndices.Values[TexelIndex * SurfaceNeighborCount + DirectionIndex];
}

#endif
