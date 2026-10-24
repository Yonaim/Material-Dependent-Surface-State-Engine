#ifndef MDSS_SURFACE_TYPES_GLSL
#define MDSS_SURFACE_TYPES_GLSL
struct TSurfaceGPUProfileParameters
{
    // profile의 용량/입력/transfer, 감쇠/형상, 누적 두께 계수 묶음이다.
    // CPU의 TSurfaceGPUProfileParameters와 vec4 3개 순서 및 크기가 일치해야 한다.
    vec4 CapacityInputAndTransfer;
    vec4 DecayAndGeometry;
    vec4 AccumulationThickness;
};

struct TSurfaceGPUGeometryScalar
{
    // meso 형상 높이와 오목함, 곡률 등 texel별 기하 값이다.
    float MesoVirtualHeight;
    float ConcavityWeight;
    float MesoMeanCurvature;
    float MesoGaussianCurvature;
};

#endif
