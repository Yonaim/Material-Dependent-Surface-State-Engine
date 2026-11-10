// TexelGeometryData.glsl — 진단용 표시 높이와 height-field normal을 정점마다 전달하는 GPU 데이터다.
#ifndef MDSS_TEXEL_GEOMETRY_DATA
#define MDSS_TEXEL_GEOMETRY_DATA

// ============================================================
// 00. Vertex Data
// ============================================================
// #region 00_Vertex_Data
// CPU 렌더 메시와 shader가 같은 순서로 해석할 정점 입력을 선언한다.

struct TTexelGeometryVertex
{
    vec4 HeightAndNormal; // x: scaled total height, yzw: displayed height-field normal
};

// #endregion

#endif
