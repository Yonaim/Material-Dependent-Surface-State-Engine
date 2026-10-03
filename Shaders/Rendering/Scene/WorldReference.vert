/**
 * @file WorldReference.vert
 * @brief World Reference의 grid와 axis 정점을 변환하고 가장자리 좌표를 전달한다.
 */
#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;
layout(location = 2) in float InEdgeCoordinate;

layout(push_constant) uniform TWorldReferencePushConstants
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
    gl_Position = Push.ViewProjectionModel * vec4(InPosition, 1.0);
}
