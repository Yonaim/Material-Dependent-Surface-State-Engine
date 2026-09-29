#version 450
#extension GL_GOOGLE_include_directive : require
#include "Debug/SurfaceDebugData.glsl"
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
    uint T = uint(gl_VertexIndex); // index buffer contains absolute simulation texel indices
    uint Surface = TexelSurfaceIndices.Values[T];
    uvec4 Range = SurfaceRanges.Values[Surface];
    uint Local = T - Range.x;
    FragUV = (vec2(Local % Range.y, Local / Range.y) + 0.5) / vec2(Range.yz);
    FragSurfaceIndex = Surface;
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * Normals.Values[T].xyz);
    FragMesoNormalWS = normalize(NormalMatrix * Computed.Values[T].Normal.xyz);
    FragTangent = normalize(cross(abs(FragNormal.z) < 0.9 ? vec3(0,0,1) : vec3(0,1,0), FragNormal));
    FragTangentSign = 1.0;
    FragWorldPosition = vec3(Push.Model * vec4(Positions.Values[T].xyz, 1.0));
    gl_Position = Push.ViewProjection * Push.Model * vec4(Computed.Values[T].PositionAndHeight.xyz, 1.0);
}
