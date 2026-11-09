// ============================================================
// 00. Shared Shader Helpers
// ============================================================
// #region 00_Shared_Shader_Helpers

float MudNoiseHash(vec3 P)
{
    P = fract(P * 0.1031);
    P += dot(P, P.yzx + 33.33);
    return fract((P.x + P.y) * P.z);
}

float MudValueNoise(vec3 P)
{
    vec3 Cell = floor(P);
    vec3 F = fract(P);
    F = F * F * (3.0 - 2.0 * F);

    float C000 = MudNoiseHash(Cell + vec3(0.0, 0.0, 0.0));
    float C100 = MudNoiseHash(Cell + vec3(1.0, 0.0, 0.0));
    float C010 = MudNoiseHash(Cell + vec3(0.0, 1.0, 0.0));
    float C110 = MudNoiseHash(Cell + vec3(1.0, 1.0, 0.0));
    float C001 = MudNoiseHash(Cell + vec3(0.0, 0.0, 1.0));
    float C101 = MudNoiseHash(Cell + vec3(1.0, 0.0, 1.0));
    float C011 = MudNoiseHash(Cell + vec3(0.0, 1.0, 1.0));
    float C111 = MudNoiseHash(Cell + vec3(1.0, 1.0, 1.0));

    float Z0 = mix(mix(C000, C100, F.x), mix(C010, C110, F.x), F.y);
    float Z1 = mix(mix(C001, C101, F.x), mix(C011, C111, F.x), F.y);
    return mix(Z0, Z1, F.z);
}

vec3 ApplyMudMicroBump(vec3 N, vec3 P, float Amount)
{
    if (Amount <= 0.0)
        return N;

    float Height = MudValueNoise(P * 8.0);
    vec3 dPdx = dFdx(P);
    vec3 dPdy = dFdy(P);
    vec3 R1 = cross(dPdy, N);
    vec3 R2 = cross(N, dPdx);
    float Det = dot(dPdx, R1);
    vec3 Gradient = sign(Det) * (dFdx(Height) * R1 + dFdy(Height) * R2);
    return normalize(abs(Det) * N - (0.025 * Amount) * Gradient);
}

vec4 ShadeWaterFilmLayer(vec3 N, vec3 V, float Coverage)
{
    // 화면의 바탕색은 이미 그려져 있다. 정면에서는 대부분 비치고,
    // 비스듬히 볼수록 물 표면의 반사가 커지도록 source-over 색과 alpha를 만든다.
    float Amount = clamp(Coverage, 0.0, 1.0);
    float NoV = max(dot(N, V), 0.0);
    float Fresnel = 0.02 + 0.98 * pow(1.0 - NoV, 5.0);
    // The presentation toggle strengthens only the optical layer. Coverage and
    // accumulation geometry still come from the same simulated WaterFilm state.
    float Absorption = Material.ExtraDemoBindings.y != 0u
        ? 0.30 + 0.55 * Amount : 0.04 + 0.24 * Amount;
    float OpticalAlpha = Fresnel + (1.0 - Fresnel) * Absorption;
    // A thin film appears early, then gains opacity across most of the range.
    float CoverageFade = smoothstep(0.02, 0.85, Amount);
    float Alpha = clamp(Material.DemoEffectOptions.z * Material.WaterAppearance.y *
                        CoverageFade * OpticalAlpha, 0.0, 0.98);

    vec3 L = normalize(vec3(0.35, 0.55, 1.0));
    vec3 Sum = V + L;
    vec3 H = Sum * inversesqrt(max(dot(Sum, Sum), 1e-12));
    float Highlight = EvaluateSpecularLobe(max(NoV, 1e-4), max(dot(N, L), 0.0),
                                           max(dot(N, H), 0.0), max(dot(V, H), 0.0),
                                           Material.DemoEffectOptions.w, 0.02);
    vec3 Reflection = EvaluateWaterFilmEnvironment(reflect(-V, N), Material.DemoEffectOptions.w) +
                      vec3(Highlight);
    vec3 Tint = MapEffectColor(2u, Amount);
    vec3 Color = (Fresnel * Reflection + (1.0 - Fresnel) * Absorption * Tint) /
                 max(OpticalAlpha, 1e-5);
    return vec4(min(Color, vec3(1.0)), Alpha);
}
// #endregion
