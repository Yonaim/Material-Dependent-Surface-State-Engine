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
    uint Center = FindSurfaceSamplingAnchor(Surface, UV, Range);
#if MDSS_GPU_VALIDATION
    if (Center >= uint(RenderSamplingBoundaryFlags.Values.length())) return vec4(0.0);
#endif
    if (Center == 0xffffffffu) return vec4(0.0);
    vec2 Extent = vec2(textureSize(RenderStateTexture, 0).xy);
    vec2 SurfacePixel = clamp(clamp(UV, 0.0, 1.0) * vec2(Range.yz),
                              vec2(0.5), vec2(Range.yz) - vec2(0.5));
    if (RenderSamplingBoundaryFlags.Values[Center] == 0u)
        return textureLod(RenderStateTexture, vec3(SurfacePixel / Extent, float(Surface)), 0.0);

    // The descriptor may point to the smoothed texture. Read that same texture
    // at chart borders instead of switching back to unsmoothed State SSBO data.
    // Renormalizing only matching texels also avoids a false coverage dip at a
    // UV seam or profile boundary.
    uint Profile = TexelProfileIndices.Values[Center];
    uint Chart = TexelChartIndices.Values[Center];
    vec2 TexelPosition = SurfacePixel - 0.5;
    ivec2 First = ivec2(floor(TexelPosition));
    vec2 Fraction = fract(TexelPosition);
    vec4 Sum = vec4(0.0);
    float WeightSum = 0.0;
    for (int Y = 0; Y < 2; ++Y)
    for (int X = 0; X < 2; ++X)
    {
        ivec2 XY = clamp(First + ivec2(X, Y), ivec2(0), ivec2(Range.yz) - 1);
        uint Texel = Range.x + uint(XY.y) * Range.y + uint(XY.x);
#if MDSS_GPU_VALIDATION
        if (Texel >= uint(TexelSurfaceIndices.Values.length()) ||
            Texel >= uint(TexelProfileIndices.Values.length()) ||
            Texel >= uint(TexelChartIndices.Values.length())) continue;
#endif
        if (TexelSurfaceIndices.Values[Texel] != Surface ||
            TexelProfileIndices.Values[Texel] != Profile ||
            TexelChartIndices.Values[Texel] != Chart) continue;
        float Weight = (X == 0 ? 1.0 - Fraction.x : Fraction.x) *
                       (Y == 0 ? 1.0 - Fraction.y : Fraction.y);
        Sum += Weight * texelFetch(RenderStateTexture, ivec3(XY, int(Surface)), 0);
        WeightSum += Weight;
    }
    return WeightSum > 1e-6 ? Sum / WeightSum : vec4(0.0);
}
