/**
 * @file OverlayCombined.frag
 * @brief Shared accumulated top and sides, with State materials composed in the fragment.
 */
#version 450
#extension GL_GOOGLE_include_directive : require
#define OVERLAY_COMBINED
#include "Rendering/StateOverlay/OverlaySurface.glsl"
