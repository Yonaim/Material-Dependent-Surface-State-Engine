/**
 * @file MaterialParameters.glsl
 * @brief 렌더링 pass들이 공유하는 material 및 demo 효과 파라미터 layout이다.
 */
#ifndef MDSS_MATERIAL_PARAMETERS
#define MDSS_MATERIAL_PARAMETERS

layout(set = 0, binding = 2) uniform MaterialParameters
{
    vec4 BaseColor;
    uint RenderMode;
    uint FlipNormalY;
    float NormalStrength;
    float AmbientLight;
    uint DebugStateChannel;
    uint StateChannelCount;
    float DebugViewParameter;
    float ReliefShadingEnabled;
    vec4 DebugOptions;
    uvec4 DebugFlags;
    uvec4 DemoStateChannels; // Heat ID, Mud ID, WaterFilm ID, effects enabled
    vec4 DemoOptions; // dry roughness, Lava threshold, mud roughness, Lit height display scale
    vec4 DemoEffectOptions; // heat strength, reserved, waterfilm opacity, waterfilm roughness
    uvec4 DemoExtraStateChannels; // Lava ID, reserved, performance flags, enabled effect bits
    vec4 EffectColorRampStarts; // Heat, Mud, WaterFilm, Lava
    vec4 EffectColorRampEnds;
    vec4 EffectLowSaturationColors[4];
    vec4 EffectHighSaturationColors[4];
    vec4 CameraPosition;
} Material;

vec3 MapEffectColor(uint EffectIndex, float Saturation)
{
    float Start = Material.EffectColorRampStarts[EffectIndex];
    float End = Material.EffectColorRampEnds[EffectIndex];
    float Range = max(End - Start, 1e-4);
    float T = clamp((Saturation - Start) / Range, 0.0, 1.0);
    return mix(Material.EffectLowSaturationColors[EffectIndex].rgb,
                Material.EffectHighSaturationColors[EffectIndex].rgb, T);
}
#endif
