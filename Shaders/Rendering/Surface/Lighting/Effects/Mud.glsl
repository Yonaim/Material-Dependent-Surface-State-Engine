/**

 * @file Mud.glsl
 * @brief State 저장값과 독립적인 시각 기본값으로 진흙 색과 roughness를 적용한다.
 */
void ApplyMud(float Coverage, inout vec3 Albedo, inout float Roughness, float MudRoughness)
{
    float Blend = smoothstep(0.02, 0.50, Coverage);
    Albedo = mix(Albedo, MapEffectColor(1u, Coverage), Blend);
    Roughness = mix(Roughness, MudRoughness, Blend);
}
