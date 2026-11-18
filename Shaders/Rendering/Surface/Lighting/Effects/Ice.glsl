// Ice.glsl — 얼음의 두께감·서리·젖은 표면을 공통 표시 파라미터로 변환한다.
#ifndef MDSS_ICE_APPEARANCE
#define MDSS_ICE_APPEARANCE

// ============================================================
// 00. Ice Appearance
// ============================================================
// #region 00_Ice_Appearance
    // Saturation은 광학 두께의 표시용 근사이며 Solver의 높이·물질량을 바꾸지 않는다.

    struct TIceAppearance
    {
        vec3 Albedo;
        float Roughness;
        float Opacity;
        float Frost;
    };

    TIceAppearance EvaluateIceAppearance(float Coverage, float WaterCoverage, float Grain, float NoV)
    {
        float Amount = clamp(Coverage, 0.0, 1.0);
        float Wetness = smoothstep(0.02, 0.60, clamp(WaterCoverage, 0.0, 1.0));
        float Frost = smoothstep(0.10, 0.75, Amount) * (1.0 - 0.70 * Wetness);
        float FrostGrain = Frost * mix(0.88, 1.0, clamp(Grain, 0.0, 1.0));
        vec3 Albedo = mix(vec3(0.32, 0.50, 0.56), vec3(0.72, 0.84, 0.86), FrostGrain);
        float Roughness = mix(0.14, 0.42, FrostGrain);

        // A thin edge transmits the substrate; a thicker frosted body becomes opaque.
        // This optical proxy is deliberately independent of the Lit height display scale.
        float Fresnel = 0.018 + 0.982 * pow(1.0 - clamp(NoV, 0.0, 1.0), 5.0);
        float Transmission = exp(-3.5 * Amount);
        float Opacity = smoothstep(0.02, 0.12, Amount) * (1.0 - (1.0 - Fresnel) * Transmission);

        return TIceAppearance(Albedo, Roughness, Opacity, Frost);
    }

// #endregion
#endif
