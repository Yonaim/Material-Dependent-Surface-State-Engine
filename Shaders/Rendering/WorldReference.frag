/**
 * @file WorldReference.frag
 * @brief World Reference grid의 가장자리를 부드럽게 fade하고 world axis 색상을 출력한다.
 */
#version 450

layout(location = 0) in vec4 FragColor;
layout(location = 1) in float EdgeCoordinate;
layout(location = 0) out vec4 OutColor;

void main()
{
    if (EdgeCoordinate < -1.5)
    {
        OutColor = FragColor;
        return;
    }

    float EdgeDistance = abs(EdgeCoordinate);
    float EdgeWidth = max(fwidth(EdgeDistance), 1e-4);
    float Alpha = 1.0 - smoothstep(1.0 - EdgeWidth, 1.0 + EdgeWidth, EdgeDistance);
    OutColor = vec4(FragColor.rgb, FragColor.a * Alpha);
}
