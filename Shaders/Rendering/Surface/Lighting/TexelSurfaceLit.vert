// TexelSurfaceLit.vert — 진단용 texel geometry vertex 경로를 Lit texel surface mesh에 재사용한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Vertex Shader Variant
// ============================================================
// #region 00_Vertex_Shader_Variant
// 공통 정점 구현을 포함해 이 pass에 필요한 컴파일 구성을 선택한다.

#include "Rendering/Overlay/Geometry/TexelMeshVertex.glsl"
// #endregion
