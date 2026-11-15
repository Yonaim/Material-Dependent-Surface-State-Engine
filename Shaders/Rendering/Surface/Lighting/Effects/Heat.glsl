// Heat.glsl — Heat saturation에 따라 표면 색상과 발광을 계산한다.
// ============================================================
// 00. Heat Appearance
// ============================================================
// #region 00_Heat_Appearance
// 정규화된 Heat 입력으로 설정된 고온 색상과 발광을 보간한다.

void ApplyHeat(float Heat, inout vec3 Albedo, float Strength)
{
    float Influence = clamp(Heat * Strength, 0.0, 1.0);
    Albedo = mix(Albedo, MapEffectColor(0u, Heat), Influence);
}

// #endregion
