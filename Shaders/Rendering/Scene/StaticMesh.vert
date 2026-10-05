/**
 * @file StaticMesh.vert
 * @brief Static Mesh 정점을 변환해 월드 공간의 normal과 tangent, UV를 전달한다.
 */
#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 1) in vec3 InNormal;

layout(location = 2) in vec2 InUV;

layout(location = 3) in vec4 InTangent;

layout(push_constant) uniform TStaticMeshPushConstants
{
    mat4 Model;
    mat4 ViewProjection;
} Push;

layout(location = 0) out vec3 FragNormal;

layout(location = 1) out vec3 FragTangent;

layout(location = 2) out float FragTangentSign;

layout(location = 3) out vec2 FragUV;

layout(location = 4) out vec3 FragWorldPosition;

void main()
{
    // normal에는 inverse-transpose를, tangent에는 model의 선형 변환을 적용한다.
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragTangent = normalize(mat3(Push.Model) * InTangent.xyz);
    FragTangentSign = InTangent.w;
    FragUV = InUV;

    FragWorldPosition = vec3(Push.Model * vec4(InPosition, 1.0));
    gl_Position = Push.ViewProjection * Push.Model * vec4(InPosition, 1.0);
}
