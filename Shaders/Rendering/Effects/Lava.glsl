// Bright molten coating for the optional demo Lava state.
vec3 LavaColor(float Coverage)
{
    return mix(vec3(0.75, 0.11, 0.008), vec3(1.0, 0.36, 0.035), clamp(Coverage, 0.0, 1.0));
}
vec3 LavaEmission(float Coverage)
{
    return vec3(0.25, 0.085, 0.008) * clamp(Coverage, 0.0, 1.0);
}
void ApplyLava(float Coverage, inout vec3 Albedo, inout float Roughness)
{
    float Blend = smoothstep(0.0, 0.7, Coverage);
    Albedo = mix(Albedo, LavaColor(Coverage), Blend);
    Roughness = mix(Roughness, 0.34, Blend);
}
