const uint InvalidSurfaceId = 0xffffffffu;

const uint InvalidTexelIndex = 0xffffffffu;

// simulation topology는 texel마다 최대 8개 방향 slot을 사용하며, 없는 이웃은 sentinel로 표시한다.
const uint SurfaceNeighborCount = 8u;

#include "Shared/SurfaceState/SurfaceTypes.glsl"

// set 0의 binding은 CPU solver와 공유하는 데이터 계약이다. state 배열은 texel-major/channel-minor 순서다.

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface

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
    // 3-plane vec4 SoA; keeps one descriptor while reducing unused profile-field traffic.
    vec4 Values[];
} ProfileParameters;

uint profileParameterRecordCount()
{
    return uint(ProfileParameters.Values.length()) / 3u;
}

vec4 profileCapacityInputAndTransfer(uint Record)
{
    return ProfileParameters.Values[Record];
}

vec4 profileDecayAndGeometry(uint Record)
{
    return ProfileParameters.Values[profileParameterRecordCount() + Record];
}

vec4 profileAccumulationThickness(uint Record)
{
    return ProfileParameters.Values[profileParameterRecordCount() * 2u + Record];
}

TSurfaceGPUProfileParameters loadProfileParameters(uint Record)
{
    TSurfaceGPUProfileParameters P;
    P.CapacityInputAndTransfer = profileCapacityInputAndTransfer(Record);
    P.DecayAndGeometry = profileDecayAndGeometry(Record);
    P.AccumulationThickness = profileAccumulationThickness(Record);
    return P;
}

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
    // FP32 mode stores float bits one-per-word; FP16 mode packs direction pairs into one word.
    uint Values[];
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
const uint SurfaceSolverHalfDynamicWeightsFlag = 1u << 20u;
const uint SurfaceSolverCurrentStateABFlag = 1u << 21u;
const uint SurfaceSolverSeedPersistentActivityFlag = 1u << 22u;
const uint SurfaceSolverPerWorkgroupChannelMaskFlag = 1u << 23u;

#ifdef MDSS_RAW_MODE
bool useRawFluxCache() { return MDSS_RAW_MODE != 0; }
#else
bool useRawFluxCache() { return (Solver.Flags & SurfaceSolverRawFluxCacheFlag) != 0u; }
#endif
bool useSparseSimulationGeometry() { return (Solver.Flags & SurfaceSolverSparseGeometryFlag) != 0u; }
#ifdef MDSS_RAW_MODE
bool useCoalescedRawFluxLayout() { return MDSS_RAW_MODE == 2 || MDSS_RAW_MODE == 4; }
#else
bool useCoalescedRawFluxLayout() { return (Solver.Flags & SurfaceSolverCoalescedRawFluxLayoutFlag) != 0u; }
#endif
bool useSparseSolver() { return (Solver.Flags & SurfaceSolverSparseSolverFlag) != 0u; }
bool useSparseAccumulationHeight() { return (Solver.Flags & SurfaceSolverSparseAccumulationHeightFlag) != 0u; }
bool useActiveChannelMask() { return (Solver.Flags & SurfaceSolverActiveChannelMaskFlag) != 0u; }
bool prepareAccumulationHeight() { return (Solver.Flags & SurfaceSolverPrepareAccumulationHeightFlag) != 0u; }
#ifdef MDSS_RAW_MODE
bool useHalfRawFluxCache() { return MDSS_RAW_MODE == 3 || MDSS_RAW_MODE == 4; }
#else
bool useHalfRawFluxCache() { return useRawFluxCache() && (Solver.Flags & SurfaceSolverHalfRawFluxCacheFlag) != 0u; }
#endif
#ifdef MDSS_HALF_WEIGHTS
bool useHalfDynamicWeights() { return MDSS_HALF_WEIGHTS != 0; }
#else
bool useHalfDynamicWeights() { return (Solver.Flags & SurfaceSolverHalfDynamicWeightsFlag) != 0u; }
#endif
bool currentStateIsAB() { return (Solver.Flags & SurfaceSolverCurrentStateABFlag) != 0u; }
bool seedPersistentActivity() { return (Solver.Flags & SurfaceSolverSeedPersistentActivityFlag) != 0u; }
bool usePerWorkgroupChannelMask() { return (Solver.Flags & SurfaceSolverPerWorkgroupChannelMaskFlag) != 0u; }
// #endregion
