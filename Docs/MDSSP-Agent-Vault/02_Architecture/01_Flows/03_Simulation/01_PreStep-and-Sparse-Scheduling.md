# Pre-Step · Sparse Scheduling

Current Geometry가 무효화되었는지 검사하고 필요한 전체 재구축과 sparse metadata reset을 수행하는 경계다.

**연결 문서**
- [[02_Architecture/01_Flows/03_Simulation/00_Step-Overview|Step Overview]]
- [[02_Architecture/03_Source/Src_Shaders/01_Simulation/02_Transport-and-Scheduling|Shader Scheduling]]
- [[02_Architecture/02_Contracts/04_GPU-Layout|GPU Layout]]

## 진입점과 조건

[`TSurfaceStateSystem::RecordStep`](../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp)은 active instance마다 현재 A/B State, texel/channel 수, 모델 행렬, gravity, 입력 활성화 및 solver flags를 `TSurfaceSolverInstanceStep`으로 묶는다. Transform·설정 변경으로 transfer-weight cache가 무효화되면 CPU에서 재계산·업로드한다.

## 수행 순서

1. reset/profile/transform 변화로 현재 geometry가 무효이면 `SurfaceAccumulationHeight` → `SurfaceDynamicGeometryUpdate` → `SurfaceDynamicWeightsUpdate`를 전체 dispatch한다.
2. Sparse 실행이 켜져 있고 active mask 또는 geometry scheduling이 필요하면 `SurfaceSparseScheduleReset`으로 metadata를 정리하고 입력 group을 병합한다.
3. active workgroup과 full-dispatch 중 선택된 실행 경로를 이후 Pass 1/2에 전달한다.

> Sparse/Full은 실행 범위 최적화이며 State 계산 수식이 다르다는 의미가 아니다.

## 실제 sparse reset 조건

`SurfaceStateSolver.cpp`는 `bSparseSolver || bActiveChannelMask || bUpdateGeometry || (bPrepareAccumulationHeight && bSparseHeight) || bHasInputActivation`이면 fused `SurfaceSparseScheduleReset`을 실행한다. `ForceFullGeometry`가 유효한 state-driven 단계는 schedule reset보다 먼저 Current geometry를 full rebuild한다. sparse solver에서 persistent activity seed flag가 있으면 indirect 대신 full dispatch가 선택될 수 있다. 이는 shader의 물리 계산식 변경이 아닌 **GPU 실행 범위** 분기다.
