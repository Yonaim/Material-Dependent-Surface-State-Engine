/**

 * @file WaterFilm.glsl
 * @brief WaterFilm을 광택과 어두운 색을 가진 표면층으로 표현한다. 두께와 고인 높이는 Lit texel mesh의 accumulation geometry pass에서 계산한다.
 */
void ApplyWaterFilm(float Coverage, inout vec3 Albedo, inout float Roughness, float FilmRoughness)
{
    float Blend = smoothstep(0.0, 1.0, Coverage);
    Albedo *= mix(vec3(1.0), MapEffectColor(2u, Coverage), Blend);
    Roughness = mix(Roughness, FilmRoughness, Blend);
}
