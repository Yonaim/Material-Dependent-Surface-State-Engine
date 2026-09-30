#ifndef MDSS_OVERLAY_SURFACE
#define MDSS_OVERLAY_SURFACE
#include "Rendering/MaterialParameters.glsl"
#include "Rendering/Lighting.glsl"
layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec2 FragUV;
layout(location = 2) flat in uint FragSurfaceIndex;
layout(location = 3) in vec3 FragWorldPosition;
layout(location = 4) in float FragCoverage;
layout(location = 0) out vec4 OutColor;
void main()
{
    const float Cutoff = 0.02;
    if (FragCoverage <= Cutoff) discard;
    vec3 N = normalize(FragNormal);
    vec3 V = normalize(Material.CameraPosition.xyz - FragWorldPosition);
#ifdef OVERLAY_WATER
    float Fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 Color = ShadeSurface(Material.WaterFilmTint.rgb, N, V, Material.DemoEffectOptions.w,
                              Material.AmbientLight, 0.0, FragCoverage);
    OutColor = vec4(Color, clamp(Material.DemoEffectOptions.z *
                                 (0.16 + 0.38 * FragCoverage + 0.18 * Fresnel), 0.0, 0.75));
#else
    vec3 Color = ShadeSurface(vec3(0.105, 0.060, 0.028), N, V, Material.DemoOptions.z,
                              Material.AmbientLight, 0.0, 0.0);
    OutColor = vec4(Color, 1.0);
#endif
}
#endif
