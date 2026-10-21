/**
 * @file WaterFilm.glsl
 * @brief 얇은 물막에 젖은 바탕색과 매끈한 표면 반응을 적용한다.
 */
void ApplyWaterFilm(float Coverage, inout vec3 Albedo, inout float Roughness, float FilmRoughness)
{
    float Blend = smoothstep(0.02, 0.85, clamp(Coverage, 0.0, 1.0));
    Albedo *= mix(1.0, 0.78, Blend);
    Roughness = mix(Roughness, FilmRoughness, Blend);
}
