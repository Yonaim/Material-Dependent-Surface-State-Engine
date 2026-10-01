/**
 * @file SurfaceDebug.frag
 * @brief Surface 상태, mapping, Meso 및 선택 State 적층 미리보기를 출력한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) flat in uint FragSurfaceIndex;
layout(location = 5) in vec3 FragMesoNormalWS;
layout(location = 6) in vec3 FragWorldPosition;

layout(set = 0, binding = 2) uniform MaterialParameters
{
    vec4 BaseColor;
    uint RenderMode;
    uint FlipNormalY;
    float NormalStrength;
    float AmbientLight;
    uint DebugStateChannel;
    uint StateChannelCount;
    float DebugViewParameter;
    float ReliefShadingEnabled;
    vec4 DebugOptions;
    uvec4 DebugFlags;
} Material;

layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

#include "Debug/SurfaceDebugData.glsl"
layout(push_constant) uniform TStaticMeshPushConstants
{
    mat4 Model;
    mat4 ViewProjection;
} Push;

mat3 AccumulationNormalMatrix()
{
    return transpose(inverse(mat3(Push.Model)));
}

layout(std430, set = 1, binding = 10) readonly buffer TSurfaceOutgoingFluxScale
{
    float Values[];
} OutgoingFluxScale;
layout(std430, set = 1, binding = 13) readonly buffer TSurfaceTexelChartIndices
{
    uint Values[];
} TexelChartIndices;
layout(std430, set = 1, binding = 16) readonly buffer TSurfaceTransferWeightDebugAverages
{
    vec4 Values[];
} TransferWeightDebugAverages;
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
const uint RENDER_MODE_SURFACE_TEXEL_GRID = 16u;
const uint RENDER_MODE_SURFACE_TEXEL_AREA = 17u;
const uint RENDER_MODE_ACCUMULATION = 18u;
const uint RENDER_MODE_FINAL_GEOMETRY = 19u;
const uint RENDER_MODE_TOTAL_SIMULATION_HEIGHT = 21u;

float GridLines(vec2 Coordinate, vec2 PixelFootprint, float LineWidth)
{
    vec2 Fraction = fract(Coordinate);
    vec2 Distance = min(Fraction, 1.0 - Fraction) / max(PixelFootprint, vec2(1e-8));
    vec2 Lines = 1.0 - smoothstep(vec2(0.0), vec2(LineWidth), Distance);
    // 셀 간격이 화면에서 2~5픽셀보다 좁아지면 세부 grid line을 숨겨 aliasing을 줄인다.
    Lines *= 1.0 - smoothstep(vec2(0.20), vec2(0.50), PixelFootprint);
    return max(Lines.x, Lines.y);
}

vec3 TexelGridColor(vec2 Coordinate, vec2 PixelFootprint)
{
    float BlockSize = max(Material.DebugViewParameter, 1.0);
    vec2 BlockCoordinate = Coordinate / BlockSize;
    vec2 BlockFootprint = PixelFootprint / BlockSize;
    float Checker = mod(floor(BlockCoordinate.x) + floor(BlockCoordinate.y), 2.0);
    float Visibility = 1.0 - smoothstep(0.20, 0.50, max(BlockFootprint.x, BlockFootprint.y));
    vec3 Color = mix(vec3(0.215, 0.245, 0.275),
                     mix(vec3(0.18, 0.21, 0.24), vec3(0.25, 0.28, 0.31), Checker), Visibility);
    Color = mix(Color, vec3(0.42, 0.46, 0.51), GridLines(Coordinate, PixelFootprint, 0.65));
    return mix(Color, vec3(0.72, 0.88, 0.98), GridLines(BlockCoordinate, BlockFootprint, 1.4));
}

vec3 HeightGridColor(vec3 Color, uvec4 Range, vec2 UVFootprint)
{
    if (Material.DebugFlags.z == 0u) return Color;
    float BlockSize = max(float(Material.DebugFlags.w), 1.0);
    vec2 Coordinate = FragUV * vec2(Range.yz) / BlockSize;
    vec2 Footprint = UVFootprint * vec2(Range.yz) / BlockSize;
    vec3 Background = Material.DebugFlags.z == 2u ? vec3(0.035, 0.045, 0.060) : Color;
    return mix(Background, vec3(0.72, 0.88, 0.98), GridLines(Coordinate, Footprint, 1.4));
}

vec3 TexelAreaColor(float Area)
{
    float Reference = max(Material.DebugViewParameter, 1e-12);
    // 고정 log2 범위로 색을 정한다: 기준의 1/4 이하는 파랑, 기준은 초록, 4배 이상은 빨강.
    float T = clamp((log2(Area) - log2(Reference)) * 0.25 + 0.5, 0.0, 1.0);
    vec3 Small = vec3(0.12, 0.52, 0.92);
    vec3 ReferenceColor = vec3(0.10, 0.78, 0.24);
    vec3 Large = vec3(0.92, 0.18, 0.12);
    return T < 0.5 ? mix(Small, ReferenceColor, T * 2.0) :
                     mix(ReferenceColor, Large, (T - 0.5) * 2.0);
}

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
    // fwidth와 dFdx/dFdy는 fragment별 조기 반환 전에 uniform 분기 안에서 계산한다.
    vec2 UVFootprint = vec2(0.0);
    float WorldAreaPerUV = 0.0;
    bool HeightMode = Material.RenderMode == RENDER_MODE_MESO_HEIGHT || Material.RenderMode == RENDER_MODE_MESO_OFFSET ||
                      Material.RenderMode == RENDER_MODE_ACCUMULATION || Material.RenderMode == RENDER_MODE_FINAL_GEOMETRY ||
                      Material.RenderMode == RENDER_MODE_TOTAL_SIMULATION_HEIGHT;
    if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_GRID || (HeightMode && Material.DebugFlags.z != 0u))
    {
        UVFootprint = fwidth(FragUV);
    }
    else if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_AREA)
    {
        vec2 UVdx = dFdx(FragUV);
        vec2 UVdy = dFdy(FragUV);
        vec3 PositionDx = dFdx(FragWorldPosition);
        vec3 PositionDy = dFdy(FragWorldPosition);
        float UVArea = abs(UVdx.x * UVdy.y - UVdx.y * UVdy.x);
        // 분자와 분모의 화면 면적이 상쇄되므로 카메라 거리와 무관한 월드 면적 비율을 얻는다.
        WorldAreaPerUV = UVArea > 0.0 ? length(cross(PositionDx, PositionDy)) / UVArea : 0.0;
    }
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

    if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_GRID)
    {
        OutColor = vec4(TexelGridColor(FragUV * vec2(Range.yz), UVFootprint * vec2(Range.yz)), 1.0);
        return;
    }
    if (Material.RenderMode == RENDER_MODE_SURFACE_TEXEL_AREA)
    {
        float Area = WorldAreaPerUV / (float(Range.y) * float(Range.z));
        bool ValidArea = Area > 0.0 && !isnan(Area) && !isinf(Area);
        OutColor = vec4(ValidArea ? TexelAreaColor(Area) : vec3(1.0, 0.18, 0.72), 1.0);
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
    // Refined source boundary faces remain visible even where UV cells have no center sample.
    if (Material.RenderMode == RENDER_MODE_MESO_OFFSET || Material.RenderMode == RENDER_MODE_FINAL_GEOMETRY ||
        Material.RenderMode == RENDER_MODE_TOTAL_SIMULATION_HEIGHT)
    {
        vec3 Normal = normalize(FragMesoNormalWS);
        float Diffuse = max(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))), 0.0);
        OutColor = vec4(HeightGridColor(Material.BaseColor.rgb * (0.28 + 0.72 * Diffuse), Range, UVFootprint), Material.BaseColor.a);
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
        OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1.0);
        return;
    }
    if (Material.RenderMode == RENDER_MODE_MACRO_GEOMETRY)
    {
        vec3 Normal = normalize(FragNormal);
        float Diffuse = abs(dot(Normal, normalize(vec3(0.35, 0.55, 1.0))));
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
        uint Component = uint(clamp(Material.DebugViewParameter, 0.0, 3.0));
        float Value = Component == 0u ? Values.x :
                      (Component == 1u ? Values.y : (Component == 2u ? Values.z : Values.w));
        OutColor = vec4(TransferWeightColor(Value), 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_ACCUMULATION)
    {
        TDebugAccumulation D = DebugAccumulation(TexelIndex, Material.DebugStateChannel,
                                                Material.StateChannelCount, Material.DebugOptions.z,
                                                AccumulationNormalMatrix());
        if (D.Status != 3u)
        {
            vec3 Color = D.Status == 4u ? vec3(1,0,1) : (D.Status == 2u ? vec3(0.42,0.42,0.45) : vec3(0.18,0.20,0.24));
            OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1);
            return;
        }
        uint Component = Material.DebugFlags.y;
        float Value = Component == 1u ? D.CavityHeight : (Component == 2u ? D.FollowingHeight : (Component == 3u ? D.Fill : D.Height));
        float Limit = Component == 3u ? 1.0 : max(Material.DebugOptions.y, 1e-12);
        vec3 Color = HeatColor(Value / Limit);
        if (Value > Limit) Color = vec3(1,0.25,0.05);
        OutColor = vec4(HeightGridColor(Color, Range, UVFootprint), 1);
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

    TDebugAccumulation D = DebugAccumulation(TexelIndex, Material.DebugStateChannel,
                                            Material.StateChannelCount, Material.DebugOptions.z,
                                            AccumulationNormalMatrix());
    if (D.Status != 3u)
    {
        OutColor = vec4(1,0,1,1);
        return;
    }
    float Value = Material.DebugFlags.x != 0u ? D.State / max(Material.DebugOptions.x, 1e-12) : D.Saturation;
    vec3 Color = HeatColor(Value);
    if (Material.DebugFlags.x != 0u && Value > 1.0) Color = vec3(1,0.25,0.05);
    OutColor = vec4(ApplyReliefLighting(Color, FragMesoNormalWS), 1.0);
}
