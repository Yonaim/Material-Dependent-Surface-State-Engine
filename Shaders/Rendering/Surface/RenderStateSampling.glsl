/**
 * @file RenderStateSampling.glsl
 * @brief Interior texel은 필터링한 렌더 State texture에서, chart/profile 경계는 기존 규칙으로 읽는다.
 */
#include "Rendering/Surface/StateSampling.glsl"

#ifndef RENDER_STATE_TEXTURE_SET
#ifdef TEXEL_LIT
#define RENDER_STATE_TEXTURE_SET 3
#else
#define RENDER_STATE_TEXTURE_SET 2
#endif
#endif

layout(set = RENDER_STATE_TEXTURE_SET, binding = 1) uniform sampler2DArray RenderStateTexture;
layout(std430, set = RENDER_STATE_TEXTURE_SET, binding = 2) readonly buffer TRenderSamplingBoundaryFlags
{
    uint Values[];
} RenderSamplingBoundaryFlags;

vec4 SampleRenderStates(uint Surface, vec2 UV, uvec4 StateChannels, uint Channels)
{
    if (Channels == 0u) return vec4(0.0);
#if MDSS_GPU_VALIDATION
    if (Surface >= uint(SurfaceRanges.Values.length())) return vec4(0.0);
#endif
    uvec4 Range = SurfaceRanges.Values[Surface];
    if (Range.y == 0u || Range.z == 0u) return vec4(0.0);
    uvec2 CenterXY = min(uvec2(clamp(UV, 0.0, 1.0) * vec2(Range.yz)), Range.yz - 1u);
    uint Center = Range.x + CenterXY.y * Range.y + CenterXY.x;
#if MDSS_GPU_VALIDATION
    if (Center >= uint(RenderSamplingBoundaryFlags.Values.length())) return vec4(0.0);
#endif
    if (RenderSamplingBoundaryFlags.Values[Center] != 0u)
        return SampleStateSaturations(Surface, UV, StateChannels, Channels);

    vec2 Extent = vec2(textureSize(RenderStateTexture, 0).xy);
    vec2 SurfacePixel = clamp(clamp(UV, 0.0, 1.0) * vec2(Range.yz),
                              vec2(0.5), vec2(Range.yz) - vec2(0.5));
    return textureLod(RenderStateTexture, vec3(SurfacePixel / Extent, float(Surface)), 0.0);
}
