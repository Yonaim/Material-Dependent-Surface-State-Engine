// GGX distribution + correlated Smith visibility + Schlick Fresnel.
// Reference: https://google.github.io/filament/main/filament.html (standard model).
float EvaluateSpecularLobe(vec3 N, vec3 V, vec3 L, float PerceptualRoughness, float F0)
{
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
    float F = F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0);
    return 3.14159265 * D * Visibility * F * NoL;
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient,
                  float WetnessCoverage, float WaterFilmCoverage)
{
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
    float WetHighlight = EvaluateSpecularLobe(N, V, L, 0.32, 0.08);
    float FilmHighlight = EvaluateSpecularLobe(N, V, L, 0.075, 0.14);
    Direct += vec3(WetHighlight * Wet * 0.65 + FilmHighlight * Film * 0.9);

    // A restrained view-angle reflection hint keeps wet edges legible without an environment map.
    float Grazing = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 SoftWetReflection = vec3(0.12, 0.15, 0.18) * (Wet * Grazing * 0.45);
    vec3 FilmEdgeReflection = vec3(0.22, 0.27, 0.32) * (Film * Grazing * 0.8);

    // Existing ambient control, fixed white key light; no environment map or tone mapper.
    return Albedo * Ambient + Direct * (1.0-Ambient) + SoftWetReflection + FilmEdgeReflection;
}

vec3 ShadeSurface(vec3 Albedo, vec3 N, vec3 ViewVector, float PerceptualRoughness, float Ambient)
{
    return ShadeSurface(Albedo, N, ViewVector, PerceptualRoughness, Ambient, 0.0, 0.0);
}
