#version 450

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;

layout(set = 0, binding = 0) uniform sampler2D BaseColorTexture;
layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

layout(set = 0, binding = 2) uniform MaterialParameters
{
    vec4 BaseColor;
    uint RenderMode;
    uint FlipNormalY;
    float NormalStrength;
    float AmbientLight;
} Material;

layout(location = 0) out vec4 OutColor;

const uint RENDER_MODE_LIT = 0u;
const uint RENDER_MODE_BASE_COLOR = 1u;
const uint RENDER_MODE_VERTEX_NORMAL_WS = 2u;
const uint RENDER_MODE_NORMAL_TEXTURE_TS = 3u;
const uint RENDER_MODE_MAPPED_NORMAL_WS = 4u;

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

    vec3 lightDirectionWS = normalize(vec3(0.35, 0.55, 1.0));
    float directLight = max(dot(mappedNormalWS, lightDirectionWS), 0.0);
    float diffuse = Material.AmbientLight + (1.0 - Material.AmbientLight) * directLight;
    OutColor = vec4(albedo.rgb * diffuse, albedo.a);
}
