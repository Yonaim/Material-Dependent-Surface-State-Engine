// Mud.glsl — State 저장값과 독립적인 시각 기본값으로 진흙 색과 roughness를 적용한다.
// ============================================================
// 00. Mud Appearance
// ============================================================
// #region 00_Mud_Appearance
// Mud State의 외형 변화를 기본 표면 조명에 사용할 재질 값으로 만든다.

void ApplyMud(float Coverage, inout vec3 Albedo, inout float Roughness, float MudRoughness)
{
    float Blend = smoothstep(0.02, 0.50, Coverage);
    Albedo = mix(Albedo, MapEffectColor(1u, Coverage), Blend);
    Roughness = mix(Roughness, MudRoughness, Blend);
}

// #endregion
