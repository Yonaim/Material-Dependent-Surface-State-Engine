/**
 * @file Gizmo.vert
 * @brief Object Gizmo 정점을 변환하고 hover 상태인 축을 강조한다.
 */
#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in float InEdgeCoordinate;

layout(push_constant) uniform TGizmoPushConstants
{
    mat4 ViewProjectionModel;
    int HighlightAxis;
    int Padding0;
    int Padding1;
    int Padding2;
} Push;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out float EdgeCoordinate;

void main()
{
    FragColor = InColor;
    EdgeCoordinate = InEdgeCoordinate;
    if ((Push.HighlightAxis == 0 && InColor.r > 0.9 && InColor.g < 0.2) ||
        (Push.HighlightAxis == 1 && InColor.g > 0.9 && InColor.r < 0.2) ||
        (Push.HighlightAxis == 2 && InColor.b > 0.9 && InColor.r < 0.2))
    {
        FragColor = vec4(1.0, 0.9, 0.05, 1.0);
    }
    gl_Position = Push.ViewProjectionModel * vec4(InPosition, 1.0);
}
