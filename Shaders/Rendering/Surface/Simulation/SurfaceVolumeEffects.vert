#version 450

// 불꽃 적분 영역을 월드 공간으로 회전하고 프래그먼트에는 로컬 위치(m)를 전달한다.
#extension GL_GOOGLE_include_directive : require

// ============================================================
// 00. Flame Vertex Entry
// ============================================================
// #region 00_Flame_Vertex_Entry
    // 불꽃 영역 표면의 로컬 정점을 받아 프래그먼트의 광선 교차 계산에 전달한다.

    layout(location = 0) in vec3 InPosition;
    layout(location = 0) out vec3 Local;

    #include "SurfaceDrawParameters.glsl"

    void main()
    {
        Local = InPosition;
        float s = Push.Pose.x;
        float c = Push.Pose.y;
        vec3 world = vec3(c * Local.x + s * Local.z, Local.y, -s * Local.x + c * Local.z);
        gl_Position = Push.ViewProjection * vec4(world, 1.0);
    }

// #endregion
