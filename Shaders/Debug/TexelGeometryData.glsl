#ifndef MDSS_TEXEL_GEOMETRY_DATA
#define MDSS_TEXEL_GEOMETRY_DATA
struct TTexelGeometryVertex
{
    vec4 PositionAndHeight; // xyz: mesh-local display position, w: scaled total height
    vec4 Normal;            // xyz: normal of the displayed height field
};
#endif
