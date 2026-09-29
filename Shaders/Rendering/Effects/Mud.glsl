// Demo mud coating; coloration is an artistic default, independent of State storage.
void ApplyMud(float Coverage, inout vec3 Albedo, inout float Roughness, float MudRoughness)
{
    float Blend = smoothstep(0.0, 1.0, Coverage);
    Albedo = mix(Albedo, vec3(0.105, 0.060, 0.028), Blend);
    Roughness = mix(Roughness, MudRoughness, Blend);
}
