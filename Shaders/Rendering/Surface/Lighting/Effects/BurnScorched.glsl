/**
 * @file BurnScorched.glsl
 * @brief Temporary burn glow and persistent scorched surface tint.
 */

// ============================================================
// 00. BurnScorched Appearance
// ============================================================
// #region 00_BurnScorched_Appearance

void ApplyScorched(float Scorched, inout vec3 Albedo, inout float Roughness)
{
    float Coverage = smoothstep(0.01, 0.30, clamp(Scorched, 0.0, 1.0));
    Albedo = mix(Albedo, vec3(0.065, 0.055, 0.045), Coverage * 0.95);
    Roughness = mix(Roughness, 0.92, Coverage);
}

float ApplyBurn(float Burn, inout vec3 Albedo, inout float Roughness)
{
    float Coverage = smoothstep(0.01, 0.25, clamp(Burn, 0.0, 1.0));
    Albedo = mix(Albedo, vec3(0.95, 0.24, 0.025), Coverage * 0.90);
    Roughness = mix(Roughness, 0.42, Coverage);
    return Coverage;
}
// #endregion
