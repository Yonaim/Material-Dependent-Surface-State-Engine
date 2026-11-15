// TexelSurfaceLit.frag — TEXEL_LIT 변형을 선택해 공통 SurfaceLit fragment 경로를 사용한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant
// 공통 fragment 구현을 포함해 이 재질 pass의 컴파일 구성을 선택한다.

#define TEXEL_LIT
#include "Rendering/Surface/Lighting/SurfaceLit.glsl"
// #endregion
