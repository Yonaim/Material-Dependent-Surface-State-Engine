// Demo wet appearance is applied after the mud coating so both States can coexist.
void ApplyWetness(float Wetness, inout vec3 Albedo, inout float Roughness, float WetRoughness)
{
    Albedo *= mix(vec3(1.0), vec3(0.44, 0.56, 0.68), Wetness);
    Roughness = mix(Roughness, WetRoughness, Wetness);
}
