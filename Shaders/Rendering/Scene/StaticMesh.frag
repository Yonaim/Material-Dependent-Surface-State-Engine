// StaticMesh.frag — Static Mesh의 기본 색상, Normal Map 조명과 렌더링 진단 모드를 처리한다.
#version 450
#extension GL_GOOGLE_include_directive : require
#include "Rendering/Surface/Lighting/MaterialParameters.glsl"
#include "Rendering/Surface/Lighting/Lighting.glsl"

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

layout(location = 0) in vec3 FragNormal;
layout(location = 1) in vec3 FragTangent;
layout(location = 2) in float FragTangentSign;
layout(location = 3) in vec2 FragUV;
layout(location = 4) in vec3 FragWorldPosition;
layout(set = 0, binding = 0) uniform sampler2D BaseColorTexture;

layout(set = 0, binding = 1) uniform sampler2D NormalTexture;

layout(location = 0) out vec4 OutColor;

const uint RENDER_MODE_LIT = 0u;

const uint RENDER_MODE_BASE_COLOR = 1u;

const uint RENDER_MODE_WIREFRAME = 2u;

const uint RENDER_MODE_VERTEX_NORMAL_WS = 3u;

const uint RENDER_MODE_NORMAL_TEXTURE_TS = 4u;

const uint RENDER_MODE_MAPPED_NORMAL_WS = 5u;

const uint RENDER_MODE_WIREFRAME_UNIFORM_WHITE = 20u;

vec3 VisualizeNormal(vec3 Normal);
vec3 ReconstructMaterialNormal(out vec3 NormalWS, out vec3 NormalTS);
vec4 EvaluateStaticMeshColor(vec4 Albedo, vec3 NormalWS, vec3 NormalTS, vec3 MappedNormalWS);


// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process
// Static Mesh의 기본 색상, Normal Map 조명과 렌더링 진단 모드를 처리한다.

void main()
{
    vec3 NormalWS;
    vec3 NormalTS;
    vec3 MappedNormalWS = ReconstructMaterialNormal(NormalWS, NormalTS);
    vec4 Albedo = texture(BaseColorTexture, FragUV) * Material.BaseColor;

    OutColor = EvaluateStaticMeshColor(Albedo, NormalWS, NormalTS, MappedNormalWS);
}
// #endregion

// ============================================================
// 02. Detailed Implementation
// ============================================================
// #region 02_Detailed_Implementation
// Static Mesh의 기본 색상, Normal Map 조명과 렌더링 진단 모드를 처리한다.


    // 02.00. Material Normal Reconstruction
    // #region 02_00_Material_Normal_Reconstruction
    // Tangent-space normal을 재구성하고 앞뒷면 방향을 반영해 world-space normal로 반환한다.

    vec3 ReconstructMaterialNormal(out vec3 normalWS, out vec3 normalTS)
    {
        // tangent-space normal map을 world-space normal로 변환한 뒤 선택된 진단 모드로 출력한다.
        normalWS = normalize(FragNormal);
        vec3 tangentWS = normalize(FragTangent - normalWS * dot(normalWS, FragTangent));
        vec3 bitangentWS = normalize(cross(normalWS, tangentWS)) * FragTangentSign;
        mat3 tangentToWorld = mat3(tangentWS, bitangentWS, normalWS);

        normalTS = texture(NormalTexture, FragUV).xyz * 2.0 - 1.0;

        if (Material.FlipNormalY != 0u)
        {
            normalTS.y = -normalTS.y;
        }

        normalTS.xy *= Material.NormalStrength;
        normalTS = normalize(normalTS);

        vec3 mappedNormalWS = normalize(tangentToWorld * normalTS);

        if (!gl_FrontFacing)
        {
            mappedNormalWS = -mappedNormalWS;
        }

        return mappedNormalWS;
    }

    // #endregion

    // 02.01. Display Mode and Lighting
    // #region 02_01_Display_Mode_and_Lighting
    // 선택된 진단 색상 또는 공통 조명 결과를 계산한다.

    vec4 EvaluateStaticMeshColor(vec4 Albedo, vec3 NormalWS, vec3 NormalTS, vec3 MappedNormalWS)
    {
        // 진단 모드는 색상과 각 좌표계 normal을 시각화해 입력 문제를 구분해 볼 수 있다.
        if (Material.RenderMode == RENDER_MODE_BASE_COLOR)
        {
            return Albedo;
        }

        if (Material.RenderMode == RENDER_MODE_VERTEX_NORMAL_WS)
        {
            return vec4(VisualizeNormal(NormalWS), 1.0);
        }

        if (Material.RenderMode == RENDER_MODE_NORMAL_TEXTURE_TS)
        {
            return vec4(VisualizeNormal(NormalTS), 1.0);
        }

        if (Material.RenderMode == RENDER_MODE_MAPPED_NORMAL_WS)
        {
            return vec4(VisualizeNormal(MappedNormalWS), 1.0);
        }

        if (Material.RenderMode == RENDER_MODE_WIREFRAME)
        {
            float Diffuse = max(dot(MappedNormalWS, normalize(vec3(0.35, 0.55, 1.0))), 0.0);
            return vec4(Albedo.rgb * (Material.AmbientLight + (1.0 - Material.AmbientLight) * Diffuse), Albedo.a);
        }

        if (Material.RenderMode == RENDER_MODE_WIREFRAME_UNIFORM_WHITE)
        {
            return vec4(1.0);
        }

        return vec4(ShadeSurface(Albedo.rgb,
                                     MappedNormalWS,
                                     Material.CameraPosition.xyz - FragWorldPosition,
                                     Material.DemoOptions.x,
                                     Material.AmbientLight),
                        Albedo.a);
    }

    // #endregion

vec3 VisualizeNormal(vec3 Normal)
{
    return Normal * 0.5 + 0.5;
}

// #endregion
