/**
 * @file OverlayWater.frag
 * @brief 공통 overlay 조명 경로에서 반투명 Water film 분기를 선택한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require
#define OVERLAY_WATER
#include "Rendering/StateOverlay/OverlaySurface.glsl"
