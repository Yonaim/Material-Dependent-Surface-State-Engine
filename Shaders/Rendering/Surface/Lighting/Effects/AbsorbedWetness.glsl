// AbsorbedWetness.glsl — 반사 수막 없이 다공성 표면의 흡수 수분 얼룩을 계산한다.
/** Surface moisture stains porous material without a reflective water layer. */

// ============================================================
// 00. AbsorbedWetness Appearance
// ============================================================
// #region 00_AbsorbedWetness_Appearance
// 흡수 수분은 다공성 표면 tint에만 반영하고 반사 수막과 구별한다.

void ApplyAbsorbedWetness(float Coverage, inout vec3 Albedo, inout float Roughness)
{
    float Stain = smoothstep(0.025, 0.75, clamp(Coverage, 0.0, 1.0));
    Albedo *= mix(1.0, 0.43, Stain);
    Roughness = mix(Roughness, 0.86, Stain);
}

// #endregion
