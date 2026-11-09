/**
 * @file OverlayCombined.frag
 * @brief Shared accumulated top and sides, with State materials composed in the fragment.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Fragment Shader Variant
// ============================================================
// #region 00_Fragment_Shader_Variant

#define OVERLAY_COMBINED
#include "Rendering/Overlay/Shading/OverlaySurface.glsl"
// #endregion
