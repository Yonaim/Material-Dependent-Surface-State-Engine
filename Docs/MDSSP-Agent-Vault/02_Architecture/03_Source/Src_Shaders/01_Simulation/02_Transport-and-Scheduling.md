# Compute Include — Transport / Sparse Scheduling

Pass 1/2가 공유하는 계산 include와 Sparse 실행 보조 pass·metadata의 위치를 정리한다.

**연결 문서**
- [[02_Architecture/01_Flows/03_Simulation/01_PreStep-and-Sparse-Scheduling|Pre-Step]]
- [[02_Architecture/03_Source/Src_Cpp/04_GPU/02_ABI-and-Packing|ABI/Packing]]

## Scheduling entry

| 파일 | 주요 코드 | 입력→출력과 다음 소비자 |
|---|---|---|
| [SurfaceSparseScheduleReset.comp](../../../../../../Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp) | `main`, solver/input/geometry schedule offsets | solver/height/geometry sparse metadata의 count·flag를 정리; 이후 sparse producer/dispatch가 사용 |

## Include-only computation과 layout

| 파일 | 주요 함수·선언 | 포함되는 이유 |
|---|---|---|
| [SurfaceSolverCommon.glsl](../../../../../../Shaders/Simulation/Solver/SurfaceSolverCommon.glsl) | common include aggregator | Bindings, schedule, cache, state math, geometry drive, transport를 두 Solver pass와 geometry pass에 모음 |
| [SolverBindings.glsl](../../../../../../Shaders/Simulation/Solver/SolverBindings.glsl) | descriptor blocks·push constants·mode predicates | CPU descriptor binding, GPU record buffer와 compile/runtime option을 pass에 선언 |
| [SparseSchedule.glsl](../../../../../../Shaders/Simulation/Solver/SparseSchedule.glsl) | `sparseWorkgroupCount`, neighbor/schedule indexing | sparse schedule metadata의 offset·workgroup 목록과 texel↔group index 계산 |
| [FluxCache.glsl](../../../../../../Shaders/Simulation/Solver/FluxCache.glsl) | RawFlux index/load helpers, finite checks | FP32/FP16·coalesced cache variant가 공유하는 flux scratch 주소 계산 |
| [StateMath.glsl](../../../../../../Shaders/Simulation/Solver/StateMath.glsl) | state/profile indexing, capacity/saturation helpers | channel/state/profile 조회와 공통 상태량 계산 |
| [GeometryDrive.glsl](../../../../../../Shaders/Simulation/Solver/GeometryDrive.glsl) | source geometry preparation, `geometryDrive` | source surface basis를 미리 준비하고 target 방향의 geometry drive 계산 |
| [Transport.glsl](../../../../../../Shaders/Simulation/Solver/Transport.glsl) | `rawFlux` | Saturation·Geometry·concavity 기여를 조합해 방향별 제한 전 flux 후보 계산 |

## 구현 경계

Flux 계산식은 [[02_Architecture/02_Contracts/03_Solver|Solver 계약]]에 둔다. Cache ON/OFF 및 sparse/full 실행 경로는 결과 계산의 의미가 아니라 비용·스케줄 선택이다.

## 추가로 확인한 Sparse / Cache 의존성

- [Pass2](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp)는 cache ON일 때 `loadCachedRawEdgeFlux`, OFF일 때 이웃에서 `rawFlux` 재계산을 사용한다. 두 경로는 다른 memory/ALU trade-off이므로 성능·정밀도 결과를 혼용하지 않는다.
- [SparseSchedule.glsl](../../../../../../Shaders/Simulation/Solver/SparseSchedule.glsl)와 [SurfaceSparseScheduleReset.comp](../../../../../../Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp)는 sparse workgroup/active channel bit mask 및 indirect command 주소와 맞물린다. scheduling metadata는 [SparseMetadataLayout.cpp](../../../../../../Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp)가 제공한다.
- CPU [SurfaceStateSolver.cpp](../../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)에서 sparse는 `vkCmdDispatchIndirect(CommandBuffer, Item.HeightBuffer, Item.SolverIndirectOffset)`를 사용한다. persistent activity seed flag에서는 full dispatch로 우회할 수 있다.
- 검사 우선순위: `CurrentState`/`NextState` 방향, input group masks, inactive channel, `OutgoingFluxScale` stale 여부, FP16 vs FP32 raw cache format.
