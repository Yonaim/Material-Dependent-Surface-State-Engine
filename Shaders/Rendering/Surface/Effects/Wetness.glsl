/**

 * @file Wetness.glsl
 * @brief Mud 색을 먼저 적용한 뒤 wetness 효과를 적용해 두 상태를 함께 표현한다.
 */
void ApplyWetness(float Wetness, inout vec3 Albedo, inout float Roughness, float WetRoughness,
                  vec3 WetTint, float Strength)
{
    float Influence = clamp(Wetness * Strength, 0.0, 1.0);
    Albedo *= mix(vec3(1.0), WetTint, Influence);
    Roughness = mix(Roughness, WetRoughness, Influence);
}
