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

// CPU 업로드 구조체의 필드 순서와 stride를 맞춘 texel별 형상 값이다.
struct TSurfaceGPUGeometryScalar
{
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
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
layout(std430, set = 0, binding = 14) readonly buffer TSurfaceTransferWeights
{
    float Values[];
} TransferWeights;
layout(std430, set = 0, binding = 15) buffer TSurfaceRawOutgoing
{
    float Values[];
} RawOutgoingBuffer;

layout(push_constant) uniform TSurfaceSolverPushConstants
{
    float DeltaTime;
    uint StateChannelCount;
    uint LocalTexelCount;
    uint Flags;
    vec4 GravityWorld;
    mat4 ModelMatrix;
} Solver;

const float GeometryEpsilon = 1.0e-6;

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

uint neighborIndex(uint TexelIndex, uint DirectionIndex)
{
    return NeighborIndices.Values[TexelIndex * SurfaceNeighborCount + DirectionIndex];
}

float transferWeight(uint SourceTexel, uint DirectionIndex)
{
    return TransferWeights.Values[SourceTexel * SurfaceNeighborCount + DirectionIndex];
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

vec3 worldPosition(uint TexelIndex)
{
    vec3 EffectiveLocalPosition = Positions.Values[TexelIndex].xyz +
                                  Normals.Values[TexelIndex].xyz *
                                  GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    return (Solver.ModelMatrix * vec4(EffectiveLocalPosition, 1.0)).xyz;
}

vec3 worldNormal(uint TexelIndex)
{
    mat3 ModelLinear = mat3(Solver.ModelMatrix);
    float Determinant = determinant(ModelLinear);
    if (abs(Determinant) <= GeometryEpsilon)
    {
        return vec3(0.0);
    }

    vec3 Transformed = transpose(inverse(ModelLinear)) * Normals.Values[TexelIndex].xyz;
    float Length = length(Transformed);
    return Length > GeometryEpsilon ? Transformed / Length : vec3(0.0);
}

float effectiveWorldHeight(uint TexelIndex)
{
    vec3 LocalPosition = Positions.Values[TexelIndex].xyz +
                         Normals.Values[TexelIndex].xyz * GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    vec3 WorldPosition = (Solver.ModelMatrix * vec4(LocalPosition, 1.0)).xyz;
    float GravityLength = length(Solver.GravityWorld.xyz);
    if (any(isnan(WorldPosition)) || any(isinf(WorldPosition)))
    {
        return uintBitsToFloat(0x7fc00000u);
    }
    if (GravityLength <= GeometryEpsilon)
    {
        return 0.0;
    }
    vec3 Up = -Solver.GravityWorld.xyz / GravityLength;
    return dot(WorldPosition, Up);
}

float directionDrive(uint SourceTexel, uint TargetTexel)
{
    vec3 Gravity = Solver.GravityWorld.xyz;
    float GravityLength = length(Gravity);
    if (GravityLength <= GeometryEpsilon)
    {
        return 0.0;
    }

    vec3 Normal = worldNormal(SourceTexel);
    if (length(Normal) <= GeometryEpsilon)
    {
        return 0.0;
    }

    vec3 GravityOnSurface = Gravity - Normal * dot(Gravity, Normal);
    float SurfaceGravityLength = length(GravityOnSurface);
    vec3 NeighborDirection = worldPosition(TargetTexel) - worldPosition(SourceTexel);
    float NeighborLength = length(NeighborDirection);
    if (SurfaceGravityLength <= GeometryEpsilon || NeighborLength <= GeometryEpsilon ||
        any(isnan(GravityOnSurface)) || any(isinf(GravityOnSurface)) ||
        any(isnan(NeighborDirection)) || any(isinf(NeighborDirection)))
    {
        return 0.0;
    }

    return clamp(dot(GravityOnSurface / SurfaceGravityLength, NeighborDirection / NeighborLength), 0.0, 1.0);
}

float rawFlux(uint SourceTexel, uint TargetTexel, uint ChannelIndex, float CachedTransferWeight)
{
    if (!supportsChannel(SourceTexel, ChannelIndex) || !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    uint SourceRecordIndex = profileRecordIndex(SourceTexel, ChannelIndex);
    TSurfaceGPUProfileParameters Parameters = ProfileParameters.Values[SourceRecordIndex];
    float TransferRate = Parameters.CapacityInputAndTransfer.z;
    float SaturationDrive = max(saturation(SourceTexel, ChannelIndex) -
                                saturation(TargetTexel, ChannelIndex), 0.0);
    float GeometryTransferRate = Parameters.CapacityInputAndTransfer.w;
    float GeometryDrive = 0.0;
    if (GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u)
    {
        float SourceHeight = effectiveWorldHeight(SourceTexel);
        float TargetHeight = effectiveWorldHeight(TargetTexel);
        if (!isnan(SourceHeight) && !isinf(SourceHeight) && !isnan(TargetHeight) && !isinf(TargetHeight))
        {
            float HeightDifference = abs(SourceHeight - TargetHeight);
            if (!isnan(HeightDifference) && !isinf(HeightDifference))
            {
                GeometryDrive = HeightDifference * directionDrive(SourceTexel, TargetTexel);
            }
        }
    }
    return (SaturationDrive * TransferRate + GeometryDrive * GeometryTransferRate) *
           CachedTransferWeight * Solver.DeltaTime;
}

#endif
