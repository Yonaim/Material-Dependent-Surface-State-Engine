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
    // Use the displaced geometric normal as the basis, then restore the material's tangent-space detail.
    N = normalize(FragMesoNormalWS);
    vec3 PositionDx = dFdx(FragWorldPosition);
    vec3 PositionDy = dFdy(FragWorldPosition);
    vec2 UVDx = dFdx(FragUV);
    vec2 UVDy = dFdy(FragUV);
    float UVDeterminant = UVDx.x * UVDy.y - UVDx.y * UVDy.x;
    if (abs(UVDeterminant) > 1e-10)
    {
        vec3 Tangent = (PositionDx * UVDy.y - PositionDy * UVDx.y) / UVDeterminant;
        vec3 Bitangent = (-PositionDx * UVDy.x + PositionDy * UVDx.x) / UVDeterminant;
        Tangent -= N * dot(N, Tangent);
        if (dot(Tangent, Tangent) > 1e-12 && dot(Bitangent, Bitangent) > 1e-12)
        {
            Tangent = normalize(Tangent);
            float Handedness = dot(cross(N, Tangent), Bitangent) < 0.0 ? -1.0 : 1.0;
            Bitangent = normalize(cross(N, Tangent)) * Handedness;
            vec3 MapN = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;
            if (Material.FlipNormalY != 0u) MapN.y = -MapN.y;
            MapN.xy *= Material.NormalStrength;
            N = normalize(mat3(Tangent, Bitangent, N) * normalize(MapN));
        }
    }
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
    float Wetness = 0.0;
    float WaterFilm = 0.0;
    if (Material.DemoStateChannels.w != 0u)
    {
        Wetness = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.x, Material.StateChannelCount);
        float Mud = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.y, Material.StateChannelCount);
        WaterFilm = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.z, Material.StateChannelCount);
#ifndef BASE_SURFACE_LIT
        ApplyMud(Mud, Color.rgb, Roughness, Material.DemoOptions.z);
#endif
        ApplyWetness(Wetness, Color.rgb, Roughness, Material.DemoOptions.y,
                     Material.WetnessTint.rgb, Material.DemoEffectOptions.x);
#ifndef BASE_SURFACE_LIT
        ApplyWaterFilm(WaterFilm, Color.rgb, Roughness, Material.DemoEffectOptions.w,
                       Material.WaterFilmTint.rgb);
#else
        WaterFilm = 0.0;
#endif
    }
    OutColor = vec4(ShadeSurface(Color.rgb, N, Material.CameraPosition.xyz - FragWorldPosition,
                               Roughness, Material.AmbientLight,
                               Wetness * Material.DemoEffectOptions.x, WaterFilm,
                               Material.DemoEffectOptions.y), Color.a);
}
