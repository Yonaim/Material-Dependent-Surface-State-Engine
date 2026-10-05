/**
 * @file OverlayStateSampling.glsl
 * @brief Overlay-only profile saturation reads with a small descriptor footprint.
 */
#ifndef MDSS_OVERLAY_STATE_SAMPLING
#define MDSS_OVERLAY_STATE_SAMPLING

struct TOverlayGPUProfileParameters
{
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
    vec4 AccumulationThickness;
};

layout(std430, set = 0, binding = 1) readonly buffer TOverlayTexelProfileIndices
{
    uint Values[];
} OverlayTexelProfileIndices;

layout(std430, set = 0, binding = 6) readonly buffer TOverlayProfileParameters
{
    TOverlayGPUProfileParameters Values[];
} OverlayProfileParameters;

layout(std430, set = 0, binding = 8) readonly buffer TOverlayCurrentState
{
    float Values[];
} OverlayCurrentState;

layout(std430, set = 0, binding = 12) readonly buffer TOverlaySurfaceRanges
{
    uvec4 Values[];
} OverlaySurfaceRanges;

layout(std430, set = 0, binding = 13) readonly buffer TOverlayTexelChartIndices
{
    uint Values[];
} OverlayTexelChartIndices;

layout(std430, set = 0, binding = 19) readonly buffer TOverlayWorldTexelAreas
{
    float Values[];
} OverlayWorldTexelAreas;

float OverlayStateSaturation(uint Texel, uint Channel, uint Channels, uint Profile)
{
    if (Channels == 0u || Channel >= Channels) return 0.0;
#if MDSS_GPU_VALIDATION
    if (Texel >= uint(OverlayWorldTexelAreas.Values.length()) ||
        Texel >= uint(OverlayCurrentState.Values.length()) / Channels ||
        Profile >= uint(OverlayProfileParameters.Values.length()) / Channels ||
        Texel >= uint(OverlayTexelProfileIndices.Values.length())) return 0.0;
#endif
    float Capacity = OverlayProfileParameters.Values[Profile * Channels + Channel]
                         .CapacityInputAndTransfer.x * OverlayWorldTexelAreas.Values[Texel] * (256.0 * 256.0);
    float Amount = OverlayCurrentState.Values[Texel * Channels + Channel];
    if (Capacity <= 0.0 || Amount < 0.0) return 0.0;
    return clamp(Amount / Capacity, 0.0, 1.0);
}

#endif
