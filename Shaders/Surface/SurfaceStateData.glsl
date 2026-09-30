// Read-only bindings shared by State diagnostics and appearance sampling.
#ifndef MDSS_SURFACE_STATE_DATA
#define MDSS_SURFACE_STATE_DATA
#ifndef SURFACE_DEBUG_SET
#define SURFACE_DEBUG_SET 1
#endif
struct TSurfaceGPUProfileParameters { vec4 CapacityInputAndTransfer; vec4 DecayAndGeometry; vec4 AccumulationThickness; };
struct TSurfaceGPUGeometryScalar { float MesoVirtualHeight; float ConcavityWeight; float MesoMeanCurvature; float MesoGaussianCurvature; };
struct TSurfaceGPUNeighborIndices { uint Indices[8]; };
layout(std430, set = SURFACE_DEBUG_SET, binding = 0) readonly buffer TSurfaceTexelSurfaceIndices { uint Values[]; } TexelSurfaceIndices;
layout(std430, set = SURFACE_DEBUG_SET, binding = 1) readonly buffer TSurfaceTexelProfileIndices { uint Values[]; } TexelProfileIndices;
layout(std430, set = SURFACE_DEBUG_SET, binding = 2) readonly buffer TSurfacePositions { vec4 Values[]; } Positions;
layout(std430, set = SURFACE_DEBUG_SET, binding = 3) readonly buffer TSurfaceNormals { vec4 Values[]; } Normals;
layout(std430, set = SURFACE_DEBUG_SET, binding = 4) readonly buffer TSurfaceGeometryScalars { TSurfaceGPUGeometryScalar Values[]; } GeometryScalars;
layout(std430, set = SURFACE_DEBUG_SET, binding = 5) readonly buffer TSurfaceNeighborIndices { TSurfaceGPUNeighborIndices Values[]; } NeighborIndices;
layout(std430, set = SURFACE_DEBUG_SET, binding = 6) readonly buffer TSurfaceProfileParameters { TSurfaceGPUProfileParameters Values[]; } ProfileParameters;
layout(std430, set = SURFACE_DEBUG_SET, binding = 7) readonly buffer TSurfaceProfileSupported { uint Values[]; } ProfileSupported;
layout(std430, set = SURFACE_DEBUG_SET, binding = 8) readonly buffer TSurfaceCurrentState { float Values[]; } CurrentState;
layout(std430, set = SURFACE_DEBUG_SET, binding = 12) readonly buffer TSurfaceRanges { uvec4 Values[]; } SurfaceRanges;
layout(std430, set = SURFACE_DEBUG_SET, binding = 17) readonly buffer TSurfaceMesoNormals { vec4 Values[]; } MesoNormals;
layout(std430, set = SURFACE_DEBUG_SET, binding = 20) readonly buffer TSurfaceWorldTexelAreas { float Values[]; } WorldTexelAreas;

#endif
