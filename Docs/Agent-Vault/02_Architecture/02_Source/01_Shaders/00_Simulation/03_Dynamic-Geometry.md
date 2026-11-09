# Compute Shader — Accumulation / Dynamic Geometry

State의 높이 기여를 모아 동적 위치·Normal·방향별 Weight를 업데이트하는 compute entry들이다.

**연결 문서**
- [[02_Architecture/00_Flows/02_Simulation/04_Accumulation-and-Geometry-Feedback|Geometry Feedback]]
- [[02_Architecture/01_Contracts/03_Geometry|Geometry Contract]]

## Geometry entry points

| 파일 | 주요 코드 | 입력→출력과 다음 소비자 |
|---|---|---|
| [SurfaceAccumulationHeight.comp](../../../../../../Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp) | `main`, dirty height detection·capacity tile scheduling | Current State+Profile→Accumulation Height 및 height dirty worklist; dynamic geometry pass와 Solver가 읽음 |
| [SurfaceDynamicGeometryUpdate.comp](../../../../../../Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp) | `main`, scheduled texel lookup | height와 static geometry→instance 위치·normal·concavity 업데이트; 이후 transfer weight/Solver가 읽음 |
| [SurfaceDynamicWeightsUpdate.comp](../../../../../../Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp) | local/world position·normal, edge weight calculation | 동적 형상과 인접 texel→directional TransferWeight buffer; Solver Pass 1이 참조 |

## 순서

Accumulation Height → Dynamic Geometry → Dynamic TransferWeight. Current invalidation 때 선행 재구축으로, Pass 2와 선택적 Reaction Pass 이후에는 최종 Next State 기준 파생값 생성으로 실행할 수 있다. 각 dispatch의 실시간 조건과 barrier는 C++ 기록을 기준으로 확인한다.

## Current prebuild와 Next 후행 경로 구분

실제 dispatch는 [FSurfaceStateSolver::RecordSteps](../../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)에서 준비한다. profile/transform/reset에 의해 `ForceFullGeometry`가 요청되고 state-driven geometry가 ON이면 **현재 Current State**를 기준으로 세 Compute shader를 full dispatch하고 barrier를 둔다. Pass 1/2와 선택적 Reaction Pass 이후에는 **최종 Next State** 기준 Accumulation Height를 만들고 `bUpdateGeometry`가 참이면 새 DynamicGeometry/Weights를 `vkCmdDispatchIndirect`한다. Geometry 업데이트가 OFF여도 마지막 substep에서 렌더용 height만 계산할 수 있다.

`SurfaceAccumulationHeight.comp`의 sparse dirty scheduling은 Scene에 활성 Reaction 규칙이 없으면 Pass 2, 하나라도 있으면 Reaction Pass의 `accumulationContributionChanged`와 연결된다. 따라서 State 변경이 전부 geometry recalculation을 의미하지 않는다. 읽기·쓰기 descriptor의 AB/BA 전환은 `DescriptorResources.cpp`가 결정한다.
