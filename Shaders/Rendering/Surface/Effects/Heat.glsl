/**
 * @file Heat.glsl
 * @brief Heat saturation gradually shifts a surface toward its configured red tint.
 */
void ApplyHeat(float Heat, inout vec3 Albedo, float Strength)
{
    float Influence = clamp(Heat * Strength, 0.0, 1.0);
    Albedo = mix(Albedo, MapEffectColor(0u, Heat), Influence);
}
