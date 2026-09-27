#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 3) in vec4 InTangent;

layout(set = 0, binding = 2) uniform MaterialParameters
{
    vec4 BaseColor;
    uint RenderMode;
    uint FlipNormalY;
    float NormalStrength;
    float AmbientLight;
    uint DebugStateChannel;
    uint StateChannelCount;
    float DebugPadding0;
    float ReliefShadingEnabled;
} Material;

bool MaterialRenderModeIsMesoOffset()
{
    return Material.RenderMode == 12u;
}

struct TSurfaceGPUGeometryScalar
{
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
};

layout(std430, set = 1, binding = 4) readonly buffer TSurfaceGeometryScalars
{
    TSurfaceGPUGeometryScalar Values[];
} GeometryScalars;
layout(std430, set = 1, binding = 12) readonly buffer TSurfaceRanges
{
    uvec4 Values[];
} SurfaceRanges;
layout(std430, set = 1, binding = 0) readonly buffer TSurfaceTexelSurfaceIndices
{
    uint Values[];
} TexelSurfaceIndices;
layout(std430, set = 1, binding = 17) readonly buffer TSurfaceMesoNormals
{
    vec4 Values[];
} MesoNormals;

layout(push_constant) uniform TStaticMeshPushConstants
{
    mat4 Model;
    mat4 ViewProjection;
} Push;

layout(location = 0) out vec3 FragNormal;
layout(location = 1) out vec3 FragTangent;
layout(location = 2) out float FragTangentSign;
layout(location = 3) out vec2 FragUV;
layout(location = 4) flat out uint FragSurfaceIndex;
layout(location = 5) out vec3 FragMesoNormalWS;

void main()
{
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragTangent = normalize(mat3(Push.Model) * InTangent.xyz);
    FragTangentSign = InTangent.w;
    FragUV = InUV;
    FragSurfaceIndex = gl_InstanceIndex;
    // 상태 히트맵의 입체 음영에 쓸 복원 노멀을 정점에서 찾아 화면 공간으로 변환한다.
    FragMesoNormalWS = FragNormal;
    uint Surface = uint(gl_InstanceIndex);
    if (Surface < uint(SurfaceRanges.Values.length()))
    {
        uvec4 Range = SurfaceRanges.Values[Surface];
        if (Range.y > 0u && Range.z > 0u)
        {
            uvec2 XY = min(uvec2(floor(clamp(InUV, 0.0, 1.0) * vec2(Range.yz))), Range.yz - 1u);
            uint TexelIndex = Range.x + XY.y * Range.y + XY.x;
            if (TexelIndex < uint(MesoNormals.Values.length()))
                FragMesoNormalWS = normalize(NormalMatrix * MesoNormals.Values[TexelIndex].xyz);
        }
    }
    vec3 LocalPosition = InPosition;
    if (MaterialRenderModeIsMesoOffset())
    {
        // 렌더 정점 UV에 대응하는 시뮬레이션 텍셀 높이만큼 원본 정점을 이동한다.
        uint Surface = uint(gl_InstanceIndex);
        if (Surface < uint(SurfaceRanges.Values.length()))
        {
            uvec4 Range = SurfaceRanges.Values[Surface];
            if (Range.y > 0u && Range.z > 0u)
            {
                uvec2 XY = min(uvec2(floor(clamp(InUV, 0.0, 1.0) * vec2(Range.yz))), Range.yz - 1u);
                uint TexelIndex = Range.x + XY.y * Range.y + XY.x;
                if (TexelIndex < uint(GeometryScalars.Values.length()) &&
                    TexelIndex < uint(TexelSurfaceIndices.Values.length()) &&
                    TexelSurfaceIndices.Values[TexelIndex] == Surface)
                {
                    LocalPosition += InNormal * GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
                }
            }
        }
    }
    gl_Position = Push.ViewProjection * Push.Model * vec4(LocalPosition, 1.0);
}
