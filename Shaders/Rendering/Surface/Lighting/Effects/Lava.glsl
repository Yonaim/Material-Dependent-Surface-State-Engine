/**

 * @file Lava.glsl
 * @brief 선택적으로 사용하는 Lava 상태를 밝은 용융층 색과 emission으로 표현한다.
 */
float LavaVisualAmount(float Coverage)
{
    // Keep the saturated lava in the orange part of the artist's color ramp.
    // The threshold controls presence; it must not force the color to yellow.
    return min(clamp(Coverage, 0.0, 1.0), 0.55);
}

vec3 LavaColor(float Coverage)
{
    return MapEffectColor(3u, LavaVisualAmount(Coverage));
}

vec3 LavaEmission(float Coverage, float Threshold)
{
    if (Coverage <= Threshold) return vec3(0.0);
    // 조명 계산 뒤에 더할 자체 발광 색이다.
    return vec3(0.25, 0.085, 0.008) * LavaVisualAmount(Coverage);
}

void ApplyLava(float Coverage, float Threshold, inout vec3 Albedo, inout float Roughness)
{
    if (Coverage <= Threshold) return;
    Albedo = LavaColor(Coverage);
    Roughness = 0.34;
}
