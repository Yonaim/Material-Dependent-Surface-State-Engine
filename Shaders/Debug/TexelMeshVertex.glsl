/**
 * @file TexelMeshVertex.glsl
 * @brief 원본 mesh topology는 유지하고, 샘플한 표시 높이만 동적으로 적용한다.
 */
#include "Surface/SurfaceStateData.glsl"
layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec4 InUVSurface;
layout(location = 3) in vec3 InDisplacementNormal;
layout(location = 4) in uvec4 InSamples;
layout(location = 5) in vec4 InWeights;
#include "Debug/TexelGeometryData.glsl"
layout(set = 2, binding = 0, std430) readonly buffer TComputedVertices
{
    TTexelGeometryVertex Values[];
} Computed;
layout(push_constant) uniform TPush { mat4 Model; mat4 ViewProjection; } Push;
layout(location = 0) out vec3 FragNormal;
layout(location = 1) out vec3 FragTangent;
layout(location = 2) out float FragTangentSign;
layout(location = 3) out vec2 FragUV;
layout(location = 4) flat out uint FragSurfaceIndex;
layout(location = 5) out vec3 FragMesoNormalWS;
layout(location = 6) out vec3 FragWorldPosition;
void main()
{
    float Height = 0.0;
    vec3 NormalOffset = vec3(0);
    float Weight = 0.0;
    for (uint K = 0u; K < 4u; ++K)
    {
        uint T = InSamples[K];
        if (InWeights[K] <= 0.0 || T >= uint(Computed.Values.length())) continue;
        Height += InWeights[K] * Computed.Values[T].HeightAndNormal.x;
        // 원본 정점의 smooth/hard normal을 유지하면서 샘플된 Meso/높이 변화량만 더한다.
        NormalOffset += InWeights[K] * (Computed.Values[T].HeightAndNormal.yzw - Normals.Values[T].xyz);
        Weight += InWeights[K];
    }
    vec3 Normal = InNormal + NormalOffset;
    if (Weight == 0.0 || dot(Normal, Normal) < 1e-12) Normal = InNormal;
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragMesoNormalWS = normalize(NormalMatrix * Normal);
    FragTangent = normalize(cross(abs(FragNormal.z) < 0.9 ? vec3(0,0,1) : vec3(0,1,0), FragNormal));
    FragTangentSign = 1.0;
    FragUV = InUVSurface.xy;
    FragSurfaceIndex = uint(InUVSurface.w);
    vec3 Position = InPosition + InDisplacementNormal * Height;
    vec4 WorldPosition = Push.Model * vec4(Position, 1.0);
    FragWorldPosition = WorldPosition.xyz;
    gl_Position = Push.ViewProjection * WorldPosition;
}
