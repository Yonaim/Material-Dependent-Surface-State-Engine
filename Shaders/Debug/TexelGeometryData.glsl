#ifndef MDSS_TEXEL_GEOMETRY_DATA
#define MDSS_TEXEL_GEOMETRY_DATA
struct TTexelGeometryVertex
{
    vec4 HeightAndNormal; // x: scaled total height, yzw: displayed height-field normal
};
#endif
