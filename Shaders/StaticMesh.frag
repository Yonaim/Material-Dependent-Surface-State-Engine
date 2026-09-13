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
}
Material;

layout(location = 0) out vec4 OutColor;

void main()
{
    vec3 normalWS = normalize(FragNormal);
    vec3 tangentWS = normalize(FragTangent - normalWS * dot(normalWS, FragTangent));
    vec3 bitangentWS = normalize(cross(normalWS, tangentWS)) * FragTangentSign;
    mat3 tangentToWorld = mat3(tangentWS, bitangentWS, normalWS);

    vec3 normalTS = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;

    normalTS = normalize(normalTS);

    vec3 mappedNormalWS = normalize(tangentToWorld * normalTS);

    vec4 albedo = texture(BaseColorTexture, FragUV) * Material.BaseColor;
    vec3 lightDirectionWS = normalize(vec3(0.35, 0.55, 1.0));
    float diffuse = 0.25 + 0.75 * max(dot(mappedNormalWS, lightDirectionWS), 0.0);

    OutColor = vec4(albedo.rgb * diffuse, albedo.a);
}