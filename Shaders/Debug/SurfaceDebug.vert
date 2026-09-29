/**
 * @file SurfaceDebug.vert
 * @brief Surface Debug 데이터를 준비하고 Meso/Final Geometry 모드에서 정점을 변위한다.
 */
#version 450
#extension GL_GOOGLE_include_directive : require

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
    float DebugViewParameter;
    float ReliefShadingEnabled;
    vec4 DebugOptions; // Raw State max, height max, preview height reference, displacement scale
    uvec4 DebugFlags; // Raw State, Accumulation component, reserved, reserved
} Material;

#include "Debug/SurfaceDebugData.glsl"

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
layout(location = 6) out vec3 FragWorldPosition;

void main()
{
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragTangent = normalize(mat3(Push.Model) * InTangent.xyz);
    FragTangentSign = InTangent.w;
    FragUV = InUV;
    FragSurfaceIndex = gl_InstanceIndex;
    // 상태 히트맵에 입체감을 더할 Meso Normal을 조회해 월드 공간으로 변환한다.
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
    // Texel Area 진단은 Meso Offset과 Normal Map을 적용하기 전의 위치와 instance scale을 사용한다.
    FragWorldPosition = vec3(Push.Model * vec4(InPosition, 1.0));
    if (Material.RenderMode == 14u || Material.RenderMode == 19u)
    {
        // 정점 UV에 해당하는 simulation texel의 Meso Virtual Height만큼 정점을 이동한다.
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
                    float Height = GeometryScalars.Values[TexelIndex].MesoVirtualHeight;
                    if (Material.RenderMode == 19u)
                    {
                        TDebugAccumulation D = DebugAccumulation(TexelIndex, Material.DebugStateChannel, Material.StateChannelCount, Material.DebugOptions.z);
                        if (D.Status == 3u) Height += D.Height;
                        if (TexelIndex < uint(MesoNormals.Values.length()))
                            FragMesoNormalWS = normalize(NormalMatrix * DebugFinalNormal(TexelIndex, Material.DebugStateChannel, Material.StateChannelCount, Material.DebugOptions.z));
                        Height *= Material.DebugOptions.w;
                    }
                    LocalPosition += InNormal * Height;
                }
            }
        }
    }
    gl_Position = Push.ViewProjection * Push.Model * vec4(LocalPosition, 1.0);
}
