// SurfaceStateData.glsl — State 진단과 appearance sampling에서 함께 사용하는 읽기 전용 GPU 데이터다.
#ifndef MDSS_SURFACE_STATE_DATA
#define MDSS_SURFACE_STATE_DATA

// 이 파일은 공용 선언만 담는다. 실제 계산은 이 파일을 include한 셰이더가 수행한다.
// include guard는 같은 파일이 여러 번 포함되어도 구조체와 버퍼가 중복 선언되지 않게 한다.
#ifndef SURFACE_DEBUG_SET
#define SURFACE_DEBUG_SET 1
#endif

// -----------------------------------------------------------------------------
// GPU Structures
// -----------------------------------------------------------------------------
#include "Shared/SurfaceState/SurfaceTypes.glsl"

struct TSurfaceGPUNeighborIndices
{
    // 각 texel의 8방향 이웃 인덱스. 없는 방향은 InvalidTexelIndex를 사용한다.
    uint Indices[8];
};

// -----------------------------------------------------------------------------
// Surface Resources
// -----------------------------------------------------------------------------
// set은 descriptor set 번호, binding은 그 안의 버퍼 슬롯 번호다.
// SURFACE_DEBUG_SET은 include 전에 재정의할 수 있다. 아래 binding 번호는
// CPU에서 만드는 descriptor layout과 같아야 하며, 중간 번호가 비어 있어도 의도된 배치다.

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

// 00.00. Texel Mapping and Static Geometry
// #region 00_00_Texel_Mapping_and_Static_Geometry
// binding 0·1·2·3·4·5의 선언을 CPU descriptor와 대응시킨다.

layout(std430, set = SURFACE_DEBUG_SET, binding = 0)

    readonly buffer TSurfaceTexelSurfaceIndices
{
    uint Values[];
}

TexelSurfaceIndices;

layout(std430, set = SURFACE_DEBUG_SET, binding = 1)

    readonly buffer TSurfaceTexelProfileIndices
{
    uint Values[];
}

TexelProfileIndices;

layout(std430, set = SURFACE_DEBUG_SET, binding = 2)

    readonly buffer TSurfacePositions
{
    vec4 Values[];
}

Positions;

layout(std430, set = SURFACE_DEBUG_SET, binding = 3)

    readonly buffer TSurfaceNormals
{
    vec4 Values[];
}

Normals;

layout(std430, set = SURFACE_DEBUG_SET, binding = 4)

    readonly buffer TSurfaceGeometryScalars
{
    TSurfaceGPUGeometryScalar Values[];
}

GeometryScalars;

layout(std430, set = SURFACE_DEBUG_SET, binding = 5)

    readonly buffer TSurfaceNeighborIndices
{
    TSurfaceGPUNeighborIndices Values[];
}

NeighborIndices;

// #endregion

// 00.01. Profile Tables and SoA Access
// #region 00_01_Profile_Tables_and_SoA_Access
// ProfileParameters 버퍼의 값을 읽어 이 구획의 수치 계산에 사용한다.

layout(std430, set = SURFACE_DEBUG_SET, binding = 6)

    readonly buffer TSurfaceProfileParameters
{
    // 3-plane vec4 SoA: capacity/input/transfer, decay/geometry, accumulation/thickness.
    vec4 Values[];
}

ProfileParameters;

// CPU가 [모든 Record의 첫 vec4][모든 Record의 둘째 vec4][모든 Record의 셋째 vec4]
// 순서로 한 버퍼에 저장하므로, Record 하나를 읽을 때 plane별 시작 위치를 계산한다.
uint SurfaceProfileRecordCount()
{
    return uint(ProfileParameters.Values.length()) / 3u;
}

vec4 SurfaceProfileCapacityInputAndTransfer(uint Record)
{
    return ProfileParameters.Values[Record];
}

vec4 SurfaceProfileDecayAndGeometry(uint Record)
{
    return ProfileParameters.Values[SurfaceProfileRecordCount() + Record];
}

vec4 SurfaceProfileAccumulationThickness(uint Record)
{
    return ProfileParameters.Values[SurfaceProfileRecordCount() * 2u + Record];
}

TSurfaceGPUProfileParameters LoadSurfaceProfileParameters(uint Record)
{
    // 세 vec4 plane에서 같은 Record 번호를 각각 읽어 한 구조체로 묶는다.
    TSurfaceGPUProfileParameters P;
    P.CapacityInputAndTransfer = SurfaceProfileCapacityInputAndTransfer(Record);
    P.DecayAndGeometry = SurfaceProfileDecayAndGeometry(Record);
    P.AccumulationThickness = SurfaceProfileAccumulationThickness(Record);

    return P;
}

layout(std430, set = SURFACE_DEBUG_SET, binding = 7)

    readonly buffer TSurfaceProfileSupported
{
    uint Values[];
}

ProfileSupported;

// #endregion

// 00.02. Current State and Display Geometry Inputs
// #region 00_02_Current_State_and_Display_Geometry_Inputs
// binding 8·12·17·19·21의 선언을 CPU descriptor와 대응시킨다.

layout(std430, set = SURFACE_DEBUG_SET, binding = 8)

    readonly buffer TSurfaceCurrentState
{
    float Values[];
}

CurrentState;

layout(std430, set = SURFACE_DEBUG_SET, binding = 12)

    readonly buffer TSurfaceRanges
{
    uvec4 Values[];
}

SurfaceRanges;

layout(std430, set = SURFACE_DEBUG_SET, binding = 17)

    readonly buffer TSurfaceMesoNormals
{
    // xyz: Meso normal; w: maximum Meso height in this surface/chart.
    vec4 Values[];
}

MesoNormals;

layout(std430, set = SURFACE_DEBUG_SET, binding = 19)

    readonly buffer TSurfaceWorldTexelAreas
{
    float Values[];
}

WorldTexelAreas;

layout(std430, set = SURFACE_DEBUG_SET, binding = 21)

    readonly buffer TSurfaceAccumulationHeights
{
    float Values[];
}

AccumulationHeights;

// #endregion

// #endregion
#endif
