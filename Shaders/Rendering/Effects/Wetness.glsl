// Demo wet appearance is applied after the mud coating so both States can coexist.
void ApplyWetness(float Wetness, inout vec3 Albedo, inout float Roughness, float WetRoughness,
                  vec3 WetTint, float Strength)
{
    float Influence = clamp(Wetness * Strength, 0.0, 1.0);
    Albedo *= mix(vec3(1.0), WetTint, Influence);
    Roughness = mix(Roughness, WetRoughness, Influence);
}
