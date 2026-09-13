#version 450

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec2 FragUV;

layout(location = 0) out vec4 OutColor;

void main()
{
    vec3 Normal = normalize(FragNormal);
    vec3 LightDirection = normalize(vec3(0.35, 0.55, 1.0));

    float Diffuse = 0.35 + 0.65 * max(dot(Normal, LightDirection), 0.0);
    vec3 FaceColor = 0.25 + 0.75 * abs(Normal);
    vec3 UVTint = vec3(0.9 + 0.1 * FragUV.x, 0.9 + 0.1 * FragUV.y, 1.0);

    OutColor = vec4(FaceColor * Diffuse * UVTint, 1.0);
}
