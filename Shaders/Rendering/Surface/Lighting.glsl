/**

 * @file Lighting.glsl
 * @brief GGX 분포, correlated Smith visibility, Schlick Fresnel을 조합한 specular 모델이다. 표준식 참고: https://google.github.io/filament/main/filament.html.
 */
float EvaluateSpecularLobe(float NoV, float NoL, float NoH, float VoH,
                           float PerceptualRoughness, float F0)
{
    // 공통 방향 항은 ShadeSurface에서 계산하고, roughness별 분포와 visibility만 계산한다.
    float Alpha = max(PerceptualRoughness * PerceptualRoughness, 0.0025);
    float A2 = Alpha * Alpha;
    float Denom = NoH * NoH * (A2 - 1.0) + 1.0;
    float D = A2 / max(3.14159265 * Denom * Denom, 1e-8);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0-A2) + A2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0-A2) + A2);
    float Visibility = 0.5 / max(GGXV + GGXL, 1e-6);
    float F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
    return 3.14159265 * D * Visibility * F * NoL;
}

vec3 EvaluateWaterFilmEnvironment(vec3 ReflectionDirection, float PerceptualRoughness)
{
    // A small analytic studio environment gives the film something to reflect
    // without requiring an environment-map descriptor. The renderer uses Z-up.
    float Height = clamp(ReflectionDirection.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 Environment = mix(vec3(0.035, 0.042, 0.050),
                           vec3(0.34, 0.40, 0.48),
                           smoothstep(0.08, 0.92, Height));

    float PanelExponent = mix(10.0, 96.0, 1.0 - clamp(PerceptualRoughness, 0.0, 1.0));
    float KeyPanel = pow(max(dot(ReflectionDirection, normalize(vec3(-0.45, 0.28, 0.85))), 0.0),
                         PanelExponent);
    float FillPanel = pow(max(dot(ReflectionDirection, normalize(vec3(0.55, 0.05, 0.72))), 0.0),
                          PanelExponent * 0.72);
    Environment += vec3(0.92, 0.96, 1.0) * (KeyPanel * 0.72 + FillPanel * 0.28);
    return Environment;
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient,
                  float WaterFilmCoverage)
{
    // 고정 key light와 ambient 항에 WaterFilm clear coat 및 반사를 더한다.
    vec3 V = ViewVector * inversesqrt(max(dot(ViewVector, ViewVector), 1e-12));
    vec3 L = normalize(vec3(0.35, 0.55, 1.0));
    vec3 Sum = V + L;
    vec3 H = Sum * inversesqrt(max(dot(Sum, Sum), 1e-12));
    float NoV = max(dot(N,V), 1e-4), NoL = max(dot(N,L), 0.0);
    float NoH = max(dot(N,H), 0.0), VoH = max(dot(V,H), 0.0);
    float Alpha = max(PerceptualRoughness * PerceptualRoughness, 0.0025);
    float A2 = Alpha * Alpha;
    float Denom = NoH * NoH * (A2 - 1.0) + 1.0;
    float D = A2 / max(3.14159265 * Denom * Denom, 1e-8);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0-A2) + A2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0-A2) + A2);
    float Visibility = 0.5 / max(GGXV + GGXL, 1e-6);
    float F = 0.04 + 0.96 * pow(1.0 - VoH, 5.0);
    vec3 Direct = ((1.0-F) * Albedo + vec3(3.14159265 * D * Visibility * F)) * NoL;

    float Film = clamp(WaterFilmCoverage, 0.0, 1.0);
    float StateHighlight = 0.0;
    if (Film > 0.0)
        StateHighlight += EvaluateSpecularLobe(NoV, NoL, NoH, VoH, PerceptualRoughness, 0.02) * Film;

    float NoVFilm = max(dot(N, V), 0.0);
    float FilmFresnel = 0.02 + 0.98 * pow(1.0 - NoVFilm, 5.0);
    vec3 ReflectionDirection = reflect(-V, N);
    vec3 FilmEnvironment = EvaluateWaterFilmEnvironment(ReflectionDirection, PerceptualRoughness);
    vec3 FilmReflection = FilmEnvironment * (Film * FilmFresnel);

    // 기존 ambient 조절과 고정된 흰 key light를 사용한다. environment map과 tone mapper는 없다.
    return Albedo * Ambient + Direct * (1.0-Ambient) + vec3(StateHighlight) + FilmReflection;
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient)
{
    return ShadeSurface(Albedo, N, ViewVector, PerceptualRoughness, Ambient, 0.0);
}
