/**
 * @file SurfaceStateData.glsl
 * @brief State 진단과 appearance sampling에서 함께 사용하는 읽기 전용 GPU 데이터다.
 *        binding 번호와 구조체 배치는 CPU에서 업로드하는 데이터 정의와 일치해야 한다.
 */

#ifndef MDSS_SURFACE_STATE_DATA
#define MDSS_SURFACE_STATE_DATA

#ifndef SURFACE_DEBUG_SET
#define SURFACE_DEBUG_SET 1
#endif


// -----------------------------------------------------------------------------
// GPU Structures
// -----------------------------------------------------------------------------
struct TSurfaceGPUProfileParameters
{
    // profile의 용량/입력/transfer, 감쇠/형상, 누적 두께 계수 묶음이다.
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
    vec4 AccumulationThickness;
};

struct TSurfaceGPUGeometryScalar
{
    // meso 형상 높이와 오목함, 곡률 등 texel별 기하 값이다.
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
};

struct TSurfaceGPUNeighborIndices
{
    // 각 texel의 8방향 이웃 인덱스. 없는 방향은 InvalidTexelIndex를 사용한다.
    uint Indices[8];
};


// -----------------------------------------------------------------------------
// Surface Resources
// -----------------------------------------------------------------------------
layout(std430, set = SURFACE_DEBUG_SET, binding = 0)

readonly buffer TSurfaceTexelSurfaceIndices
{
    uint Values[];
} TexelSurfaceIndices;

layout(std430, set = SURFACE_DEBUG_SET, binding = 1)

readonly buffer TSurfaceTexelProfileIndices
{
    uint Values[];
} TexelProfileIndices;

layout(std430, set = SURFACE_DEBUG_SET, binding = 2)

readonly buffer TSurfacePositions
{
    vec4 Values[];
} Positions;

layout(std430, set = SURFACE_DEBUG_SET, binding = 3)

readonly buffer TSurfaceNormals
{
    vec4 Values[];
} Normals;

layout(std430, set = SURFACE_DEBUG_SET, binding = 4)

readonly buffer TSurfaceGeometryScalars
{
    TSurfaceGPUGeometryScalar Values[];
} GeometryScalars;

layout(std430, set = SURFACE_DEBUG_SET, binding = 5)

readonly buffer TSurfaceNeighborIndices
{
    TSurfaceGPUNeighborIndices Values[];
} NeighborIndices;

layout(std430, set = SURFACE_DEBUG_SET, binding = 6)

readonly buffer TSurfaceProfileParameters
{
    TSurfaceGPUProfileParameters Values[];
} ProfileParameters;

layout(std430, set = SURFACE_DEBUG_SET, binding = 7)

readonly buffer TSurfaceProfileSupported
{
    uint Values[];
} ProfileSupported;

layout(std430, set = SURFACE_DEBUG_SET, binding = 8)

readonly buffer TSurfaceCurrentState
{
    float Values[];
} CurrentState;

layout(std430, set = SURFACE_DEBUG_SET, binding = 12)

readonly buffer TSurfaceRanges
{
    uvec4 Values[];
} SurfaceRanges;

layout(std430, set = SURFACE_DEBUG_SET, binding = 17)

readonly buffer TSurfaceMesoNormals
{
    vec4 Values[];
} MesoNormals;

layout(std430, set = SURFACE_DEBUG_SET, binding = 20)

readonly buffer TSurfaceWorldTexelAreas
{
    float Values[];
} WorldTexelAreas;

layout(std430, set = SURFACE_DEBUG_SET, binding = 22)

readonly buffer TSurfaceAccumulationHeights
{
    float Values[];
} AccumulationHeights;


#endif
