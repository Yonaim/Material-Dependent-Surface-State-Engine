/** Surface moisture stains porous material without a reflective water layer. */
void ApplyAbsorbedWetness(float Coverage, inout vec3 Albedo, inout float Roughness)
{
    float Stain = smoothstep(0.025, 0.75, clamp(Coverage, 0.0, 1.0));
    Albedo *= mix(1.0, 0.43, Stain);
    Roughness = mix(Roughness, 0.86, Stain);
}
