/**
 * @file Gizmo.frag
 * @brief Object Gizmo의 vertex color를 fragment color로 출력한다.
 */
#version 450

layout(location = 0) in vec4 FragColor;
layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = FragColor;
}
