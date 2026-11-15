// OverlayCombined.frag — 공유 적층 상면·측면에 State 재질을 합성해 최종 색상을 출력한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant
// 공통 fragment 구현을 포함해 이 재질 pass의 컴파일 구성을 선택한다.

#define OVERLAY_COMBINED
#include "Rendering/Overlay/Shading/OverlaySurface.glsl"
// #endregion
