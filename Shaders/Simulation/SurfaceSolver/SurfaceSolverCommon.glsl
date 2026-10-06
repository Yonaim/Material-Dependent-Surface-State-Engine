/**
 * @file SurfaceSolverCommon.glsl
 * @brief Solver pass에서 공유하는 GPU 데이터와 상태, 감쇠, 형상 계산 함수를 선언한다.
 */
#ifndef MDSS_SURFACE_SOLVER_COMMON_GLSL
#define MDSS_SURFACE_SOLVER_COMMON_GLSL

const uint InvalidSurfaceId = 0xffffffffu;

const uint InvalidTexelIndex = 0xffffffffu;

// simulation topology는 texel마다 최대 8개 방향 slot을 사용하며, 없는 이웃은 sentinel로 표시한다.
const uint SurfaceNeighborCount = 8u;

struct TSurfaceGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
    vec4 AccumulationThickness;
};

// CPU에서 업로드하는 구조체와 필드 순서 및 stride를 맞춘 texel별 형상 데이터다.
struct TSurfaceGPUGeometryScalar
{
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
};

// set 0의 binding은 CPU solver와 공유하는 데이터 계약이다. state 배열은 texel-major/channel-minor 순서다.
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
#ifdef SURFACE_TRANSFER_WEIGHTS_WRITE
layout(std430, set = 0, binding = 14) buffer TSurfaceTransferWeights
#else
layout(std430, set = 0, binding = 14) readonly buffer TSurfaceTransferWeights
#endif
{
    float Values[];
} TransferWeights;

layout(std430, set = 0, binding = 15) buffer TSurfaceRawOutgoing
{
    uint Values[];
} RawOutgoingBuffer;

float loadRawOutgoingFloat(uint Index)
{
    return uintBitsToFloat(RawOutgoingBuffer.Values[Index]);
}

void storeRawOutgoingFloat(uint Index, float Value)
{
    RawOutgoingBuffer.Values[Index] = floatBitsToUint(Value);
}

layout(std430, set = 0, binding = 17) readonly buffer TSurfaceMesoNormals
{
    vec4 Values[];
} MesoNormals;

layout(std430, set = 0, binding = 18) readonly buffer TSurfaceReverseNeighborDirectionIndices
{
    uint Values[];
} ReverseNeighborDirectionIndices;

layout(std430, set = 0, binding = 19) readonly buffer TSurfaceWorldTexelAreas
{
    float Values[];
} WorldTexelAreas;

// dynamic geometry의 texel당 두 vec4: 위치+평균 이웃 거리, local normal+마지막 생성 높이.
layout(std430, set = 0, binding = 20) buffer TSurfaceDynamicGeometry
{
    vec4 Values[]; // displaced position + mean distance, local normal + last built height
} DynamicGeometry;
// Height values and sparse scheduling metadata share one uint-addressed storage buffer.
// Float height/dirty planes are stored as IEEE-754 bit patterns so the same descriptor can also
// use uint atomics for compact workgroup lists and indirect dispatch commands.
layout(std430, set = 0, binding = 21) buffer TSurfaceAccumulationHeights
{
    uint Values[];
} AccumulationHeights;

float accumulationFloat(uint Index)
{
    return uintBitsToFloat(AccumulationHeights.Values[Index]);
}

void storeAccumulationFloat(uint Index, float Value)
{
    AccumulationHeights.Values[Index] = floatBitsToUint(Value);
}
#if defined(SURFACE_CONCAVITY_WEIGHT_WRITE)
layout(std430, set = 0, binding = 22) buffer TSurfaceDynamicConcavityWeights
#else
layout(std430, set = 0, binding = 22) readonly buffer TSurfaceDynamicConcavityWeights
#endif
{
    float Values[];
} DynamicConcavityWeights;

const float StateReferenceArea = 1.0 / (256.0 * 256.0);

// world texel 면적을 solver profile의 고정 기준 면적 단위로 변환한다.
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

const float GeometryEpsilon = 1.0e-6;

const uint SurfaceSolverRawFluxCacheFlag = 1u << 12u;
const uint SurfaceSolverSparseGeometryFlag = 1u << 13u;
const uint SurfaceSolverCoalescedRawFluxLayoutFlag = 1u << 14u;
const uint SurfaceSolverSparseSolverFlag = 1u << 15u;
const uint SurfaceSolverSparseAccumulationHeightFlag = 1u << 16u;
const uint SurfaceSolverActiveChannelMaskFlag = 1u << 17u;
const uint SurfaceSolverPrepareAccumulationHeightFlag = 1u << 18u;
const uint SurfaceSolverHalfRawFluxCacheFlag = 1u << 19u;

bool useRawFluxCache() { return (Solver.Flags & SurfaceSolverRawFluxCacheFlag) != 0u; }
bool useSparseSimulationGeometry() { return (Solver.Flags & SurfaceSolverSparseGeometryFlag) != 0u; }
bool useCoalescedRawFluxLayout() { return (Solver.Flags & SurfaceSolverCoalescedRawFluxLayoutFlag) != 0u; }
bool useSparseSolver() { return (Solver.Flags & SurfaceSolverSparseSolverFlag) != 0u; }
bool useSparseAccumulationHeight() { return (Solver.Flags & SurfaceSolverSparseAccumulationHeightFlag) != 0u; }
bool useActiveChannelMask() { return (Solver.Flags & SurfaceSolverActiveChannelMaskFlag) != 0u; }
bool prepareAccumulationHeight() { return (Solver.Flags & SurfaceSolverPrepareAccumulationHeightFlag) != 0u; }
bool useHalfRawFluxCache() { return useRawFluxCache() && (Solver.Flags & SurfaceSolverHalfRawFluxCacheFlag) != 0u; }

uint sparseWorkgroupCount()
{
    return (Solver.LocalTexelCount + 63u) / 64u;
}

// AccumulationHeights metadata layout (uint words):
// [3*N]
// solver:       [dispatch 3][scheduled flags G][scheduled list G][active channel mask 1]
// accumulation: [dispatch 3][scheduled flags G][scheduled list G]
// geometry:     [dispatch 3][scheduled flags G][scheduled list G]
uint solverScheduleCommandBase() { return 3u * Solver.LocalTexelCount; }
uint solverScheduleFlagBase() { return solverScheduleCommandBase() + 3u; }
uint solverScheduleListBase() { return solverScheduleFlagBase() + sparseWorkgroupCount(); }
uint solverActiveChannelMaskIndex() { return solverScheduleListBase() + sparseWorkgroupCount(); }

uint accumulationScheduleCommandBase() { return solverActiveChannelMaskIndex() + 1u; }
uint accumulationScheduleFlagBase() { return accumulationScheduleCommandBase() + 3u; }
uint accumulationScheduleListBase() { return accumulationScheduleFlagBase() + sparseWorkgroupCount(); }

uint geometryScheduleCommandBase() { return accumulationScheduleListBase() + sparseWorkgroupCount(); }
uint geometryScheduleFlagBase() { return geometryScheduleCommandBase() + 3u; }
uint geometryScheduleListBase() { return geometryScheduleFlagBase() + sparseWorkgroupCount(); }

uint solverOriginalGroup()
{
    if (!useSparseSolver()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[solverScheduleListBase() + gl_WorkGroupID.x];
}

uint accumulationOriginalGroup()
{
    if (!useSparseAccumulationHeight()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[accumulationScheduleListBase() + gl_WorkGroupID.x];
}

uint geometryOriginalGroup()
{
    if (!useSparseSimulationGeometry()) return gl_WorkGroupID.x;
    return AccumulationHeights.Values[geometryScheduleListBase() + gl_WorkGroupID.x];
}

bool isChannelActive(uint ChannelIndex)
{
    if (!useActiveChannelMask() || ChannelIndex >= 32u) return true;
    uint Mask = AccumulationHeights.Values[solverActiveChannelMaskIndex()];
    return (Mask & (1u << ChannelIndex)) != 0u;
}

void scheduleAccumulationGroup(uint Group)
{
    if (!useSparseAccumulationHeight() || !prepareAccumulationHeight()) return;
    uint Groups = sparseWorkgroupCount();
    if (Group >= Groups) return;
    if (atomicCompSwap(AccumulationHeights.Values[accumulationScheduleFlagBase() + Group], 0u, 1u) == 0u)
    {
        uint Slot = atomicAdd(AccumulationHeights.Values[accumulationScheduleCommandBase()], 1u);
        if (Slot < Groups) AccumulationHeights.Values[accumulationScheduleListBase() + Slot] = Group;
    }
}

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
// Profile은 [0, 1] 무차원 계수를 저장하며 실제 속도는 여기서 계산한다.
// CPU transport step bound와 동일한 보정값을 사용한다 (ADR 0033).
#include "SurfaceState/Types/SurfaceSolverRates.h"

const float BaseSaturationTransferRate = MDSS_BASE_SATURATION_TRANSFER_RATE; // State / second

const float BaseGeometryTransferRate = MDSS_BASE_GEOMETRY_TRANSFER_RATE; // State / (world-length * second)

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

uint reverseNeighborDirectionIndex(uint TexelIndex, uint DirectionIndex)
{
    return (ReverseNeighborDirectionIndices.Values[TexelIndex] >> (DirectionIndex * 4u)) & 0xfu;
}

float transferWeight(uint SourceTexel, uint DirectionIndex)
{
    return TransferWeights.Values[SourceTexel * SurfaceNeighborCount + DirectionIndex];
}

bool supportsChannel(uint TexelIndex, uint ChannelIndex);

float accumulationHeight(uint TexelIndex)
{
    if (!isValidTexel(TexelIndex)) return 0.0;
    float AreaScale = texelAreaScale(TexelIndex);
    if (AreaScale <= 0.0 || !solverFinite(AreaScale)) return 0.0;
    float CavityAmount = 0.0;
    float FollowingHeightWorld = 0.0;
    float CavityThicknessWeighted = 0.0;
    for (uint ChannelIndex = 0u; ChannelIndex < Solver.StateChannelCount; ++ChannelIndex)
    {
        if (!supportsChannel(TexelIndex, ChannelIndex)) continue;
        uint Record = profileRecordIndex(TexelIndex, ChannelIndex);
        TSurfaceGPUProfileParameters P = ProfileParameters.Values[Record];
        float Factor = P.DecayAndGeometry.z;
        float CavityFactor = P.DecayAndGeometry.w;
        float ThicknessPerAmount = P.AccumulationThickness.x;
        float State = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
        if (!solverFinite(State) || State <= 0.0 ||
            !solverFinite(Factor) || Factor <= 0.0 ||
            !solverFinite(CavityFactor) ||
            !solverFinite(ThicknessPerAmount) || ThicknessPerAmount < 0.0) continue;
        // 형상에 반영되는 양은 Capacity로 제한하고, 초과량은 전달 계산을 위해 State에 유지한다.
        float Capacity = P.CapacityInputAndTransfer.x * AreaScale;
        if (!solverFinite(Capacity) || Capacity <= 0.0) continue;
        State = min(State, Capacity);
        float Amount = (State / AreaScale) * Factor;
        float CavityContribution = Amount * clamp(CavityFactor, 0.0, 1.0);
        CavityAmount += CavityContribution;
        CavityThicknessWeighted += CavityContribution * ThicknessPerAmount;
        FollowingHeightWorld += Amount * (1.0 - clamp(CavityFactor, 0.0, 1.0)) * ThicknessPerAmount;
    }
    float MesoHeight = GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    float CavityHeight = min(CavityAmount, 1.0) * max(-MesoHeight, 0.0);
    // cavity 용량을 초과한 기여는 각 State의 cavity 비중에 따른 두께로 표면 위에 쌓인다.
    if (CavityAmount > 1.0)
        FollowingHeightWorld += (CavityAmount - 1.0) * (CavityThicknessWeighted / CavityAmount);
    mat3 NormalMatrix = mat3(Solver.NormalMatrixAndUpColumns[0].xyz,
                             Solver.NormalMatrixAndUpColumns[1].xyz,
                             Solver.NormalMatrixAndUpColumns[2].xyz);
    vec3 LocalNormal = normalize(Normals.Values[TexelIndex].xyz);
    // local normal 방향 변위를 world geometric normal 방향의 요청 두께와 맞춘다.
    float WorldToLocalHeight = length(NormalMatrix * LocalNormal);
    float FollowingHeight = FollowingHeightWorld * WorldToLocalHeight;
    float Height = CavityHeight + FollowingHeight;
    return !solverFinite(Height) ? 0.0 : max(Height, 0.0);
}

vec3 effectiveLocalPosition(uint TexelIndex)
{
    // 기본 Meso offset에 accumulation height를 선택적으로 더한 local-space 위치다.
    vec3 Position = Positions.Values[TexelIndex].xyz +
                    Normals.Values[TexelIndex].xyz * GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
    if ((Solver.Flags & (1u << 6u)) != 0u)
        Position += Normals.Values[TexelIndex].xyz * accumulationFloat(TexelIndex);
    return Position;
}

vec3 effectiveLocalNormal(uint TexelIndex)
{
    // accumulation 형상이 켜져 있으면 이웃 높이의 least-squares gradient로 normal을 보정한다.
    if ((Solver.Flags & (1u << 6u)) == 0u) return MesoNormals.Values[TexelIndex].xyz;
    vec3 Fallback = MesoNormals.Values[TexelIndex].xyz;
    vec3 N = normalize(Normals.Values[TexelIndex].xyz);
    vec3 U = normalize(cross(abs(N.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0), N));
    vec3 V = cross(N, U);
    float CenterHeight = GeometryScalars.Values[TexelIndex].MesoVirtualHeight + accumulationFloat(TexelIndex);
    float XX = 0.0, XY = 0.0, YY = 0.0, XH = 0.0, YH = 0.0;
    for (uint DirectionIndex = 0u; DirectionIndex < SurfaceNeighborCount; ++DirectionIndex)
    {
        uint Other = neighborIndex(TexelIndex, DirectionIndex);
        if (Other == InvalidTexelIndex || Other >= Solver.LocalTexelCount || !isValidTexel(Other) ||
            dot(N, Normals.Values[Other].xyz) < 0.5) continue;
        vec3 Delta = Positions.Values[Other].xyz - Positions.Values[TexelIndex].xyz;
        float X = dot(Delta, U), Y = dot(Delta, V);
        float LengthSquared = X * X + Y * Y;
        if (LengthSquared <= 1.0e-16) continue;
        float OtherHeight = GeometryScalars.Values[Other].MesoVirtualHeight + accumulationFloat(Other);
        float Weight = 1.0 / LengthSquared;
        float DH = OtherHeight - CenterHeight;
        XX += X * X * Weight; XY += X * Y * Weight; YY += Y * Y * Weight;
        XH += X * DH * Weight; YH += Y * DH * Weight;
    }
    float Det = XX * YY - XY * XY;
    if (Det <= 1.0e-6 * max(XX * YY, 1.0e-12)) return Fallback;
    vec2 Gradient = vec2(YY * XH - XY * YH, XX * YH - XY * XH) / Det;
    vec3 Result = normalize(N - U * Gradient.x - V * Gradient.y);
    return !solverFinite(Result) ? Fallback : Result;
}

float concavityWeight(uint TexelIndex)
{
    // 동적 형상이 꺼져 있으면 전처리 값을, 켜져 있으면 Geometry Update cache를 사용한다.
    return (Solver.Flags & (1u << 6u)) != 0u
               ? DynamicConcavityWeights.Values[TexelIndex]
               : GeometryScalars.Values[TexelIndex].ConcavityWeight;
}

bool supportsChannel(uint TexelIndex, uint ChannelIndex)
{
    // 유효 texel이고 양의 면적이며, 해당 texel의 profile이 channel을 지원해야 한다.
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

float decayAmountPrepared(uint TexelIndex, float Current,
                          TSurfaceGPUProfileParameters Parameters, float AreaScale)
{
    if ((Solver.Flags & (1u << 2u)) != 0u)
    {
        return 0.0;
    }
    float DecayRate = Parameters.DecayAndGeometry.x;
    float CavityRetentionFactor = Parameters.DecayAndGeometry.y;
    float ConcavityWeight = concavityWeight(TexelIndex);
    float Retention = (Solver.Flags & (1u << 3u)) != 0u
                          ? 1.0
                          : 1.0 - ConcavityWeight * CavityRetentionFactor;
    return min(Current, max(0.0, DecayRate * AreaScale * Retention * Solver.DeltaTime));
}

float decayAmount(uint TexelIndex, uint ChannelIndex)
{
    uint RecordIndex = profileRecordIndex(TexelIndex, ChannelIndex);
    TSurfaceGPUProfileParameters Parameters = ProfileParameters.Values[RecordIndex];
    float Current = CurrentState.Values[stateIndex(TexelIndex, ChannelIndex)];
    return decayAmountPrepared(TexelIndex, Current, Parameters, texelAreaScale(TexelIndex));
}

// Pass 1에서는 source마다 한 번 준비해 모든 이웃과 channel 계산에 재사용한다.
// Pass 2는 유입 source의 형상을 준비해 방향별 flux를 다시 계산한다.
mat3 SolverModelLinear;
vec3 SolverUp;
vec3 SourcePosition;
vec3 SourceGravityDirection;
bool SourceGeometryValid;

void prepareSourceGeometry(uint SourceTexel)
{
    // 중력의 표면 접선 성분과 source 위치를 한 번 계산해 이웃별 geometry drive에 재사용한다.
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
    bool DynamicGeometryEnabled = (Solver.Flags & (1u << 6u)) != 0u;
    vec3 LocalNormal = DynamicGeometryEnabled
                           ? DynamicGeometry.Values[SourceTexel * 2u + 1u].xyz
                           : ((Solver.Flags & (1u << 4u)) != 0u
                                  ? Normals.Values[SourceTexel].xyz
                                  : MesoNormals.Values[SourceTexel].xyz);
    vec3 Normal = SolverNormalMatrix * LocalNormal;
    float NormalLength = length(Normal);
    if (NormalLength <= GeometryEpsilon || !solverFinite(NormalLength))
    {
        return;
    }
    Normal /= NormalLength;
    vec3 Gravity = Solver.GravityWorld.xyz;
    vec3 GravityOnSurface = Gravity - Normal * dot(Gravity, Normal);
    float SurfaceGravityLength = length(GravityOnSurface);
    if (SurfaceGravityLength <= GeometryEpsilon || !solverFinite(SurfaceGravityLength))
    {
        return;
    }
    SourceGravityDirection = GravityOnSurface / SurfaceGravityLength;
    SourcePosition = DynamicGeometryEnabled
                         ? DynamicGeometry.Values[SourceTexel * 2u].xyz
                         : effectiveLocalPosition(SourceTexel);
    SourceGeometryValid = true;
}

float geometryDrive(uint TargetTexel)
{
    if (!SourceGeometryValid)
    {
        return 0.0;
    }
    vec3 TargetPosition = effectiveLocalPosition(TargetTexel);
    // 위치 차이를 사용하므로 model translation은 높이 차와 이웃 방향 계산에서 상쇄된다.
    vec3 NeighborDirection = SolverModelLinear * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);
    if (NeighborLength <= GeometryEpsilon || !solverFinite(NeighborLength))
    {
        return 0.0;
    }
    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(SourceGravityDirection,
                                    NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

float geometryDriveToPosition(vec3 TargetPosition)
{
    if (!SourceGeometryValid)
    {
        return 0.0;
    }
    // 위치 차이를 사용하므로 model translation은 높이 차와 이웃 방향 계산에서 상쇄된다.
    vec3 NeighborDirection = SolverModelLinear * (TargetPosition - SourcePosition);
    float NeighborLength = length(NeighborDirection);
    if (NeighborLength <= GeometryEpsilon || !solverFinite(NeighborLength))
    {
        return 0.0;
    }
    float HeightDrive = abs(dot(NeighborDirection, SolverUp));
    float DirectionDrive = clamp(dot(SourceGravityDirection,
                                    NeighborDirection / NeighborLength), 0.0, 1.0);
    return HeightDrive * DirectionDrive;
}

float rawFlux(uint SourceTexel, uint TargetTexel, uint ChannelIndex, float CachedTransferWeight,
              TSurfaceGPUProfileParameters SourceParameters, float SourceSaturation,
              float TargetSaturation, float TargetConcavity, vec3 TargetPosition)
{
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0)
    {
        return 0.0;
    }

    float SaturationTransferRate = SourceParameters.CapacityInputAndTransfer.z * BaseSaturationTransferRate;
    float SaturationDrive = (Solver.Flags & (1u << 1u)) != 0u
                                ? 0.0
                                : max(SourceSaturation - TargetSaturation, 0.0);
    float GeometryTransferRate = SourceParameters.CapacityInputAndTransfer.w * BaseGeometryTransferRate;
    float GeometryDrive = GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u
                              ? geometryDriveToPosition(TargetPosition)
                              : 0.0;
    float Exit = max(concavityWeight(SourceTexel) - TargetConcavity, 0.0);
    float Retention = clamp(1.0 - SourceParameters.AccumulationThickness.y * Exit, 0.0, 1.0);
    return (SaturationDrive * SaturationTransferRate + GeometryDrive * GeometryTransferRate * max(SourceSaturation, 0.0)) *
           CachedTransferWeight * Retention * Solver.DeltaTime;
}

float rawFlux(uint SourceTexel, uint TargetTexel, uint ChannelIndex, float CachedTransferWeight,
              TSurfaceGPUProfileParameters SourceParameters, float SourceSaturation)
{
    // 포화도 차이와 경사 방향 이동량을 합쳐 source에서 target으로의 양을 계산한다.
    if (CachedTransferWeight <= 0.0 || Solver.DeltaTime <= 0.0 ||
        !supportsChannel(TargetTexel, ChannelIndex))
    {
        return 0.0;
    }

    float SaturationTransferRate =
        SourceParameters.CapacityInputAndTransfer.z * BaseSaturationTransferRate;
    float GeometryTransferRate =
        SourceParameters.CapacityInputAndTransfer.w * BaseGeometryTransferRate;
    float TargetSaturation = 0.0;
    float TargetConcavity = 0.0;
    vec3 TargetPosition = vec3(0.0);

    // 필요한 항만 준비한다. GLSL은 함수 인자를 호출 전에 평가하므로
    // 아래 값을 무조건 인라인 인자로 넘기면 비활성 flux에서도 이웃 데이터를 읽게 된다.
    if ((Solver.Flags & (1u << 1u)) == 0u && SaturationTransferRate != 0.0)
    {
        TargetSaturation = saturation(TargetTexel, ChannelIndex);
    }
    if (SourceParameters.AccumulationThickness.y != 0.0)
    {
        TargetConcavity = concavityWeight(TargetTexel);
    }
    if (GeometryTransferRate > 0.0 && (Solver.Flags & 1u) == 0u)
    {
        TargetPosition = effectiveLocalPosition(TargetTexel);
    }

    return rawFlux(SourceTexel, TargetTexel, ChannelIndex, CachedTransferWeight,
                   SourceParameters, SourceSaturation,
                   TargetSaturation, TargetConcavity, TargetPosition);
}

#endif
