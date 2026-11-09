# 현행 설계 판단

> 2026-10-26 작업 트리 기준의 짧은 이유 목록이다. 실제 동작은 코드·테스트, 계산 정의는 [[02_Architecture/03_System-Map|Architecture]]로 확인한다. 과거 Decision 번호는 개인 볼트와 구버전 보관본의 이력 식별자다.

## State와 보존

| 현행 선택 | 이유와 지켜야 할 조건 |
|---|---|
| State는 texel별 총량, Capacity는 포화 기준 | 입력·이웃 유입의 Capacity 초과량을 버리지 않는다. Transport는 상한 없는 `State / Capacity`를 사용하고 source의 전체 outgoing은 보유량으로 제한한다. 과거 Decision 0004. |
| 기준 면적은 `1/256² world-length²`로 고정 | 해상도가 달라도 Profile 수치의 기준을 바꾸지 않는다. Capacity·입력·Decay를 world texel 면적으로 환산한다. 경계 sampling 오차까지 없어졌다는 뜻은 아니다. 과거 Decision 0009. |
| Contact는 사건 입력 | `InputDelta`는 첫 Solver step에서 한 번 소비한다. Add/Remove는 signed delta이고 Next State는 음수가 되지 않는다. |

## Geometry와 수송

| 현행 선택 | 이유와 지켜야 할 조건 |
|---|---|
| Simulation UV의 texel graph와 UV seam 이웃 연결 | Render UV의 화면 효과와 수송 topology를 구분한다. 같은 실제 edge는 이웃 graph에서 연결하고 열린 경계는 유지한다. |
| Normal Map 기반 Virtual Meso와 Macro/Meso 오목도 | 미세 형상을 수송·Decay에 사용한다. State-driven geometry가 켜지면 적층으로 바뀐 위치·normal·오목도·간선 가중치를 GPU에서 갱신한다. 정적 cache와 동적 instance 형상을 혼동하지 않는다. |
| 포화도 차이와 중력·높이차를 별도 수송 구동력으로 둠 | Geometry 이동에는 source Saturation을 곱하고, `cavityExitResistanceFactor`는 홈에서 밖으로 나가는 flux에만 적용한다. Decay 보호는 별도 계수이며 cavity가 채워지면 줄어든다. |
| Fixed timestep 기본 ON, Auto substepping 기본 OFF | 기본 dt는 1/60초다. Auto를 켠 경우에만 Transport 상한으로 세분화하고 frame당 최대 8회 후 남은 시간은 이월한다. 과거 Decision 0013. |

## GPU와 실험 경로

| 현행 선택 | 이유와 지켜야 할 조건 |
|---|---|
| Scene Profile table과 정적 Geometry를 공유, State는 instance별 | 동일한 입력의 전처리·GPU Geometry를 재사용해도 각 instance의 State A/B와 동적 형상은 독립적이어야 한다. |
| 두-pass gather와 A/B State | Pass 1이 Decay·방향 flux 합·source alpha를 계산하고 barrier 후 Pass 2가 incoming과 입력을 합친다. 받은 양은 같은 step에서 다시 보내지 않는다. |
| Raw Flux cache·FP16·sparse 경로는 선택형 | 현재 기본 Raw Flux cache와 FP16 Raw Flux는 ON, FP16 Weights는 OFF다. `compact_storage`는 buffer 크기를 바꿀 수 있다. 실측 전에는 빠른 경로를 단정하지 않는다. 과거 Decision 0025의 제거 기록은 현행 상태가 아니다. |
| 비교 씬 시작 시 Sparse Solver ON | `ReactionTransitionComparison.Scene`의 이전 Ice·Lava 구성은 128 해상도의 국소 접촉 3판이므로 편집 화면에서 Sparse Solver가 유리했다. [[05_Experiments/02_Performance-Measurements#PERF-045 — 비교 씬 편집 화면의 Sparse Solver 기본값|PERF-045]]에서 같은 State로 GPU 프레임 19.216→8.573ms를 확인했다. 현재 Fuel·Burn 구성은 별도 성능 실측 전이며, 다른 Scene의 전역 기본값 OFF는 유지한다. |

## 확정된 개발 방향 — 상태 전이와 상호작용

2026-10-26 사용자 지시에 따라 프로젝트의 개발 범위를 **State 전이와 State 간 상호작용**으로 확장한다. `Ice → Water`처럼 보유량을 소비해 다른 State를 만드는 전이와, Heat가 Burn 생성을 촉진하거나 Water가 이를 억제하는 상호작용을 함께 다룬다. 조건을 제공하는 State가 반드시 소비되는 것은 아니다.

State는 texel별 총량이며 **저장 상한을 두지 않는 기존 계약을 유지한다**. `stateCapacity`는 포화 기준이다. 여러 반응의 총 소비량은 보유량 이하로 제한해야 하지만, 생성량에 저장 공간을 이유로 한 상한은 도입하지 않는다. 제안의 `Burn ∈ [0,1]`은 예시일 뿐 채택한 State 계약이 아니다.

현재 version 3/4 `.SRProfile`의 `transitions` 정의는 source를 소비하지 않는 이전 선언 형식의 설명이며 새 기능의 범위를 제한하지 않는다. 소비 후 생성하는 전이는 Reaction의 하위 유형으로 version 5 `reactions`의 `consumes`·`produces`로 표현한다. 별도 GPU Reaction Pass, 공통 공급량 제한과 면적 환산 조건을 구현했다. 상세 계약은 [[02_Architecture/01_Contracts/06_Reactions|Local Reactions]]를 따른다. Rule별 sample 선택은 두지 않고 기준 면적당 양으로 통일한다.

## 아직 확정되지 않은 범위

- 게임 Physics adapter와 Surface-bound 공개 Contact wrapper는 없다.
- 성능 A/B 경로의 채택은 같은 조건의 기준·변경 후 측정과 정확성 확인을 거쳐 결정한다. 근거 없는 속도·메모리 절감률을 현행 판단으로 쓰지 않는다.
