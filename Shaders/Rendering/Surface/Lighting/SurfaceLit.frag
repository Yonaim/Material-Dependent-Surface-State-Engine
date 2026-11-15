// SurfaceLit.frag — 기본 SurfaceLit fragment shader 구현을 공통 include에서 불러온다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant
// 공통 fragment 구현을 포함해 이 재질 pass의 컴파일 구성을 선택한다.

#include "Rendering/Surface/Lighting/SurfaceLit.glsl"
// #endregion
