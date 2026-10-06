/**

 * @file Lava.glsl
 * @brief 선택적으로 사용하는 Lava 상태를 밝은 용융층 색과 emission으로 표현한다.
 */
vec3 LavaColor(float Coverage)
{
    return MapEffectColor(3u, Coverage);
}

vec3 LavaEmission(float Coverage, float Threshold)
{
    if (Coverage <= Threshold) return vec3(0.0);
    // 조명 계산 뒤에 더할 자체 발광 색이다.
    return vec3(0.25, 0.085, 0.008) * clamp(Coverage, 0.0, 1.0);
}

void ApplyLava(float Coverage, float Threshold, inout vec3 Albedo, inout float Roughness)
{
    if (Coverage <= Threshold) return;
    Albedo = LavaColor(Coverage);
    Roughness = 0.34;
}
