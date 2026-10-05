/**
 * @file OverlaySide.vert
 * @brief compute pass가 만든 옆면 segment를 6개 정점으로 펼쳐 world-space 조명 입력을 만든다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

struct TSideSegment
{
    vec4 BaseA;
    vec4 TopA;
    vec4 BaseB;
    vec4 TopB;
    vec4 Inside;
};

layout(set = 3, binding = 3, std430) readonly buffer TSegments { TSideSegment Values[]; } Segments;

layout(push_constant) uniform TPush { mat4 Model; mat4 ViewProjection; } Push;

layout(location = 0) out vec3 FragNormal;

layout(location = 1) out vec2 FragUV;

layout(location = 2) flat out uint FragSurfaceIndex;

layout(location = 3) out vec3 FragWorldPosition;

layout(location = 4) out float FragCoverage;

void main()
{
    // 각 정점은 segment 내 corner 번호로 결정되며, 무효 segment는 clip space 밖으로 보낸다.
    uint SegmentIndex = uint(gl_VertexIndex) / 6u;
    uint Corner = uint(gl_VertexIndex) % 6u;
    TSideSegment S = Segments.Values[SegmentIndex];
    if (S.Inside.w < 0.0)
    {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        FragNormal = vec3(0.0, 0.0, 1.0);
        FragUV = vec2(0.0);
        FragSurfaceIndex = 0u;
        FragWorldPosition = vec3(0.0);
        FragCoverage = 0.0;
        return;
    }
    vec3 BaseA = vec3(Push.Model * vec4(S.BaseA.xyz, 1.0));
    vec3 TopA = vec3(Push.Model * vec4(S.TopA.xyz, 1.0));
    vec3 BaseB = vec3(Push.Model * vec4(S.BaseB.xyz, 1.0));
    vec3 TopB = vec3(Push.Model * vec4(S.TopB.xyz, 1.0));
    vec3 Inside = vec3(Push.Model * vec4(S.Inside.xyz, 1.0));
    vec3 Edge = BaseB - BaseA;
    vec3 Up = 0.5 * (TopA + TopB - BaseA - BaseB);
    vec3 N = cross(Edge, Up);
    // 높이가 거의 0인 면에서도 가능한 normal을 만들고 안쪽 방향을 기준으로 뒤집는다.
    if (dot(N, N) < 1e-16) N = cross(Edge, TopA - BaseA);
    if (dot(N, N) < 1e-16) N = vec3(0.0, 0.0, 1.0);
    N = normalize(N);
    if (dot(N, 0.5 * (BaseA + BaseB) - Inside) < 0.0) N = -N;
    const uint Indices[6] = uint[6](0u, 2u, 3u, 0u, 3u, 1u);
    uint Vertex = Indices[Corner];
    FragWorldPosition = Vertex == 0u ? BaseA : Vertex == 1u ? TopA : Vertex == 2u ? BaseB : TopB;
    FragUV = Vertex < 2u ? vec2(S.BaseA.w, S.TopA.w) : vec2(S.BaseB.w, S.TopB.w);
    FragNormal = N;
    FragSurfaceIndex = uint(S.Inside.w);
    FragCoverage = 1.0;
    gl_Position = Push.ViewProjection * vec4(FragWorldPosition, 1.0);
}
