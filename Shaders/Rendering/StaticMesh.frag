/**
 * @file StaticMesh.frag
 * @brief Static Mesh의 기본 색상, Normal Map 조명과 렌더링 진단 모드를 처리한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Rendering/MaterialParameters.glsl"
#include "Rendering/Lighting.glsl"

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) in vec3 FragWorldPosition;

layout(set = 0, binding = 0) uniform sampler2D BaseColorTexture;
layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

layout(location = 0) out vec4 OutColor;

const uint RENDER_MODE_LIT = 0u;
const uint RENDER_MODE_BASE_COLOR = 1u;
const uint RENDER_MODE_WIREFRAME = 2u;
const uint RENDER_MODE_VERTEX_NORMAL_WS = 3u;
const uint RENDER_MODE_NORMAL_TEXTURE_TS = 4u;
const uint RENDER_MODE_MAPPED_NORMAL_WS = 5u;

vec3 VisualizeNormal(vec3 Normal)
{
    return Normal * 0.5 + 0.5;
}

void main()
{
    vec3 normalWS = normalize(FragNormal);
    vec3 tangentWS = normalize(FragTangent - normalWS * dot(normalWS, FragTangent));
    vec3 bitangentWS = normalize(cross(normalWS, tangentWS)) * FragTangentSign;
    mat3 tangentToWorld = mat3(tangentWS, bitangentWS, normalWS);

    vec3 normalTS = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;
    if (Material.FlipNormalY != 0u)
    {
        normalTS.y = -normalTS.y;
    }
    normalTS.xy *= Material.NormalStrength;
    normalTS = normalize(normalTS);

    vec3 mappedNormalWS = normalize(tangentToWorld * normalTS);
    vec4 albedo = texture(BaseColorTexture, FragUV) * Material.BaseColor;

    if (Material.RenderMode == RENDER_MODE_BASE_COLOR)
    {
        OutColor = albedo;
        return;
    }

    if (Material.RenderMode == RENDER_MODE_VERTEX_NORMAL_WS)
    {
        OutColor = vec4(VisualizeNormal(normalWS), 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_NORMAL_TEXTURE_TS)
    {
        OutColor = vec4(VisualizeNormal(normalTS), 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_MAPPED_NORMAL_WS)
    {
        OutColor = vec4(VisualizeNormal(mappedNormalWS), 1.0);
        return;
    }

    if (Material.RenderMode == RENDER_MODE_WIREFRAME)
    {
        float Diffuse = max(dot(mappedNormalWS, normalize(vec3(0.35,0.55,1.0))), 0.0);
        OutColor = vec4(albedo.rgb * (Material.AmbientLight + (1.0-Material.AmbientLight) * Diffuse), albedo.a);
        return;
    }
    OutColor = vec4(ShadeSurface(albedo.rgb, mappedNormalWS, Material.CameraPosition.xyz - FragWorldPosition,
                               Material.DemoOptions.x, Material.AmbientLight), albedo.a);
}
