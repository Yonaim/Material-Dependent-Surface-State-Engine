// WaterFilm shades the state as a glossy, darkening surface layer; its thickness and
// pooled relief come from the accumulation geometry pass used by the Lit texel mesh.
void ApplyWaterFilm(float Coverage, inout vec3 Albedo, inout float Roughness, float FilmRoughness)
{
    float Blend = smoothstep(0.0, 1.0, Coverage);
    Albedo *= mix(1.0, 0.72, Blend);
    Roughness = mix(Roughness, FilmRoughness, Blend);
}
