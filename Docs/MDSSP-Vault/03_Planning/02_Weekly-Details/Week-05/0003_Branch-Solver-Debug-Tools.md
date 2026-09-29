# Branch 3 — Solver Debug Tools

> **한 줄 요약:** Solver의 State·Geometry·Flux 중간값과 실행 상태를 확인하는 Debug UI를 추가한다.

상태: **구현 완료 · Branch 6 통합 검증 대기**

브랜치: `feat/solver-debug-tools`  
선행 조건: Branch 2.3 Virtual Meso Geometry 구현을 포함한 현재 `main`
통합 범위: 기존 Branch 3 `OutgoingFluxScale Debug View`, Branch 4 `Solver Debug Controls`, Branch 5 `Solver Debug Statistics`  
관련 설계: [[04_Architecture/0006_Surface-State-Update|Surface State Update]], [[04_Architecture/0009_Rendering|Rendering]], [[04_Architecture/0008_Surface-GPU-Data-Layout|Surface GPU Data Layout]], [[04_Architecture/0010_UI-Interface|UI Interface]], [[../../../06_Development/Notes/Surface-State-GPU-Resource|Surface State GPU Resource]]

## 실행 시간 계약 갱신 — 2026-09-29

실제 경과 시간×배속을 누적한다. 기본 Fixed ON·Auto OFF는 1/60초씩 계산하며 Auto ON에서만 세분화한다. frame당 최대 8 Solver 실행이며 미완료 구간과 잔여 시간은 이월한다. Pause 중 시간은 누적하지 않으며 Step은 Auto OFF에서 1/60초, ON에서 Transport 상한으로 한 번이다. Reset은 State·입력뿐 아니라 진행·대기·미완료 구간도 초기화한다. GPU timing은 frame의 모든 반복 합이다. [[05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]]

## 목표

Solver 동작을 한 곳에서 제어하고 관찰할 수 있도록 `OutgoingFluxScale` 뷰, pause/step/reset, 실행 통계를 한 브랜치에서 구현한다. 이 기능들은 같은 Solver 상태와 GPU 리소스 수명에 의존하므로 순차 브랜치로 나누지 않고 한 번에 통합한다.

화면 렌더링과 접촉 입력 UI는 Solver가 멈춰 있어도 계속 동작한다. 일시 정지 중 들어온 접촉 입력은 보존하고, 재개하거나 Step을 실행할 때 소비한다. 통계는 이미 CPU/GPU에 있는 metadata와 비동기 timestamp query만 사용하며 전체 State readback을 추가하지 않는다.

## 구현 결과

- `OutgoingFluxScale` 뷰가 선택한 State channel의 제한 비율을 렌더링하고 범례를 제공한다.
- Pause는 Solver dispatch와 A/B 교환을 멈추며, Step은 update 한 번을 실행한다. Reset은 State와 대기 중인 입력을 초기화한다.
- Solver UI에 texel 수, valid texel 수와 비율, 다음 입력 buffer(A/B), GPU Solver 시간을 표시한다.
- GPU timestamp로 Render, Solver 전체, Pass 1, Pass 2 시간을 수집하고 UI에서 1초 평균으로 보여준다. timestamp를 쓸 수 없는 경우 GPU 측정값을 임의로 대체하지 않는다.
- Solver 디버그 항을 그룹별로 켜고 끌 수 있으며, UI 배치와 뷰 설명도 정리했다.

기능 구현은 끝났다. 작은 GPU fixture, Vulkan validation, 지원 GPU에서의 동작 및 데모 화면 확인은 Week 5 Branch 6에서 통합 검증하고 결과를 기록한다.

## 기능 계약

### 1. OutgoingFluxScale 뷰

- Surface Debug view 목록에 `Outgoing Flux Scale`을 추가하고 현재 선택한 Registry State channel의 값을 보여준다.
- Graphics fragment shader가 instance별 OutgoingFluxScale buffer를 읽도록 기존 graphics descriptor set의 binding 10을 사용한다. Layout은 fragment stage visibility를 포함한다.
- 값 범위 `[0, 1]`에 연속 색상 ramp를 적용한다. `0`은 outgoing을 강하게 제한하고 `1`은 제한이 없음을 뜻한다.
- 선택한 State가 Profile에서 지원되지 않거나 mapping이 invalid인 texel은 기존 Surface Debug 진단 색상 계약과 일치시킨다.
- 선택 채널 UI와 범례를 같은 `Outgoing Flux Scale` view context 안에 표시한다. State Heatmap 색상과 혼동되지 않도록 별도 ramp를 쓴다.

### 2. Solver 제어

| Control | 동작 |
|---|---|
| Pause | Solver dispatch 및 A/B 역할 교환을 멈춘다. 렌더링과 UI 갱신은 계속한다. 접촉 입력은 다음 Solver step까지 보존한다. |
| Step | 일시 정지 상태에서 Auto OFF는 1/60초, ON은 현재 Transport 상한으로 모든 simulated instance를 정확히 한 번 갱신하고, 각 A/B 방향을 한 번 교환한 뒤 계속 pause 상태를 유지한다. |
| Reset State | 명시적으로 요청된 시점에 모든 simulated instance의 State A/B와 InputDelta를 0으로 초기화하고 현재 방향을 A로 되돌린다. 지원 channel의 OutgoingFluxScale은 1, invalid/unsupported 위치는 0으로 둔다. 이미 누적된 CPU Contact 입력도 비운다. |

Reset은 frame마다 실행하지 않는다. 요청을 처리하기 전에 graphics queue의 작업 완료를 기다린 뒤 host-visible buffer를 0으로 초기화한다. Pause 중 새로 들어온 입력은 보존하지만 Reset 직전에 대기 중인 입력은 reset 계약대로 비운다.

### 3. Solver 통계

| 항목 | 표시 정의 |
|---|---|
| Texel count | Scene에서 Solver가 관리하는 instance별 texel 수 합계. Shared Geometry도 State는 instance별이므로 instance마다 센다. |
| Valid texel / ratio | Shared Geometry의 texel에서 `IsValid()`인 개수와 `valid / total` 비율. |
| Current buffer | 다음 Solver step이 읽을 State buffer가 A인지 B인지 표시한다. 모든 managed instance가 같은 방향인지, 다르면 instance별로 표현할지 구현과 UI에서 명확하게 한다. |
| Recent GPU solver time | Pass 1 직전과 Pass 2 완료 뒤 기록한 timestamp 차이 중 가장 최근 완료된 Solver step. query가 끝나지 않았으면 기다리지 않고 이전 완료 값이나 초기 `N/A`를 유지한다. |

GPU timestamp를 지원하지 않는 queue/device에서는 CPU 제출 시간을 대신 쓰지 않고 `N/A (unsupported)`로 표시한다. Query pool 재사용과 해제는 frame fence 완료 시점에 맞춘다.

## 구현 순서

1. `OutgoingFluxScale` graphics descriptor 및 shader view를 연결하고 selected channel 범례를 추가한다.
2. Solver system에 정지, 단일 step 요청, State reset 요청 경로를 추가한다. Pause 중 InputDelta를 소비하거나 버리지 않는지 확인한다.
3. texel/valid count와 A/B 방향을 UI가 조회할 수 있게 노출한다.
4. 기존 timestamp query를 실제 Solver dispatch가 있는 frame에만 기록하고 pause/step 상태와 연결한다.
5. Simulation Debug의 Solver 섹션에서 view, control, 통계를 함께 확인할 수 있도록 UI를 구성한다.
6. GPU fixture, build, 사용 가능한 GPU validation, Demo Scene 수동 확인 결과를 기록한다.

## 검증

| 검증 | 확인 사항 |
|---|---|
| Scale view | 작은 GPU fixture의 buffer 값과 shader가 같은 texel/channel index를 읽고, `0` 제한·`1` 제한 없음 의미가 범례와 일치한다. |
| Channel 선택 | 둘 이상의 Registry channel 중 선택한 channel만 view에 반영된다. |
| Pause | 여러 frame 동안 State와 A/B 방향이 바뀌지 않지만 렌더링은 계속된다. Pause 중 제출된 접촉 입력은 누적되어 있다. |
| Step | pause 중 Step 한 번이 정확히 한 Solver update와 한 번의 A/B 교환을 수행하고 다시 멈춘다. |
| Reset | 모든 State/InputDelta가 0, Current가 A, 지원 channel scale이 1, invalid/unsupported scale이 0이며 CPU 대기 입력도 비어 있다. |
| Instance isolation | 공유 Geometry를 쓰는 두 instance를 함께 reset해도 State가 섞이지 않는다. |
| Texel 통계 | 공유 Geometry instance별 계산, all-valid/all-invalid/mixed mapping에서 count와 ratio가 정확하다. |
| Current buffer | 표시된 A/B 방향이 실제 다음 dispatch descriptor 선택과 일치한다. |
| GPU timing | 지원 장치에서 유한한 0 이상 값을 표시하고, 미완료 query를 기다리느라 frame을 stall하지 않는다. 미지원 장치는 `N/A`를 표시한다. |
| Validation | graphics descriptor stage/binding, reset buffer 동기화, query pool 수명 관련 Vulkan validation 오류가 없다. |

작은 합성 GPU fixture로 값과 불변 조건을 먼저 검증하고, Demo Scene은 UI 통합과 시각 확인에 사용한다. 화면만 보고 수식 정확성을 판정하지 않는다.

## 완료 조건

- OutgoingFluxScale 값과 의미를 화면에서 확인할 수 있다.
- Pause, 한 번 Step, Reset 동작이 계약대로 재현된다.
- 통계가 실제 Scene instance/resource 상태와 일치한다.
- build와 적용 가능한 테스트가 통과하고, GPU 환경이 제한될 때는 skip 사유가 기록된다.

## 제외 범위

Raw flux readback, 전체 GPU profiler, playback/recording, Solver 수식 변경, staging buffer 도입, 상세 per-texel CPU 통계.
