// SurfaceSolverCommon.glsl — Solver 셰이더가 공유하는 binding·State 접근·수송 계산을 연결한다.
#ifndef MDSS_SURFACE_SOLVER_COMMON_GLSL
#define MDSS_SURFACE_SOLVER_COMMON_GLSL

// ============================================================
// 00. Solver Dependencies
// ============================================================
// #region 00_Solver_Dependencies
// 각 Solver stage에서 동일한 binding·주소·수송 helper를 포함한다.

#include "Simulation/Solver/SolverBindings.glsl"
#include "Simulation/Solver/SparseSchedule.glsl"
#include "Simulation/Solver/FluxCache.glsl"
#include "Simulation/Solver/StateMath.glsl"
#include "Simulation/Solver/GeometryDrive.glsl"
#include "Simulation/Solver/Transport.glsl"
// #endregion

#endif
