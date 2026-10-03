/**
 * @file Lava.glsl
 * @brief 선택적으로 사용하는 Lava 상태를 밝은 용융층 색과 emission으로 표현한다.
 */
vec3 LavaColor(float Coverage)
{
    // coverage가 커질수록 주황빛이 강해진다.
    return mix(vec3(0.75, 0.11, 0.008), vec3(1.0, 0.36, 0.035), clamp(Coverage, 0.0, 1.0));
}
vec3 LavaEmission(float Coverage)
{
    // 조명 계산 뒤에 더할 자체 발광 색이다.
    return vec3(0.25, 0.085, 0.008) * clamp(Coverage, 0.0, 1.0);
}
void ApplyLava(float Coverage, inout vec3 Albedo, inout float Roughness)
{
    float Blend = smoothstep(0.0, 0.7, Coverage);
    Albedo = mix(Albedo, LavaColor(Coverage), Blend);
    Roughness = mix(Roughness, 0.34, Blend);
}
