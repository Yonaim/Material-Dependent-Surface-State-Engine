// MaterialParameters.glsl — 렌더링 pass들이 공유하는 material 및 demo 효과 파라미터 layout이다.
#ifndef MDSS_MATERIAL_PARAMETERS
#define MDSS_MATERIAL_PARAMETERS

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

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
    uvec4 DemoStateChannels;      // Heat ID, Mud ID, WaterFilm ID, effects enabled
    vec4 DemoOptions;             // dry roughness, Lava threshold, mud roughness, Lit height display scale
    vec4 DemoEffectOptions;       // heat strength, mud noise amount, waterfilm opacity, waterfilm roughness
    uvec4 DemoExtraStateChannels; // Lava ID, overlay shading diagnostic, performance flags, enabled effect bits
    vec4 EffectColorRampStarts;   // Heat, Mud, WaterFilm, Lava
    vec4 EffectColorRampEnds;
    vec4 EffectLowSaturationColors[4];
    vec4 EffectHighSaturationColors[4];
    vec4 CameraPosition;
    vec4 WaterAppearance;       // film/droplets mode, opacity multiplier, darkening, droplet density
    uvec4 ExtraDemoBindings;    // absorbedwetness ID, WaterFilm visibility mode, Ice ID
    uvec4 BurnScorchedBindings; // Burn ID, Scorched ID
}

Material;

vec3 MapEffectColor(uint EffectIndex, float Saturation)
{
    float Start = Material.EffectColorRampStarts[EffectIndex];
    float End = Material.EffectColorRampEnds[EffectIndex];
    float Range = max(End - Start, 1e-4);
    float T = clamp((Saturation - Start) / Range, 0.0, 1.0);

    return mix(
        Material.EffectLowSaturationColors[EffectIndex].rgb, Material.EffectHighSaturationColors[EffectIndex].rgb, T);
}

// #endregion
#endif
