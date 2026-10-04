# Decision 0013 — Fixed timestep과 Auto substepping 분리

> **한 줄 요약:** 기본 step은 1/60초로 고정하고, 필요할 때 더 잘게 나누는 기능은 별도 옵션으로 둔다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

> **후속 결정 — 2026-10-02:** 인터랙티브 실행에서는 설정 준비 시간을 누적하지 않는다. 긴 frame에서 무한히 늘어나는 backlog도 제한한다. 아래의 기본 Fixed/Auto 간격 정책은 유지한다.

## 쉽게 읽기

기본 1/60초 timestep과 자동 세분화를 서로 다른 옵션으로 둔다. Auto가 켜졌을 때만 안전 간격으로 step을 나누고, 과도한 밀린 시간은 제한해 UI에 표시한다.

## Context — 왜 필요했나

초기 누적 시간 구현에서는 Fixed ON도 Profile·면적·Weight로 선택한 `MaximumStep`을 사용했다. 같은 설정에서는 간격이 일정했지만 Profile 계수 변경으로 dt가 달라질 수 있어, 고정 간격과 자동 간격 조정의 역할이 섞여 있었다.

게임 엔진의 Substepping은 시간 구간을 여러 계산으로 나누는 기능이고, 수치해석의 Adaptive time stepping은 조건에 따른 dt 조정이다. 공식 문서와 용어의 차이는 [[02_Research/0004_Substepping-and-Adaptive-Time-Stepping|조사 문서]]에 기록한다.

초기 정책은 frame당 8회 한도를 넘긴 경과 시간을 모두 보존했다. 설정 변경·GPU 준비가 긴 frame도 여기에 포함돼 불필요한 따라잡기 작업이 누적될 수 있었다.

## Decision — 무엇을 정했나

- UI에 `Fixed timestep`, `Auto substepping`을 독립 옵션으로 제공한다. 기본값은 각각 **ON**, **OFF**다.
- Fixed ON은 실제 경과 시간×배속에서 1/60초 구간을 소비한다. 계수 변경은 기본 구간을 바꾸지 않는다.
- Auto ON에서만 기존 CPU Transport 상한을 계산·적용한다. Auto OFF일 때 불필요한 상한 계산을 실행하지 않는다.

| Fixed | Auto | 시간 소비 |
|---|---|---|
| ON | OFF | 1/60초씩 계산, 잔여 시간 대기 |
| ON | ON | 1/60초가 모이면 상한 이하로 나누고 마지막 짧은 step으로 해당 구간 완료 |
| OFF | OFF | 누적된 경과 시간 전체를 한 Solver step으로 소비 |
| OFF | ON | 상한 이하로 나누고 마지막 잔여 시간도 소비 |

- frame당 최대 8 **Solver 실행**을 유지한다. 일반 frame에서는 미완료 고정 구간의 잔여 시간을 다음 frame에서 재개한다. 별도 시간 budget을 중복 누적하지 않는다.
- 인터랙티브 실행의 pending은 배속 1×에서 최대 고정 구간 4개(약 66.67 ms)로 제한한다. 배속이 4×를 넘으면 최소한 해당 배속의 60 Hz frame에 필요한 tick 수만큼 허용한다. 초과 시간은 폐기하고 UI의 `Time skipped`에 표시한다. 15/30/60/120 FPS의 정상 frame 및 60 FPS의 4× 배속은 이 제한으로 시간을 잃지 않는다.
- UI에서 Fixed/Auto·Solver 항목·Accumulation Geometry Update·RawFlux cache·Profile override·해상도·overlay tile을 변경하거나 Scene 파일 대화상자를 열면 해당 frame의 시간을 누적하지 않고 기존 pending을 비운다. 설정 변경으로 무효화된 TransferWeight cache는 그 frame에 준비하고, 해당 frame의 GPU 작업 완료를 기다린 뒤 시계 기준을 다시 잡는다. State와 누적 시뮬레이션 진행 시간은 설정 변경만으로 초기화하지 않는다.
- 초기안은 옵션 변경 중 진행 중인 고정 구간을 보존했으며, 현재 UI 설정 변경 경로는 그 잔여 시간도 비운다. 설정 변경 없이 Auto OFF로 전환하는 저수준 clock 호출은 기존 구간을 마무리한다.
- Pause는 시간을 누적·소비하지 않는다. 수동 Step은 Solver 한 번이며 Auto OFF는 1/60초, ON은 현재 Transport 상한이다. 기존 backlog와 미완료 고정 구간은 수동 실행으로 소비하지 않는다.
- Reset·Scene·해상도 변경은 clock와 미완료 고정 구간을 초기화한다. 입력 한 번 소비, 갱신된 State로 후속 반복, alpha, timestamp 합산 계약은 유지한다.
- 현재 시간 상한은 원시 유출 비율에 대한 보수적 조건이며, 오차 추정기 또는 엄밀한 CFL 증명으로 표현하지 않는다.

## Alternatives Considered — 다른 방법

- Fixed ON에서 자동 상한을 계속 적용하기: 고정 구간과 자동 조정의 구분이 드러나지 않는다.
- 자동 세분화를 제거하기: 빠른 흐름과 높은 해상도에서 alpha에 따른 속도 제한을 줄일 선택지가 사라진다.
- frame 반복 한도에서 남은 시간을 무조건 폐기하기: 정상적인 저 FPS·고배속 frame의 시뮬레이션 시간도 잃는다. 현재 정책은 허용된 pending을 이월하고 초과분만 폐기한다.

## Consequences — 결정의 영향

기본 실행은 계수와 무관한 dt=1/60초다. Auto OFF에서는 alpha가 원시 유출을 제한할 수 있어 흐름이 의도보다 느려질 수 있다. Auto ON은 재전달 기회를 늘리지만 GPU 비용과 backlog가 커질 수 있다. Fixed OFF·Auto OFF에서는 긴 frame dt도 나누지 않으므로 고정 모드와 동일 결과를 가정하지 않는다. 지속적인 과부하나 긴 일시정지에서는 시뮬레이션 시간이 실제 시간보다 느리게 진행될 수 있으며, 폐기량을 UI에서 확인한다.

CPU 회귀는 네 조합과 시간 보존·한도·전환을 검사한다. Scene과 GPU의 기존 Transport 검사는 유지하며 전체 분포·성능 비교는 별도 실험 대상이다.

## Related — 관련 문서

- [[05_Decisions/0011_Accumulated-Simulation-Timestep|Decision 0011 — 초기 누적 시간 구현]]
- [[05_Decisions/0012_Geometry-Rate-Recalibration|Decision 0012]]
- [[02_Research/0004_Substepping-and-Adaptive-Time-Stepping|공식 문서·용어 조사]]
- [[03_Architecture/0006_Surface-State-Update|State Update]]
- [[03_Architecture/0009_UI-Interface|UI]]
- 실험 계획
