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

layout(std430, set = 0, binding = 17) readonly buffer TSurfaceMesoNormals
{
    vec4 Values[];
} MesoNormals;

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
    // Capacity는 포화 기준량이다. 초과량도 이웃 전달을 구동하도록 비율의 상한을 제한하지 않는다.
    return CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)] / Capacity;
}

float decayAmount(uint TexelIndex, uint ChannelIndex)
{
    if ((Solver.Flags & (1u << 2u)) != 0u)
    {
        return 0.0;
    }
    uint RecordIndex = profileRecordIndex(TexelIndex, ChannelIndex);
    float DecayRate = ProfileParameters.Values[RecordIndex].DecayAndGeometry.x;
    float CavityRetentionFactor = ProfileParameters.Values[RecordIndex].DecayAndGeometry.y;
    float ConcavityWeight = GeometryScalars.Values[TexelIndex].ConcavityWeight;
    float Retention = (Solver.Flags & (1u << 3u)) != 0u
                          ? 1.0
                          : 1.0 - ConcavityWeight * CavityRetentionFactor;
    float Current = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
    return min(Current, max(0.0, DecayRate * Retention * Solver.DeltaTime));
}

// Per-invocation values, shared across all neighbor slots and State channels.
mat3 SolverNormalMatrix;
vec3 SolverUp;

void initializeGeometryDrive()
{
    SolverNormalMatrix = mat3(0.0);
    SolverUp = vec3(0.0);
    if ((Solver.Flags & (1u << 0u)) != 0u)
    {
        return;
    }
    float GravityLength = length(Solver.GravityWorld.xyz);
    if (GravityLength <= GeometryEpsilon || isnan(GravityLength) || isinf(GravityLength))
    {
        return;
    }
    SolverUp = -Solver.GravityWorld.xyz / GravityLength;
    mat3 ModelLinear = mat3(Solver.ModelMatrix);
    float Determinant = determinant(ModelLinear);
    if (!isnan(Determinant) && !isinf(Determinant) && abs(Determinant) > GeometryEpsilon)
    {
        SolverNormalMatrix = transpose(inverse(ModelLinear));
    }
}

float geometryDrive(uint SourceTexel, uint TargetTexel)
{
    if ((Solver.Flags & (1u << 0u)) != 0u || length(SolverUp) <= GeometryEpsilon)
    {
        return 0.0;
    }
    vec3 LocalNormal = (Solver.Flags & (1u << 4u)) != 0u
                           ? Normals.Values[SourceTexel].xyz
                           : MesoNormals.Values[SourceTexel].xyz;
    vec3 Normal = SolverNormalMatrix * LocalNormal;
    float NormalLength = length(Normal);
    if (NormalLength <= GeometryEpsilon || isnan(NormalLength) || isinf(NormalLength))
    {
        return 0.0;
    }
    Normal /= NormalLength;
    vec3 Gravity = Solver.GravityWorld.xyz;
    vec3 GravityOnSurface = Gravity - Normal * dot(Gravity, Normal);
    float SurfaceGravityLength = length(GravityOnSurface);
    vec3 SourcePosition = Positions.Values[SourceTexel].xyz + Normals.Values[SourceTexel].xyz *
                          GeometryScalars.Values[SourceTexel].MesoVirtualHeight;
    vec3 TargetPosition = Positions.Values[TargetTexel].xyz + Normals.Values[TargetTexel].xyz *
                          GeometryScalars.Values[TargetTexel].MesoVirtualHeight;
    // Translation cancels in both height difference and neighbor direction.
    vec3 NeighborDirection = mat3(Solver.ModelMatrix) * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);
    if (SurfaceGravityLength <= GeometryEpsilon || NeighborLength <= GeometryEpsilon ||
        isnan(SurfaceGravityLength) || isinf(SurfaceGravityLength) ||
        isnan(NeighborLength) || isinf(NeighborLength))
    {
        return 0.0;
    }
    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(GravityOnSurface / SurfaceGravityLength,
                                    NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

float rawFlux(uint SourceTexel, uint TargetTexel, uint ChannelIndex, float CachedTransferWeight)
{
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0 ||
        !supportsChannel(SourceTexel, ChannelIndex) || !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    uint SourceRecordIndex = profileRecordIndex(SourceTexel, ChannelIndex);
    TSurfaceGPUProfileParameters Parameters = ProfileParameters.Values[SourceRecordIndex];
    float TransferRate = Parameters.CapacityInputAndTransfer.z;
    float SaturationDrive = (Solver.Flags & (1u << 1u)) != 0u
                                ? 0.0
                                : max(saturation(SourceTexel, ChannelIndex) -
                                          saturation(TargetTexel, ChannelIndex),
                                      0.0);
    float GeometryTransferRate = Parameters.CapacityInputAndTransfer.w;
    float GeometryDrive = GeometryTransferRate > 0.0
                              ? geometryDrive(SourceTexel, TargetTexel)
                              : 0.0;
    return (SaturationDrive * TransferRate + GeometryDrive * GeometryTransferRate) *
           CachedTransferWeight * Solver.DeltaTime;
}

#endif
