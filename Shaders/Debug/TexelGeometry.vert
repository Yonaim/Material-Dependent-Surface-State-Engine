/**
 * @file TexelGeometry.vert
 * @brief 계산된 texel 높이와 normal을 원본 mesh 정점에 보간해 적용한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Debug/TexelMeshVertex.glsl"
