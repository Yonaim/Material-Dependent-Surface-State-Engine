/**
 * @file TexelSurfaceLit.frag
 * @brief TEXEL_LIT 변형을 선택해 공통 SurfaceLit fragment 경로를 사용한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant

#define TEXEL_LIT
#include "Rendering/Surface/Lighting/SurfaceLit.glsl"
// #endregion
