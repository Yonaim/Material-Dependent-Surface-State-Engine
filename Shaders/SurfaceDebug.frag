#version 450

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) flat in uint FragSurfaceIndex;

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
    float DebugPadding1;
} Material;

struct TSurfaceGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
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
layout(std430, set = 1, binding = 12) readonly buffer TSurfaceRanges
{
    uvec4 Values[]; // first texel, width, height, texel count
} SurfaceRanges;
layout(std430, set = 1, binding = 13) readonly buffer TSurfaceTexelChartIndices
{
    uint Values[];
} TexelChartIndices;

layout(location = 0) out vec4 OutColor;

const uint InvalidIndex = 0xffffffffu;

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
    bool bInBuffer = TexelIndex < uint(TexelSurfaceIndices.Values.length());
    bool bValid = bInBuffer && TexelSurfaceIndices.Values[TexelIndex] == FragSurfaceIndex &&
                  TexelProfileIndices.Values[TexelIndex] != InvalidIndex;

    if (Material.RenderMode == 6u)
    {
        OutColor = vec4(bValid ? vec3(0.10, 0.78, 0.24) : vec3(0.86, 0.12, 0.08), 1.0);
        return;
    }
    if (!bValid)
    {
        OutColor = vec4(0.10, 0.10, 0.13, 1.0);
        return;
    }

    if (Material.RenderMode == 7u)
    {
        uint Hash = FragSurfaceIndex * 1664525u + 1013904223u;
        vec3 Color = vec3(float(Hash & 255u), float((Hash >> 8u) & 255u), float((Hash >> 16u) & 255u)) / 255.0;
        OutColor = vec4(0.25 + 0.70 * Color, 1.0);
        return;
    }
    if (Material.RenderMode == 8u || Material.RenderMode == 9u)
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
        if (Material.RenderMode == 8u)
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
    OutColor = vec4(HeatColor(Saturation), 1.0);
}
