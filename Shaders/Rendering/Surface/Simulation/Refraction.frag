#version 450

// 불투명 패스의 색과 깊이를 읽는 Surface Simulation 굴절 경로를 컴파일한다.
#extension GL_GOOGLE_include_directive : require
// ============================================================
// 00. Refraction Fragment Entry
// ============================================================
// #region 00_Refraction_Fragment_Entry
    // 불투명 색과 깊이 입력을 포함하도록 공통 표시 셰이더를 구성한다.

    #define WATER_REFRACTION
    #include "SurfaceFragmentMain.glsl"

// #endregion
