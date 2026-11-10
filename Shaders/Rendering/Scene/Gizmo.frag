// Gizmo.frag — Object Gizmo의 vertex color를 fragment color로 출력한다.
#version 450

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface
// CPU가 전달하는 리소스 선언과 shader의 접근 경계를 모아 ABI 대응을 확인한다.

layout(location = 0) in vec4 FragColor;
layout(location = 0) out vec4 OutColor;

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process
// Object Gizmo의 vertex color를 fragment color로 출력한다.

void main()
{
    OutColor = FragColor;
}

// #endregion
