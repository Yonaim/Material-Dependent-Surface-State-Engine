# Solver Pass 2 — Incoming Gather / Next State

Pass 1 scratch와 이웃 출발 Texel의 보정 비율을 읽고 각 destination Texel의 Next State를 하나의 invocation에서 기록한다.

**연결 문서**
- [[02_Architecture/01_Flows/02_Frame/01_Contact-to-InputDelta|InputDelta]]
- [[02_Architecture/01_Flows/03_Simulation/04_Accumulation-and-Geometry-Feedback|Geometry Feedback]]
- [[02_Architecture/03_Source/Src_Shaders/01_Simulation/01_Solver-Passes|Shader Entry]]

## 실행

`SurfaceSolverPass2.comp`는 이웃 출발점의 Flux와 Outgoing scale을 gather하고, 현재 State에 Incoming을 더하고 Outgoing·Decay를 반영한다. 첫 실행 Step의 `InputDelta`를 **한 번 소비**하여 Next State에 적용한다. 새로 비었던 texel에 주입한 InputDelta는 현재 step의 Pass 1 Flux를 바꾸지 않으므로 다음 Step부터 이웃으로 전파된다.

Reaction이 있는 Scene에서는 여기서 최종 State를 확정하지 않고 모든 채널의 기본 결과와 신규 유입 정보를 ReactionScratch에 준비한다. compute barrier 뒤 `SurfaceReaction.comp`가 최종 State와 다음 활성/Height schedule을 기록한다. [[02_Architecture/02_Contracts/05_Reactions|Local Reactions]] 참고.

## 쓰기와 동기화

- **읽기:** Current State, 이웃 Raw Flux/Outgoing scale, `InputDelta`, Profile.
- **쓰기:** Next State, 필요할 경우 다음 activity metadata.
- **경계:** Pass 1 → Barrier → Pass 2 → Barrier → Height/Geometry, 이후 State A/B current 방향 전환.

실제 Next State 수식과 Capacity 초과량 보존 규칙은 [[02_Architecture/02_Contracts/03_Solver|Solver 계약]]에만 정의한다.

## 입력 소비·Sparse stale cache 가드

Pass2 shader는 source neighbor의 `CurrentState > 0`과 OutgoingFluxScale을 먼저 확인한다. 선택적 flux cache를 사용하면 저장된 방향별 값을, 미사용이면 이웃 source에서 flux를 다시 계산한다. 현재 texel의 NextState 기록 직전에 InputDelta를 읽고 즉시 0으로 만든다. Sparse 실행에서는 next activity mask 및 accumulation-relevant capacity-clamped state 변화가 후속 scheduler에 전달된다. C++는 Pass2 이후 NextState/InputDelta/indirect metadata barrier를 기록한다.
