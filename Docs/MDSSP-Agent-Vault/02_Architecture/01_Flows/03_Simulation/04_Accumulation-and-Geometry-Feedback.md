# Accumulation / Dynamic Geometry Feedback

Solver State 저장량과 실제 형상 높이는 동일한 변수가 아니다. State로부터 파생한 높이·법선·TransferWeight가 다음 step으로 전달되는 실행 순서만 다룬다.

**연결 문서**
- [[02_Architecture/03_Source/Src_Shaders/01_Simulation/03_Dynamic-Geometry|Dynamic Shaders]]
- [[02_Architecture/01_Flows/04_Rendering/00_Render-Overview|Rendering]]

## State에서 다음 Geometry까지

1. `SurfaceAccumulationHeight.comp`가 Next State의 적층 기여를 공통 높이로 합친다.
2. State-driven geometry가 켜져 있으면 `SurfaceDynamicGeometryUpdate.comp`가 position·normal·concavity를, `SurfaceDynamicWeightsUpdate.comp`가 다음 step의 directional TransferWeight를 준비한다.
3. 꺼져 있으면 Solver용 동적 geometry 갱신을 건너뛰되 마지막 substep에서는 표시용 Height를 만들 수 있다.
4. 후속 compute/read 및 renderer가 읽기 전에 필요한 barrier를 적용하고 A/B current 방향을 전환한다.

`LitHeightDisplayScale`과 화면용 smoothing은 **렌더링 전용**이다. Simulator의 geometry에 다시 쓰지 않는다. 계산식은 [[02_Architecture/02_Contracts/02_Geometry|Geometry 계약]].

## 현재 상태 full rebuild vs 새 상태 sparse derive

실제 `RecordSteps`의 `bNeedsPreSolverGeometryBuild`는 state-driven geometry ON이면서 force-full invalidation 때만 참이다. 이때 Current buffer를 대상으로 Height → DynamicGeometry → DynamicWeights를 full dispatch한다. 반면 Pass2 이후에는 Next buffer를 대상으로 height(조건부 sparse/full)를 갱신하고 geometry ON일 때에만 dynamic geometry·weights를 indirect dispatch한다. Geometry OFF의 마지막 substep에서는 renderer용 height만 준비할 수 있다. 이 분리를 생략하면 화면의 높이와 solver weight 갱신 시점을 잘못 해석할 수 있다.
