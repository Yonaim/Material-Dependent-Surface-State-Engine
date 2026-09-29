// Demo wet appearance is applied after the mud coating so both States can coexist.
void ApplyWetness(float Wetness, inout vec3 Albedo, inout float Roughness, float WetRoughness)
{
    Albedo *= mix(1.0, 0.56, Wetness);
    Roughness = mix(Roughness, WetRoughness, Wetness);
}
