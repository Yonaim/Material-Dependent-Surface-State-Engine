/**
 * @file OverlayTop.vert
 * @brief overlay 상단 mesh에 texel 높이, normal, coverage를 보간해 표면 위에 배치한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Rendering/Surface/StateSampling.glsl"
#include "Rendering/Surface/MaterialParameters.glsl"
#include "Debug/TexelGeometryData.glsl"

layout(location = 0) in vec3 InPosition;

layout(location = 1) in vec3 InNormal;

layout(location = 2) in vec4 InUVSurface;

layout(location = 3) in vec3 InDisplacementNormal;

layout(location = 4) in uvec4 InSamples;

layout(location = 5) in vec4 InWeights;

layout(set = 2, binding = 0, std430) readonly buffer TComputedVertices
{
    TTexelGeometryVertex Values[];
} Computed;

layout(set = 3, binding = 4, std430) readonly buffer TCoverageValues
{
    float Values[];
} Coverage;

layout(push_constant) uniform TPush { mat4 Model; mat4 ViewProjection; } Push;

layout(location = 0) out vec3 FragNormal;

layout(location = 1) out vec2 FragUV;

layout(location = 2) flat out uint FragSurfaceIndex;

layout(location = 3) out vec3 FragWorldPosition;

layout(location = 4) out float FragCoverage;

layout(location = 5) flat out uint FragIsSide;

layout(location = 6) flat out mat3 FragNormalMatrix;


void main()
{
    // 각 mesh 정점이 참조하는 최대 네 texel의 형상을 bilinear weight로 보간한다.
    float Height = 0.0;
    vec3 DisplacedNormal = vec3(0.0);
    float Weight = 0.0;
    for (uint K = 0u; K < 4u; ++K)
    {
        uint T = InSamples[K];
        if (InWeights[K] <= 0.0 || T >= uint(Computed.Values.length())) continue;
        Height += InWeights[K] * Computed.Values[T].HeightAndNormal.x;
        DisplacedNormal += InWeights[K] * Computed.Values[T].HeightAndNormal.yzw;
        Weight += InWeights[K];
    }
    vec3 Normal = InNormal;
    // Use the height-field normal directly. The prior additive correction preserved
    // the source shading normal and could retain its triangle facets on the overlay.
    if (Weight > 0.0 && dot(DisplacedNormal, DisplacedNormal) > 1e-12)
        Normal = normalize(DisplacedNormal);
    if (Material.DemoExtraStateChannels.y == 4u) Normal = InNormal;
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragIsSide = 0u;
    FragNormalMatrix = NormalMatrix;
    FragNormal = normalize(NormalMatrix * Normal);
    FragUV = InUVSurface.xy;
    FragSurfaceIndex = uint(InUVSurface.w);
    uint Vertex = uint(gl_VertexIndex);
    FragCoverage = Vertex < uint(Coverage.Values.length()) ? Coverage.Values[Vertex] : 0.0;
    vec3 Position = InPosition + InDisplacementNormal * Height;
    vec4 WorldPosition = Push.Model * vec4(Position, 1.0);
    FragWorldPosition = WorldPosition.xyz;
    gl_Position = Push.ViewProjection * WorldPosition;
}
