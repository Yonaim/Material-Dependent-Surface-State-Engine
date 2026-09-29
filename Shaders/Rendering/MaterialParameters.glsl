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
    uvec4 DemoStateChannels; // Wetness ID, Mud ID, effects enabled, reserved
    vec4 DemoOptions; // dry / wet / mud perceptual roughness, Lit mud height reference
    vec4 CameraPosition;
} Material;
#endif
