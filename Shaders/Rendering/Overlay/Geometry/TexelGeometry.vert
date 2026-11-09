/**
 * @file TexelGeometry.vert
 * @brief 계산된 texel 높이와 normal을 원본 mesh 정점에 보간해 적용한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Vertex Shader Variant
// ============================================================
// #region 00_Vertex_Shader_Variant

#include "Rendering/Overlay/Geometry/TexelMeshVertex.glsl"
// #endregion
