#version 450

layout(location = 0) out vec3 FragColor;

const vec2 Positions[3] = vec2[](
    vec2( 0.0, -0.55),
    vec2( 0.55, 0.45),
    vec2(-0.55, 0.45)
);

const vec3 Colors[3] = vec3[](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.2),
    vec3(0.2, 0.4, 1.0)
);

void main()
{
    gl_Position = vec4(Positions[gl_VertexIndex], 0.0, 1.0);
    FragColor = Colors[gl_VertexIndex];
}
