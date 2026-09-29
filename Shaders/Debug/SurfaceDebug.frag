/**
 * @file SurfaceDebug.frag
 * @brief Surface 상태, 유효성, 이웃, texel 및 Meso 형상을 Surface Debug 뷰에 출력한다.
 */
#version 450

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) flat in uint FragSurfaceIndex;
layout(location = 5) in vec3 FragMesoNormalWS;

layout(set = 0, binding = 2) uniform MaterialParameters
{
    vec4 BaseColor;
    uint RenderMode;
    uint FlipNormalY;
    float NormalStrength;
    float AmbientLight;
    uint DebugStateChannel;
    uint StateChannelCount;
    float DebugPadding0;
    float ReliefShadingEnabled;
} Material;

layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

struct TSurfaceGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
};

struct TSurfaceGPUGeometryScalar
{
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
};

struct TSurfaceGPUNeighborIndices
{
    uint Indices[8];
};

layout(std430, set = 1, binding = 0) readonly buffer TSurfaceTexelSurfaceIndices
{
    uint Values[];
} TexelSurfaceIndices;
layout(std430, set = 1, binding = 1) readonly buffer TSurfaceTexelProfileIndices
{
    uint Values[];
} TexelProfileIndices;
layout(std430, set = 1, binding = 5) readonly buffer TSurfaceNeighborIndices
{
    TSurfaceGPUNeighborIndices Values[];
} NeighborIndices;
layout(std430, set = 1, binding = 4) readonly buffer TSurfaceGeometryScalars
{
    TSurfaceGPUGeometryScalar Values[];
} GeometryScalars;
layout(std430, set = 1, binding = 6) readonly buffer TSurfaceProfileParameters
{
    TSurfaceGPUProfileParameters Values[];
} ProfileParameters;
layout(std430, set = 1, binding = 7) readonly buffer TSurfaceProfileSupported
{
    uint Values[];
} ProfileSupported;
layout(std430, set = 1, binding = 8) readonly buffer TSurfaceCurrentState
{
    float Values[];
} CurrentState;
layout(std430, set = 1, binding = 10) readonly buffer TSurfaceOutgoingFluxScale
{
    float Values[];
} OutgoingFluxScale;
layout(std430, set = 1, binding = 12) readonly buffer TSurfaceRanges
{
    uvec4 Values[]; // 시작 texel, 너비, 높이, texel 수
} SurfaceRanges;
layout(std430, set = 1, binding = 13) readonly buffer TSurfaceTexelChartIndices
{
    uint Values[];
} TexelChartIndices;
layout(std430, set = 1, binding = 16) readonly buffer TSurfaceTransferWeightDebugAverages
{
    vec4 Values[];
} TransferWeightDebugAverages;
layout(std430, set = 1, binding = 17) readonly buffer TSurfaceMesoNormals
{
    vec4 Values[];
} MesoNormals;

layout(location = 0) out vec4 OutColor;

const uint InvalidIndex = 0xffffffffu;
// 값은 TRenderViewMode enum의 항목과 일치해야 한다.
const uint RENDER_MODE_SURFACE_STATE_HEATMAP = 6u;
const uint RENDER_MODE_SURFACE_VALIDITY = 7u;
const uint RENDER_MODE_SURFACE_ID = 8u;
const uint RENDER_MODE_NEIGHBOR_COUNT = 9u;
const uint RENDER_MODE_SURFACE_SEAM = 10u;
const uint RENDER_MODE_OUTGOING_FLUX_SCALE = 11u;
const uint RENDER_MODE_SOLVER_TRANSFER_WEIGHT = 12u;
const uint RENDER_MODE_MESO_HEIGHT = 13u;
const uint RENDER_MODE_MESO_OFFSET = 14u;
const uint RENDER_MODE_MACRO_GEOMETRY = 15u;

vec3 HeatColor(float Value)
{
    const vec3 DeepBlue = vec3(0.075, 0.090, 0.260);
    const vec3 Blue     = vec3(0.130, 0.360, 0.610);
    const vec3 Teal     = vec3(0.120, 0.610, 0.540);
    const vec3 Yellow   = vec3(0.990, 0.900, 0.200);
    float T = clamp(Value, 0.0, 1.0);
    if (T < 0.33) return mix(DeepBlue, Blue, T / 0.33);
    if (T < 0.66) return mix(Blue, Teal, (T - 0.33) / 0.33);
    return mix(Teal, Yellow, (T - 0.66) / 0.34);
}

vec3 TransferWeightColor(float Value)
{
    const vec3 Blocked = vec3(0.16, 0.035, 0.24);
    const vec3 Medium  = vec3(0.30, 0.24, 0.78);
    const vec3 Open    = vec3(0.18, 0.94, 0.98);
    float T = clamp(Value, 0.0, 1.0);
    return T < 0.5 ? mix(Blocked, Medium, T * 2.0) : mix(Medium, Open, (T - 0.5) * 2.0);
}

vec3 OutgoingFluxScaleColor(float Value)
{
    const vec3 Restricted = vec3(0.76, 0.08, 0.10);
    const vec3 Partial = vec3(1.00, 0.70, 0.10);
    const vec3 Unrestricted = vec3(0.34, 0.86, 0.28);
    float T = clamp(Value, 0.0, 1.0);
    return T < 0.5 ? mix(Restricted, Partial, T * 2.0) : mix(Partial, Unrestricted, (T - 0.5) * 2.0);
}

vec3 ApplyReliefLighting(vec3 HeatmapColor, vec3 MesoNormal)
{
    if (Material.ReliefShadingEnabled < 0.5)
    {
        return HeatmapColor;
    }

    vec3 ReliefNormal = normalize(MesoNormal);
    vec3 LightDirection = normalize(vec3(0.35, 0.55, 1.0));
    float Diffuse = max(dot(ReliefNormal, LightDirection), 0.0);
    float Brightness = 0.58 + 0.42 * Diffuse;
    return HeatmapColor * Brightness;
}

void main()
{
    if (FragSurfaceIndex >= uint(SurfaceRanges.Values.length()))
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return;
    }

    uvec4 Range = SurfaceRanges.Values[FragSurfaceIndex];
    if (Range.y == 0u || Range.z == 0u)
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return;
    }

    uvec2 TexelXY = min(uvec2(floor(clamp(FragUV, 0.0, 1.0) * vec2(Range.yz))), Range.yz - 1u);
    uint TexelIndex = Range.x + TexelXY.y * Range.y + TexelXY.x;
    bool bInBuffer = TexelIndex < uint(TexelSurfaceIndices.Values.length()) &&
                     TexelIndex < uint(TexelProfileIndices.Values.length());
    bool bGeometryValid = bInBuffer && TexelSurfaceIndices.Values[TexelIndex] == FragSurfaceIndex;
    bool bSimulationEnabled = bGeometryValid &&
                              TexelProfileIndices.Values[TexelIndex] != InvalidIndex;

    if (Material.RenderMode == RENDER_MODE_SURFACE_VALIDITY)
    {
        vec3 Color = !bGeometryValid ? vec3(0.86, 0.12, 0.08) :
                     (bSimulationEnabled ? vec3(0.10, 0.78, 0.24) : vec3(0.18, 0.48, 0.82));
        OutColor = vec4(Color, 1.0);
        return;
    }
    if (!bGeometryValid)
    {
        OutColor = vec4(0.10, 0.10, 0.13, 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_OUTGOING_FLUX_SCALE)
    {
        if (Material.StateChannelCount == 0u || Material.DebugStateChannel >= Material.StateChannelCount)
        {
            OutColor = vec4(0.35, 0.35, 0.38, 1.0);
            return;
        }
        uint ScaleIndex = TexelIndex * Material.StateChannelCount + Material.DebugStateChannel;
        uint ProfileIndex = TexelProfileIndices.Values[TexelIndex];
        uint ProfileRecordIndex = ProfileIndex * Material.StateChannelCount + Material.DebugStateChannel;
        if (!bSimulationEnabled || ScaleIndex >= uint(OutgoingFluxScale.Values.length()) ||
            ProfileRecordIndex >= uint(ProfileSupported.Values.length()) ||
            ProfileSupported.Values[ProfileRecordIndex] == 0u)
        {
            OutColor = vec4(0.42, 0.42, 0.45, 1.0);
            return;
        }
        OutColor = vec4(OutgoingFluxScaleColor(OutgoingFluxScale.Values[ScaleIndex]), 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_MESO_HEIGHT)
    {
        if (TexelIndex >= uint(GeometryScalars.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }
        float Height = GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
        float SignedT = clamp(0.5 + atan(Height * 16.0) / 3.14159265, 0.0, 1.0);
        vec3 Negative = vec3(0.12, 0.52, 0.92);
        vec3 Zero = vec3(0.12, 0.13, 0.17);
        vec3 Positive = vec3(1.0, 0.42, 0.10);
        vec3 Color = SignedT < 0.5 ? mix(Negative, Zero, SignedT * 2.0) : mix(Zero, Positive, (SignedT - 0.5) * 2.0);
        OutColor = vec4(Color, 1.0);
        return;
    }
    if (Material.RenderMode == RENDER_MODE_MESO_OFFSET)
    {
        vec3 Normal = normalize(FragMesoNormalWS);
        float Diffuse = max(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))), 0.0);
        OutColor = vec4(Material.BaseColor.rgb * (0.28 + 0.72 * Diffuse), Material.BaseColor.a);
        return;
    }
    if (Material.RenderMode == RENDER_MODE_MACRO_GEOMETRY)
    {
        vec3 Normal = normalize(FragNormal);
        float Diffuse = max(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))), 0.0);
        OutColor = vec4(Material.BaseColor.rgb * (0.28 + 0.72 * Diffuse), Material.BaseColor.a);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_SURFACE_ID)
    {
        uint Hash = FragSurfaceIndex * 1664525u + 1013904223u;
        vec3 Color = vec3(float(Hash & 255u), float((Hash >> 8u) & 255u), float((Hash >> 16u) & 255u)) / 255.0;
        OutColor = vec4(0.25 + 0.70 * Color, 1.0);
        return;
    }
    if (Material.RenderMode == RENDER_MODE_NEIGHBOR_COUNT || Material.RenderMode == RENDER_MODE_SURFACE_SEAM)
    {
        if (TexelIndex >= uint(NeighborIndices.Values.length()) ||
            TexelIndex >= uint(TexelChartIndices.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }
        uint NeighborCount = 0u;
        bool bHasCrossChartNeighbor = false;
        uint Chart = TexelChartIndices.Values[TexelIndex];
        for (uint Slot = 0u; Slot < 8u; ++Slot)
        {
            uint Neighbor = NeighborIndices.Values[TexelIndex].Indices[Slot];
            if (Neighbor == InvalidIndex || Neighbor >= uint(TexelChartIndices.Values.length())) continue;
            ++NeighborCount;
            uint NeighborChart = TexelChartIndices.Values[Neighbor];
            bHasCrossChartNeighbor = bHasCrossChartNeighbor ||
                                     (Chart != InvalidIndex && NeighborChart != InvalidIndex && NeighborChart != Chart);
        }
        if (Material.RenderMode == RENDER_MODE_NEIGHBOR_COUNT)
        {
            float T = float(NeighborCount) / 8.0;
            OutColor = vec4(mix(vec3(0.08, 0.10, 0.18), vec3(0.95, 0.72, 0.12), T), 1.0);
        }
        else
        {
            OutColor = vec4(bHasCrossChartNeighbor ? vec3(1.0, 0.18, 0.72) : vec3(0.12, 0.16, 0.22), 1.0);
        }
        return;
    }

    if (Material.RenderMode == RENDER_MODE_SOLVER_TRANSFER_WEIGHT)
    {
        if (TexelIndex >= uint(TransferWeightDebugAverages.Values.length()))
        {
            OutColor = vec4(0.35, 0.35, 0.35, 1.0);
            return;
        }
        vec4 Values = TransferWeightDebugAverages.Values[TexelIndex];
        uint Component = uint(clamp(Material.DebugPadding0, 0.0, 3.0));
        float Value = Component == 0u ? Values.x :
                      (Component == 1u ? Values.y : (Component == 2u ? Values.z : Values.w));
        OutColor = vec4(TransferWeightColor(Value), 1.0);
        return;
    }

    if (Material.RenderMode != RENDER_MODE_SURFACE_STATE_HEATMAP)
    {
        OutColor = vec4(0.35, 0.35, 0.38, 1.0);
        return;
    }

    // simulation 대상이 아닌 Surface도 형상 진단 뷰에는 표시한다.
    if (!bSimulationEnabled)
    {
        OutColor = vec4(0.18, 0.20, 0.24, 1.0);
        return;
    }

    if (Material.StateChannelCount == 0u || Material.DebugStateChannel >= Material.StateChannelCount)
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return;
    }
    uint ProfileIndex = TexelProfileIndices.Values[TexelIndex];
    uint ProfileRecordIndex = ProfileIndex * Material.StateChannelCount + Material.DebugStateChannel;
    uint StateIndex = TexelIndex * Material.StateChannelCount + Material.DebugStateChannel;
    if (ProfileRecordIndex >= uint(ProfileSupported.Values.length()) ||
        ProfileRecordIndex >= uint(ProfileParameters.Values.length()) ||
        StateIndex >= uint(CurrentState.Values.length()))
    {
        OutColor = vec4(0.35, 0.35, 0.35, 1.0);
        return;
    }
    if (ProfileSupported.Values[ProfileRecordIndex] == 0u)
    {
        OutColor = vec4(0.42, 0.42, 0.45, 1.0);
        return;
    }

    float Capacity = ProfileParameters.Values[ProfileRecordIndex].CapacityInputAndTransfer.x;
    float StateValue = CurrentState.Values[StateIndex];
    float Saturation = Capacity > 0.0 ? clamp(StateValue / Capacity, 0.0, 1.0) : 0.0;
    OutColor = vec4(ApplyReliefLighting(HeatColor(Saturation), FragMesoNormalWS), 1.0);
}
