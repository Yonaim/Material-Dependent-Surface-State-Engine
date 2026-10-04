# Decision 0011 — 실제 경과 시간을 누적하는 Solver 반복

> **한 줄 요약:** 실제 경과 시간만큼 Solver를 반복하고, 한 번에 처리하지 못한 시간은 다음 frame으로 넘긴다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

> **후속 결정 — 2026-09-29:** 이하의 `MaximumStep` 기반 Fixed 정책은 초기 누적 시간 구현 기록이다. 현재 정책은 [[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]로 분리했다. Fixed 기본 ON은 1/60초 구간을 사용하고, 기본 OFF인 Auto substepping에서만 아래 Transport 상한을 적용한다.

> **후속 결정 — 2026-10-02:** 이하의 무제한 backlog 보존은 초기안의 기록이다. 현재 인터랙티브 실행은 설정 준비 시간을 제외하고 pending을 제한한다. [[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]를 따른다.

## 쉽게 읽기

실제 경과 시간을 누적해 Solver를 반복하고, 한 번에 처리하지 못한 시간은 넘기는 초기 정책을 기록한다. 현재 Fixed/Auto 및 backlog 정책은 Decision 0013의 후속 결정이 기준이다.

## Context — 왜 필요했나

기존 Fixed ON은 렌더 frame마다 1/60초×배속의 step 한 번을 실행했다. 15 FPS·배속 1에서는 실제 1초에 시뮬레이션 0.25초만 진행했다. 실제 frame dt를 한 번 곱하면 진행 시간은 맞지만 중간 유입을 같은 frame에서 다시 전달할 기회가 부족하다.

## Decision — 무엇을 정했나

```text
실행 중: PendingTime += 실제 frame 경과 시간 × TimeScale
Fixed ON: PendingTime에서 MaximumStep 단위로 소비, 잔여 시간 유지
Fixed OFF: 같은 MaximumStep으로 나누되 마지막 작은 잔여 시간까지 소비
매 반복: Pass 1 → barrier → Pass 2 → barrier → A/B 교환
한 frame 최대 8 step, 남은 PendingTime은 다음 frame으로 이월
```

- Fixed 기본 ON을 유지한다. `MaximumStep`은 최대 1/60초이며 현재 형상·Profile·면적·Weight에 따라 더 작아질 수 있다. 배속은 step dt가 아니라 누적 시간에 곱한다.
- 반복마다 갱신된 Current State에서 RawFlux·alpha를 다시 계산한다. 새로 들어온 이웃 값과 첫 step의 입력이 같은 frame의 후속 step에서 이동할 수 있다.
- Discrete Contact 입력은 첫 실행 step의 Pass 2에서 한 번 소비한다. 연속 입력률 모델은 추가하지 않는다.
- Pause 중 실제 시간을 누적하지 않는다. Step은 Paused 상태에서 MaximumStep 한 번이며 기존 backlog는 소비하지 않는다.
- Reset, Scene 변경, 해상도 변경은 State·입력과 clock의 누적·진행 시간을 초기화한다.
- GPU Pass 1·2 timestamp는 해당 frame의 모든 instance·모든 step을 합산한다. UI에 step 수, step 한도, 진행 시간, backlog를 표시한다.

### 시간 간격의 보수적 기준

출발 포화도를 sigma라 하면 SaturationDrive≤sigma이고 DirectionDrive≤1이다. source i의 한 step 원시 유출 비율에 대한 상한을 사용한다.

```text
K_i = SatRate_i × Σ_j Weight_i→j
    + GeoRate_i × Σ_j (Weight_i→j × abs(world height difference_i→j))
MaximumStep = min(1/60, 모든 지원 source/channel의 0.9 × Capacity_i / K_i)
```

K=0인 항목은 제한하지 않는다. debug OFF 항은 제외하고 Geometry 방향은 최악의 경우 1로 둔다. State 크기와 무관한 보수적 상한이므로 CPU readback 없이 계산한다. Profile override·term·instance 선형 transform 변경에 따라 다시 계산하며, 같은 조건에서는 재사용한다. 기존 alpha는 계속 적용한다.

이 기준은 **감쇠를 제외한 원시 Transport가 보유량의 90%를 넘지 않도록 하는 상한**이다. 감쇠 후 가용량 감소는 alpha가 처리한다. 일반 graph의 정확도·수렴·모든 물리 안정성을 증명한 CFL 조건으로 표현하지 않는다. dt-halving 검토는 계속 필요하다.

## Alternatives Considered — 다른 방법

- frame당 고정 한 step: 저 FPS에서 시뮬레이션 시간이 느려진다.
- 긴 frame dt 한 번: 재전달 횟수가 부족하고 alpha에 의한 속도 제한이 커질 수 있다.
- 반복 수 제한 시 초과 시간 폐기: 실제 누적 시간이 사라지므로 backlog 보존을 채택한다.

## Consequences — 결정의 영향

GPU가 필요한 step을 처리할 수 있으면 FPS와 무관하게 같은 시뮬레이션 시간을 계산한다. 한 frame의 일을 8회로 제한해도 지속 과부하에서는 backlog가 늘고 화면의 시뮬레이션은 실제 시간보다 늦을 수 있다. 이 구현은 GPU 처리량 부족 자체를 없애지 않는다. 필요한 간격이 작은 High 해상도에서는 비용을 별도로 측정한다.

## Related — 관련 문서

- [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009]]
- [[05_Decisions/0010_Geometry-Transport-Mobility|Decision 0010]]
- [[03_Architecture/0009_UI-Interface|UI]]
- [[03_Architecture/0006_Surface-State-Update|최적화와 비용]]
- 비교 실험
