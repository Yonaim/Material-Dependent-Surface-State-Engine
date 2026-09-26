#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;

layout(push_constant) uniform TGizmoPushConstants
{
    mat4 ViewProjectionModel;
} Push;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = InColor;
    gl_Position = Push.ViewProjectionModel * vec4(InPosition, 1.0);
}
