// 진단용 표시 높이와 height-field normal을 정점마다 전달하는 GPU 데이터다.
#ifndef MDSS_TEXEL_GEOMETRY_DATA
#define MDSS_TEXEL_GEOMETRY_DATA
struct TTexelGeometryVertex
{
    vec4 HeightAndNormal; // x: scaled total height, yzw: displayed height-field normal
};
#endif
