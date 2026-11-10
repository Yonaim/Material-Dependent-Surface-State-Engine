// Gizmo.vert — Object Gizmo 정점을 변환하고 hover 상태인 축을 강조한다.
#version 450

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(push_constant) uniform TGizmoPushConstants
{
    mat4 ViewProjectionModel;
    int HighlightAxis;
    int Padding0;
    int Padding1;
    int Padding2;
}

Push;

layout(location = 0) out vec4 FragColor;

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process
// Object Gizmo 정점을 변환하고 hover 상태인 축을 강조한다.

void main()
{
    // 선택된 axis의 색을 강조하고 gizmo 정점을 clip space로 변환한다.
    FragColor = InColor;

    if ((Push.HighlightAxis == 0 && InColor.r > 0.9 && InColor.g < 0.2) ||
        (Push.HighlightAxis == 1 && InColor.g > 0.9 && InColor.r < 0.2) ||
        (Push.HighlightAxis == 2 && InColor.b > 0.9 && InColor.r < 0.2))
    {
        FragColor = vec4(1.0, 0.9, 0.05, 1.0);
    }

    gl_Position = Push.ViewProjectionModel * vec4(InPosition, 1.0);
}

// #endregion
