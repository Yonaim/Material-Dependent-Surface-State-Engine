#version 450

// 불투명 장면의 색 및 깊이 샘플링이 필요 없는 Surface Simulation 표시 경로다.
#extension GL_GOOGLE_include_directive : require
// ============================================================
// 00. Surface Fragment Entry
// ============================================================
// #region 00_Surface_Fragment_Entry
    // 굴절 텍스처 입력이 없는 공통 표시 셰이더를 사용한다.

    #include "SurfaceFragmentMain.glsl"

// #endregion
