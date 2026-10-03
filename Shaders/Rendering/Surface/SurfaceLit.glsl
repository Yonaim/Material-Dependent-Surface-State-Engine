/**
 * @file SurfaceLit.glsl
 * @brief Static mesh와 texel mesh가 공유하는 PBR 조명 및 demo State 효과 경로다.
 */
#include "Rendering/Surface/MaterialParameters.glsl"
#include "Rendering/Surface/StateSampling.glsl"
#include "Rendering/Surface/Effects/Mud.glsl"
#include "Rendering/Surface/Effects/Wetness.glsl"
#include "Rendering/Surface/Effects/WaterFilm.glsl"
#include "Rendering/Surface/Effects/Lava.glsl"
#include "Rendering/Surface/Lighting.glsl"
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
#ifndef BASE_SURFACE_LIT
    // 일반 texel-lit view는 변위 normal 위에 material 세부 normal을 더한다.
    N = normalize(FragMesoNormalWS);
#endif
    // base pass가 normal map의 적분 높이로 이미 geometry를 변위시켰다.
    // 같은 normal map을 두 번 적용하지 않도록 macro normal에서 출발한다.
    // texel mesh에는 정점 tangent가 없으므로 위치와 UV의 screen derivative에서 tangent basis를 복원한다.
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
    // 일반 mesh는 정점에서 전달된 tangent와 handedness로 tangent-to-world 변환을 구성한다.
    vec3 T = normalize(FragTangent - N * dot(N, FragTangent));
    vec3 B = normalize(cross(N,T)) * FragTangentSign;
    vec3 MapN = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;
    if (Material.FlipNormalY != 0u) MapN.y = -MapN.y;
    MapN.xy *= Material.NormalStrength;
    N = normalize(mat3(T,B,N) * normalize(MapN));
#endif
    if (!gl_FrontFacing) N = -N;
    vec4 Color = texture(BaseColorTexture, FragUV) * Material.BaseColor;
    float Roughness = Material.DemoOptions.x;
    float Wetness = 0.0;
    float WaterFilm = 0.0;
    float Lava = 0.0;
    if (Material.DemoStateChannels.w != 0u)
    {
        // demo channel ID는 동적으로 지정되며, 모든 상태를 공통 saturation sampling 경로로 읽는다.
        Wetness = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.x, Material.StateChannelCount);
        float Mud = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.y, Material.StateChannelCount);
        WaterFilm = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoStateChannels.z, Material.StateChannelCount);
        Lava = SampleStateSaturation(FragSurfaceIndex, FragUV, Material.DemoExtraStateChannels.x, Material.StateChannelCount);
#ifndef BASE_SURFACE_LIT
        // 기본 surface pass는 이미 형상을 만들었으므로 외관 효과만 적용하고 두께는 추가하지 않는다.
        ApplyMud(Mud, Color.rgb, Roughness, Material.DemoOptions.z);
        ApplyLava(Lava, Color.rgb, Roughness);
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
#ifndef BASE_SURFACE_LIT
    if (Material.DemoStateChannels.w != 0u)
        OutColor.rgb = min(OutColor.rgb + LavaEmission(Lava), vec3(1.0));
#endif
}
