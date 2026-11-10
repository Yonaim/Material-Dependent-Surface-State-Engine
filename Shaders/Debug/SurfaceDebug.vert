// SurfaceDebug.vert — Macro mesh 기반 Surface Debug 데이터를 준비한다. 높이 형상은 TexelGeometry.vert를 사용한다.
#version 450
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUV;
layout(location = 3) in vec4 InTangent;

// CPU: RendererShared.h의 FMaterialUniform prefix; RendererMaterials.cpp에서 업로드한다.
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
    vec4 DebugOptions; // reserved, reserved, Lit height display scale, displacement scale
    uvec4 DebugFlags;  // reserved, Accumulation component, height grid mode, height grid block size
}

Material;

#include "Debug/SurfaceDebugData.glsl"

layout(push_constant) uniform TStaticMeshPushConstants
{
    mat4 Model;
    mat4 ViewProjection;
}

Push;

layout(location = 0) out vec3 FragNormal;
layout(location = 1) out vec3 FragTangent;
layout(location = 2) out float FragTangentSign;
layout(location = 3) out vec2 FragUV;
layout(location = 4) flat out uint FragSurfaceIndex;
layout(location = 5) out vec3 FragMesoNormalWS;
layout(location = 6) out vec3 FragWorldPosition;

void WriteMesoNormal(mat3 NormalMatrix);
void ApplyMesoOffset(inout vec3 LocalPosition);

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process
// Macro mesh 기반 Surface Debug 데이터를 준비한다. 높이 형상은 TexelGeometry.vert를 사용한다.

void main()
{
    mat3 NormalMatrix = transpose(inverse(mat3(Push.Model)));
    FragNormal = normalize(NormalMatrix * InNormal);
    FragTangent = normalize(mat3(Push.Model) * InTangent.xyz);
    FragTangentSign = InTangent.w;
    FragUV = InUV;

    // RendererCommands.cpp의 draw firstInstance는 Section.Surface를 전달한다.
    FragSurfaceIndex = gl_InstanceIndex;
    WriteMesoNormal(NormalMatrix);
    vec3 LocalPosition = InPosition;

    // Texel Area 진단은 Meso Offset과 Normal Map을 적용하기 전의 위치와 instance scale을 사용한다.
    FragWorldPosition = vec3(Push.Model * vec4(InPosition, 1.0));
    ApplyMesoOffset(LocalPosition);
    gl_Position = Push.ViewProjection * Push.Model * vec4(LocalPosition, 1.0);
}

// #endregion

// ============================================================
// 02. Detailed Implementation
// ============================================================
// #region 02_Detailed_Implementation
// Macro mesh 기반 Surface Debug 데이터를 준비한다. 높이 형상은 TexelGeometry.vert를 사용한다.

    // 02.00. Evaluate Meso and Final Normals
    // #region 02_00_Evaluate_Meso_and_Final_Normals
    // SurfaceRanges·MesoNormals 버퍼의 값을 읽어 이 구획의 수치 계산에 사용한다.

    void WriteMesoNormal(mat3 NormalMatrix)
    {
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
                {
                    FragMesoNormalWS = normalize(NormalMatrix * MesoNormals.Values[TexelIndex].xyz);
                }
            }
        }
    }

    // #endregion

    // 02.01. Apply Preview Position Displacement
    // #region 02_01_Apply_Preview_Position_Displacement
    // 계산 결과를 TexelSurfaceIndices 버퍼에 기록해 후속 GPU 단계에 전달한다.

    void ApplyMesoOffset(inout vec3 LocalPosition)
    {
        if (Material.RenderMode == 14u)
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
                        LocalPosition += InNormal * Height;
                    }
                }
            }
        }
    }

    // #endregion

// #endregion
