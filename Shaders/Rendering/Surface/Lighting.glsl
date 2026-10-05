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

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient,
                  float WetnessCoverage, float WaterFilmCoverage, float WetnessSpecularStrength)
{
    // 고정 key light와 ambient 항에 wetness 및 film 반사를 더한다.
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

    float Wet = clamp(WetnessCoverage, 0.0, 1.0);
    float Film = clamp(WaterFilmCoverage, 0.0, 1.0);
    float StateHighlight = 0.0;
    if (Wet > 0.0 && WetnessSpecularStrength > 0.0)
        StateHighlight += EvaluateSpecularLobe(NoV, NoL, NoH, VoH, 0.32, 0.08) *
                          Wet * 0.65 * WetnessSpecularStrength;
    if (Film > 0.0)
        StateHighlight += EvaluateSpecularLobe(NoV, NoL, NoH, VoH, PerceptualRoughness, 0.14) * Film * 0.9;
    Direct += vec3(StateHighlight);

    // environment map 없이도 비스듬한 각도의 젖은 가장자리가 보이도록 약한 반사를 더한다.
    float Grazing = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 SoftWetReflection = vec3(0.12, 0.15, 0.18) * (Wet * Grazing * 0.45 * WetnessSpecularStrength);
    vec3 FilmEdgeReflection = vec3(0.22, 0.27, 0.32) * (Film * Grazing * 0.8);

    // 기존 ambient 조절과 고정된 흰 key light를 사용한다. environment map과 tone mapper는 없다.
    return Albedo * Ambient + Direct * (1.0-Ambient) + SoftWetReflection + FilmEdgeReflection;
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient,
                  float WetnessCoverage, float WaterFilmCoverage)
{
    return ShadeSurface(Albedo, N, ViewVector, PerceptualRoughness, Ambient,
                        WetnessCoverage, WaterFilmCoverage, 1.0);
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient)
{
    return ShadeSurface(Albedo, N, ViewVector, PerceptualRoughness, Ambient, 0.0, 0.0);
}
