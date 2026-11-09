/**
 * @file Gizmo.frag
 * @brief Object Gizmo의 vertex color를 fragment color로 출력한다.
 */
#version 450

// ============================================================
// 00. Resource Interface
// ============================================================
// #region 00_Resource_Interface

layout(location = 0) in vec4 FragColor;
layout(location = 0) out vec4 OutColor;

// #endregion

// ============================================================
// 01. Main Process
// ============================================================
// #region 01_Main_Process

void main()
{
    OutColor = FragColor;
}

// #endregion
