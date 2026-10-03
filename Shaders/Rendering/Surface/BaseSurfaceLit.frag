// 기본 surface pass에서 texel 형상과 Normal Map 높이를 반영한 조명을 출력한다.
#version 450
#extension GL_GOOGLE_include_directive : require
#define TEXEL_LIT
#define BASE_SURFACE_LIT
#include "Rendering/Surface/SurfaceLit.glsl"
