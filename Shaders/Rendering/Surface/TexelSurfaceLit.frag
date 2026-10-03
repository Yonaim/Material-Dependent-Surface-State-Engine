// TEXEL_LIT 변형을 선택해 공통 SurfaceLit fragment 경로를 사용한다.
#version 450
#extension GL_GOOGLE_include_directive : require
#define TEXEL_LIT
#include "Rendering/Surface/SurfaceLit.glsl"
