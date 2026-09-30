#include "Rendering/MaterialParameters.glsl"
#include "Rendering/StateSampling.glsl"
#include "Rendering/Effects/Mud.glsl"
#include "Rendering/Effects/Wetness.glsl"
#include "Rendering/Effects/WaterFilm.glsl"
#include "Rendering/Lighting.glsl"
layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) flat in uint FragSurfaceIndex;
layout(location = 5) in vec3 FragMesoNormalWS;
layout(location = 6) in vec3 FragWorldPosition;
layout(set = 0, binding = 0) uniform sampler2D BaseColorTexture;
layout(set = 0, binding = 1) uniform sampler2D NormalTexture;
layout(location = 0) out vec4 OutColor;
void main()
{
    vec3 N = normalize(FragNormal);
#ifdef TEXEL_LIT
    // Computed normal already includes Meso and accumulation. Do not apply the source normal map twice.
    N = normalize(FragMesoNormalWS);
#else
    vec3 T = normalize(FragTangent - N * dot(N, FragTangent));
    vec3 B = normalize(cross(N,T)) * FragTangentSign;
    vec3 MapN = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;
    if (Material.FlipNormalY != 0u) MapN.y = -MapN.y;
    MapN.xy *= Material.NormalStrength;
    N = normalize(mat3(T,B,N) * normalize(MapN));
#endif
    vec4 Color = texture(BaseColorTexture, FragUV) * Material.BaseColor;
    float Roughness = Material.DemoOptions.x;
    if (Material.DemoStateChannels.w != 0u)
    {
        float Wetness = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.x, Material.StateChannelCount);
        float Mud = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.y, Material.StateChannelCount);
        float WaterFilm = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.z, Material.StateChannelCount);
        ApplyMud(Mud, Color.rgb, Roughness, Material.DemoOptions.z);
        ApplyWetness(Wetness, Color.rgb, Roughness, Material.DemoOptions.y);
        ApplyWaterFilm(WaterFilm, Color.rgb, Roughness, Material.DemoOptions.y);
    }
    OutColor = vec4(ShadeSurface(Color.rgb, N, Material.CameraPosition.xyz - FragWorldPosition,
                               Roughness, Material.AmbientLight), Color.a);
}
