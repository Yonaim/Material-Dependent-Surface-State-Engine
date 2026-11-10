// SurfaceDebugColors.glsl — Surface 진단 값의 색상과 heatmap·relief 조명 표현을 계산한다.
// ============================================================
// 00. Shared Shader Helpers
// ============================================================
// #region 00_Shared_Shader_Helpers
// Surface 진단 값의 색상과 heatmap·relief 조명 표현을 계산한다.

// 00.00. Grid and Area Visualization
// #region 00_00_Grid_and_Area_Visualization
// UV grid 경계와 texel 면적 차이를 화면 해상도에 맞춰 색상으로 나타낸다.

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
    vec3 Color =
        mix(vec3(0.215, 0.245, 0.275), mix(vec3(0.18, 0.21, 0.24), vec3(0.25, 0.28, 0.31), Checker), Visibility);
    Color = mix(Color, vec3(0.42, 0.46, 0.51), GridLines(Coordinate, PixelFootprint, 0.65));

    return mix(Color, vec3(0.72, 0.88, 0.98), GridLines(BlockCoordinate, BlockFootprint, 1.4));
}

vec3 HeightGridColor(vec3 Color, uvec4 Range, vec2 UVFootprint)
{
    if (Material.DebugFlags.z == 0u)
    {
        return Color;
    }

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
    return T < 0.5 ? mix(Small, ReferenceColor, T * 2.0) : mix(ReferenceColor, Large, (T - 0.5) * 2.0);
}

// #endregion

// 00.01. State Palettes and Relief Lighting
// #region 00_01_State_Palettes_and_Relief_Lighting
// State별 팔레트와 선택적인 relief 명암을 결합한다.

vec3 HeatColor(float Value)
{
    const vec3 DeepBlue = vec3(0.075, 0.090, 0.260);
    const vec3 Blue = vec3(0.130, 0.360, 0.610);
    const vec3 Teal = vec3(0.120, 0.610, 0.540);
    const vec3 Yellow = vec3(0.990, 0.900, 0.200);
    float T = clamp(Value, 0.0, 1.0);

    if (T < 0.33)
    {
        return mix(DeepBlue, Blue, T / 0.33);
    }

    if (T < 0.66)
    {
        return mix(Blue, Teal, (T - 0.33) / 0.33);
    }

    return mix(Teal, Yellow, (T - 0.66) / 0.34);
}

vec3 TransferWeightColor(float Value)
{
    const vec3 Blocked = vec3(0.16, 0.035, 0.24);
    const vec3 Medium = vec3(0.30, 0.24, 0.78);
    const vec3 Open = vec3(0.18, 0.94, 0.98);
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

// #endregion

// #endregion
