# Compute Shader — Solver Pass 1 / 2

수송 두 entry, 선택적 Reaction entry와 아직 실행하지 않는 Input placeholder를 분리 표시한다. 각 entry의 Input/Output과 barrier 소비자를 먼저 확인한다.

**연결 문서**
- [[02_Architecture/00_Flows/02_Simulation/02_Flux-Pass1|Pass 1]]
- [[02_Architecture/00_Flows/02_Simulation/03_Gather-Pass2|Pass 2]]

## Entry points

| 파일 | 주요 코드 | 입력→출력과 다음 소비자 |
|---|---|---|
| [SurfaceSolverPass1.comp](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass1.comp) | `evaluateDirectionRawFlux`, `main` | Current State/Profile/Geometry→Decay 결과·RawOutgoing·source별 outgoing scale/cache; Pass 2가 읽음 |
| [SurfaceSolverPass2.comp](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp) | `accumulationContributionChanged`, `main` | Raw outgoing+scale를 neighbor gather하고 InputDelta를 한 번 소비→기본 State. Scene에 활성 Reaction 규칙이 없으면 Next State와 다음 activity·geometry dirty 상태를 기록 |
| [SurfaceReaction.comp](../../../../../../Shaders/Simulation/Solver/SurfaceReaction.comp) | `reactionRequest`, `termCoefficient`, `main` | 기본 Input/Transport/Decay snapshot → 유입 전환/continuous 반응 → 최종 Next State 및 sparse/Height schedule |
| [SurfaceInput.comp](../../../../../../Shaders/Simulation/Input/SurfaceInput.comp) | placeholder only | 아직 SPIR-V를 생성하지 않는 placeholder. 실제 Contact는 `SurfaceContactApplication.cpp`에서 InputDelta로 모음 |

## 실행 의존성

Pass 1이 쓰는 RawOutgoing, source scale 또는 Flux cache는 Pass 2에서 읽는다. C++ [`SurfaceStateSolver.cpp`](../../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)의 barrier·dispatch 기록을 같이 확인한다. shader binding/field의 CPU mirror는 [GPU ABI 문서](../../00_CPP/01_GPU/00_Index.md)와 [GPU Layout contract](../../../01_Contracts/05_GPU-Layout.md)가 기준이다.


## 상태 확인

`SurfaceInput.comp`는 placeholder이며 **실제 Contact dispatch가 아니다**. CPU `SurfaceContactApplication.cpp`가 `InputDelta`를 만든다.

## 실제 shader 내부에서 확인한 경계

- [Pass1](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass1.comp): 각 channel에서 지원하지 않는 texel 또는 inactive channel을 구분하고 `RawOutgoing`/`OutgoingFluxScale`를 만든다. `AvailableState`보다 raw outgoing 총량이 크면 `min(1, AvailableState / RawOutgoing)`로 출발 texel 유출을 제한한다.
- [Pass2](../../../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp): 유효 이웃의 `CurrentState > 0` 및 출발 scale을 확인하고, optional raw cache를 읽거나 flux를 재계산한다. 이 검사는 sparse 실행에서 stale scratch가 섞이는 문제를 완화한다. `InputDelta.Values[...] = 0.0`로 실제 소모를 수행한다.
- Scene에 활성 Reaction 규칙이 없으면 Pass2의 `accumulationContributionChanged(...)`가, 하나라도 있으면 Reaction Pass의 같은 검사가 최종 State를 기준으로 sparse accumulation을 예약한다. `min(State, Capacity)`의 형상 기여가 변하지 않으면 State가 변해도 재계산하지 않을 수 있다. Capacity는 **저장량 upper clamp가 아닌 포화도 기준량**이다.
- [SurfaceInput.comp](../../../../../../Shaders/Simulation/Input/SurfaceInput.comp)의 코드만으로 실행 pipeline이 있다고 단정하지 않는다. 이번 C++ source의 contact 주입은 CPU 쪽 `ApplyPendingContacts`이며 `SurfaceStateSolver.cpp`의 파이프라인 생성 목록에는 이 entry가 없다.

## 상위 실행 단계와 helper

- Pass1은 기존 channel/profile 준비 후 `EvaluateRawOutgoing`에서 FP16 packing 또는 float cache/방향 합산을 수행한다. 양자화된 edge 합을 사용하는 기존 보존 규칙을 유지한다.
- Pass2 main은 shared mask 초기화 → barrier → `AdvanceTexelState` → sparse 예약/reduction → barrier → group mask 게시를 보여 준다. `GatherIncoming`은 stale source 검사와 cache/recompute 경로를 담당한다.
- Reaction main은 동일한 workgroup 참여/마스크 게시 구조를 유지하고 `ReactAndPublishTexel` → `IntegrateReactionRules`로 texel 처리와 local reaction 적분을 구분한다. 네 번의 buffer memory barrier와 bounded substep/진단 정책은 그대로다.
- helper 선언 → main → 세부 구현 순서이며 CPU dispatch, descriptor, push constant, pass 사이 barrier는 변경하지 않았다.
