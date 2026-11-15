// OverlayMud.frag — 공통 overlay 조명 경로에서 흙색 재질 분기를 선택한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant
// 공통 fragment 구현을 포함해 이 재질 pass의 컴파일 구성을 선택한다.

#include "Rendering/Overlay/Shading/OverlaySurface.glsl"
// #endregion
