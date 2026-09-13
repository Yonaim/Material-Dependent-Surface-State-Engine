#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 3) in vec4 InTangent;

layout(push_constant) uniform StaticMeshPushConstants
{
    mat4 Model;
    mat4 ViewProjection;
} Push;

layout(location = 0) out vec3 FragNormal;
layout(location = 1) out vec3 FragTangent;
layout(location = 2) out float FragTangentSign;
layout(location = 3) out vec2 FragUV;

void main()
{
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragTangent = normalize(mat3(Push.Model) * InTangent.xyz);
    FragTangentSign = InTangent.w;
    FragUV = InUV;

    gl_Position = Push.ViewProjection * Push.Model * vec4(InPosition, 1.0);
}
