/**
 * @file SurfaceSolverCommon.glsl
 * @brief Solver pass에서 공유하는 GPU 데이터와 상태, 감쇠, 형상 계산 함수를 선언한다.
 */
#ifndef MDSS_SURFACE_SOLVER_COMMON_GLSL
#define MDSS_SURFACE_SOLVER_COMMON_GLSL

const uint InvalidSurfaceId = 0xffffffffu;
const uint InvalidTexelIndex = 0xffffffffu;
const uint SurfaceNeighborCount = 8u;
layout(constant_id = 0) const bool UseRawFluxCache = true;

struct TSurfaceGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
};

// CPU에서 업로드하는 구조체와 필드 순서 및 stride를 맞춘 texel별 형상 데이터다.
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

layout(std430, set = 0, binding = 18) readonly buffer TSurfaceReverseNeighborSlots
{
    uint Values[];
} ReverseNeighborSlots;
layout(std430, set = 0, binding = 19) buffer TSurfaceRawFlux
{
    float Values[];
} RawFluxBuffer;

layout(std430, set = 0, binding = 20) readonly buffer TSurfaceWorldTexelAreas
{
    float Values[];
} WorldTexelAreas;

const float StateReferenceArea = 1.0 / (256.0 * 256.0);
float texelAreaScale(uint TexelIndex) { return WorldTexelAreas.Values[TexelIndex] / StateReferenceArea; }

layout(push_constant) uniform TSurfaceSolverPushConstants
{
    float DeltaTime;
    uint StateChannelCount;
    uint LocalTexelCount;
    uint Flags;
    vec4 GravityWorld;
    vec4 ModelLinearColumns[3];
    vec4 NormalMatrixAndUpColumns[3];
} Solver;

bool rawFluxCacheEnabled()
{
    return UseRawFluxCache;
}

const float GeometryEpsilon = 1.0e-6;
// Profile은 [0, 1] 무차원 계수를 저장하며 실제 속도는 여기서 계산한다.
// 초기 기준값은 기존 DemoWetness의 전달 속도를 유지한다 (ADR 0029).
const float BaseSaturationTransferRate = 1.0;  // State / second
const float BaseGeometryTransferRate = 100.0; // State / (world-length * second)

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

uint reverseNeighborSlot(uint TexelIndex, uint DirectionIndex)
{
    return (ReverseNeighborSlots.Values[TexelIndex] >> (DirectionIndex * 4u)) & 0xfu;
}

uint rawFluxIndex(uint TexelIndex, uint ChannelIndex, uint DirectionIndex)
{
    // Direction slot별 plane으로 나눠 인접 invocation의 저장 위치를 연속시킨다.
    return DirectionIndex * (Solver.LocalTexelCount * Solver.StateChannelCount) +
           stateIndex(TexelIndex, ChannelIndex);
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
    return WorldTexelAreas.Values[TexelIndex] > 0.0 &&
           ProfileSupported.Values[profileRecordIndex(TexelIndex, ChannelIndex)] != 0u;
}

float stateCapacity(uint TexelIndex, uint ChannelIndex)
{
    return ProfileParameters.Values[profileRecordIndex(TexelIndex, ChannelIndex)]
        .CapacityInputAndTransfer.x * texelAreaScale(TexelIndex);
}

float saturation(uint TexelIndex, uint ChannelIndex)
{
    float Capacity = stateCapacity(TexelIndex, ChannelIndex);
    // Capacity는 포화 기준량이다. 보유량이 Capacity를 넘으면 초과 비율도 이웃 전달량에 반영한다.
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
    return min(Current, max(0.0, DecayRate * texelAreaScale(TexelIndex) * Retention * Solver.DeltaTime));
}

// Pass 1에서는 source마다 한 번 준비해 모든 이웃과 channel 계산에 재사용한다.
// RawFlux cache를 쓰지 않는 Pass 2에서는 유입 source의 형상 데이터를 준비한 뒤 방향별 flux를 다시 계산한다.
mat3 SolverModelLinear;
vec3 SolverUp;
vec3 SourcePosition;
vec3 SourceGravityDirection;
bool SourceGeometryValid;

void prepareSourceGeometry(uint SourceTexel)
{
    SourceGeometryValid = false;
    if ((Solver.Flags & (1u << 0u)) != 0u)
    {
        return;
    }
    SolverUp = vec3(Solver.NormalMatrixAndUpColumns[0].w,
                    Solver.NormalMatrixAndUpColumns[1].w,
                    Solver.NormalMatrixAndUpColumns[2].w);
    if (length(SolverUp) <= GeometryEpsilon)
    {
        return;
    }
    SolverModelLinear = mat3(Solver.ModelLinearColumns[0].xyz,
                             Solver.ModelLinearColumns[1].xyz,
                             Solver.ModelLinearColumns[2].xyz);
    mat3 SolverNormalMatrix = mat3(Solver.NormalMatrixAndUpColumns[0].xyz,
                                   Solver.NormalMatrixAndUpColumns[1].xyz,
                                   Solver.NormalMatrixAndUpColumns[2].xyz);
    vec3 LocalNormal = (Solver.Flags & (1u << 4u)) != 0u
                           ? Normals.Values[SourceTexel].xyz
                           : MesoNormals.Values[SourceTexel].xyz;
    vec3 Normal = SolverNormalMatrix * LocalNormal;
    float NormalLength = length(Normal);
    if (NormalLength <= GeometryEpsilon || isnan(NormalLength) || isinf(NormalLength))
    {
        return;
    }
    Normal /= NormalLength;
    vec3 Gravity = Solver.GravityWorld.xyz;
    vec3 GravityOnSurface = Gravity - Normal * dot(Gravity, Normal);
    float SurfaceGravityLength = length(GravityOnSurface);
    if (SurfaceGravityLength <= GeometryEpsilon || isnan(SurfaceGravityLength) || isinf(SurfaceGravityLength))
    {
        return;
    }
    SourceGravityDirection = GravityOnSurface / SurfaceGravityLength;
    SourcePosition = Positions.Values[SourceTexel].xyz + Normals.Values[SourceTexel].xyz *
                     GeometryScalars.Values[SourceTexel].MesoVirtualHeight;
    SourceGeometryValid = true;
}

float geometryDrive(uint TargetTexel)
{
    if (!SourceGeometryValid)
    {
        return 0.0;
    }
    vec3 TargetPosition = Positions.Values[TargetTexel].xyz + Normals.Values[TargetTexel].xyz *
                          GeometryScalars.Values[TargetTexel].MesoVirtualHeight;
    // 위치 차이를 사용하므로 model translation은 높이 차와 이웃 방향 계산에서 상쇄된다.
    vec3 NeighborDirection = SolverModelLinear * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);
    if (NeighborLength <= GeometryEpsilon || isnan(NeighborLength) || isinf(NeighborLength))
    {
        return 0.0;
    }
    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(SourceGravityDirection,
                                    NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

float rawFlux(uint TargetTexel, uint ChannelIndex, float CachedTransferWeight,
              TSurfaceGPUProfileParameters SourceParameters, float SourceSaturation)
{
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0 ||
        !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    float SaturationTransferRate = SourceParameters.CapacityInputAndTransfer.z * BaseSaturationTransferRate;
    float SaturationDrive = (Solver.Flags & (1u << 1u)) != 0u
                                ? 0.0
                                : max(SourceSaturation -
                                          saturation(TargetTexel, ChannelIndex),
                                      0.0);
    float GeometryTransferRate = SourceParameters.CapacityInputAndTransfer.w * BaseGeometryTransferRate;
    float GeometryDrive = GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u
                              ? geometryDrive(TargetTexel)
                              : 0.0;
    return (SaturationDrive * SaturationTransferRate + GeometryDrive * GeometryTransferRate * max(SourceSaturation, 0.0)) *
           CachedTransferWeight * Solver.DeltaTime;
}

#endif
