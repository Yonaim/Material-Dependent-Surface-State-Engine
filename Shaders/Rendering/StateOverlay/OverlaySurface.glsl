/**
 * @file OverlaySurface.glsl
 * @brief Heat, Lava, WaterFilm, Mud overlay fragment shader들이 공유하는 coverage 기반 조명 경로다.
 */
#ifndef MDSS_OVERLAY_SURFACE
#define MDSS_OVERLAY_SURFACE
#include "Rendering/Surface/MaterialParameters.glsl"
#include "Rendering/Surface/Lighting.glsl"
#include "Rendering/Surface/Effects/Lava.glsl"
#ifdef OVERLAY_COMBINED
#define SURFACE_DEBUG_SET 1
#define RENDER_STATE_TEXTURE_SET 4
#include "Rendering/Surface/RenderStateSampling.glsl"
#include "Rendering/Surface/Effects/Heat.glsl"
#include "Rendering/Surface/Effects/Mud.glsl"
#include "Rendering/Surface/Effects/WaterFilm.glsl"
layout(set = 0, binding = 0) uniform sampler2D BaseColorTexture;
#endif

layout(location = 0) in vec3 FragNormal;

layout(location = 1) in vec2 FragUV;

layout(location = 2) flat in uint FragSurfaceIndex;

layout(location = 3) in vec3 FragWorldPosition;

layout(location = 4) in float FragCoverage;

layout(location = 0) out vec4 OutColor;

void main()
{
    // 아주 낮은 coverage는 버리고 재질별 branch에서 색, 조명, 투명도를 정한다.
    const float Cutoff = 0.02;
    if (FragCoverage <= Cutoff) discard;
    vec3 N = normalize(FragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);
#ifdef OVERLAY_COMBINED
    uint EnabledEffects = Material.DemoExtraStateChannels.w;
    uint PerformanceFlags = Material.DemoExtraStateChannels.z;
    bool CoverageSmoothing = (PerformanceFlags & 1u) != 0u;
    bool PrecomputedSmoothing = (PerformanceFlags & 2u) != 0u;
    bool UseRenderTexture = (PerformanceFlags & 4u) != 0u || PrecomputedSmoothing;
    vec4 States = UseRenderTexture
        ? SampleRenderStates(FragSurfaceIndex, FragUV,
                             uvec4(Material.DemoStateChannels.xyz, Material.DemoExtraStateChannels.x),
                             Material.StateChannelCount)
        : SampleStateSaturations(FragSurfaceIndex, FragUV,
                                 uvec4(Material.DemoStateChannels.xyz, Material.DemoExtraStateChannels.x),
                                 Material.StateChannelCount);
    if (CoverageSmoothing && !PrecomputedSmoothing)
    {
        if ((EnabledEffects & 1u) != 0u)
            States.x = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoStateChannels.x, Material.StateChannelCount);
        if ((EnabledEffects & 2u) != 0u)
            States.y = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoStateChannels.y, Material.StateChannelCount);
        if ((EnabledEffects & 4u) != 0u)
            States.z = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoStateChannels.z, Material.StateChannelCount);
        if ((EnabledEffects & 8u) != 0u)
            States.w = SampleSmoothedStateSaturation(FragSurfaceIndex, FragUV,
                                                     Material.DemoExtraStateChannels.x, Material.StateChannelCount);
    }
    vec3 Albedo = (texture(BaseColorTexture, FragUV) * Material.BaseColor).rgb;
    float Roughness = Material.DemoOptions.x;
    if ((EnabledEffects & 1u) != 0u)
        ApplyHeat(States.x, Albedo, Material.DemoEffectOptions.x);
    if ((EnabledEffects & 2u) != 0u)
        ApplyMud(States.y, Albedo, Roughness, Material.DemoOptions.z);
    float WaterCoat = Material.DemoExtraStateChannels.y == 0u && (EnabledEffects & 4u) != 0u ? States.z : 0.0;
    if (WaterCoat > 0.0)
        ApplyWaterFilm(WaterCoat, Albedo, Roughness, Material.DemoEffectOptions.w);
    if ((EnabledEffects & 8u) != 0u)
        ApplyLava(States.w, Albedo, Roughness);
    vec3 Color = ShadeSurface(Albedo, N, V, Roughness, Material.AmbientLight, WaterCoat);
    vec3 Emission = (EnabledEffects & 8u) != 0u && States.w > 0.02 ? LavaEmission(States.w) : vec3(0.0);
    OutColor = vec4(min(Color + Emission, vec3(1.0)), 1.0);
#elif defined(OVERLAY_LAVA)
    vec3 Color = ShadeSurface(LavaColor(FragCoverage), N, V, 0.34, Material.AmbientLight);
    OutColor = vec4(min(Color + LavaEmission(FragCoverage), vec3(1.0)), 1.0);
#elif defined(OVERLAY_WATER)
    float Fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 Color = ShadeSurface(MapEffectColor(2u, FragCoverage), N, V, Material.DemoEffectOptions.w,
                              Material.AmbientLight, FragCoverage);
    OutColor = vec4(Color, clamp(Material.DemoEffectOptions.z *
                                 (0.16 + 0.38 * FragCoverage + 0.18 * Fresnel), 0.0, 0.75));
#else
    vec3 Color = ShadeSurface(MapEffectColor(1u, FragCoverage), N, V, Material.DemoOptions.z,
                              Material.AmbientLight, 0.0);
    OutColor = vec4(Color, 1.0);
#endif
}
#endif
