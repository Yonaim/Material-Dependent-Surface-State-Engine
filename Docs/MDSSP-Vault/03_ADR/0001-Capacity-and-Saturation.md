# ADR 0001 — Capacity와 Saturation

- 상태: **Accepted**
- 날짜: 2026-09-11 이후 최신 설계에 반영

## Context

기존 `State ∈ [0,1] + Overflow` 구조는 Mud·Snow처럼 큰 누적량을 표현할 때 Capacity의 의미와 포화도를 분리하기 어려웠다.

## Decision

- `stateCapacity`를 State별 SRProfile 독립 파라미터로 둔다.
- 기본값은 `1.0`, 적층 상태에는 `> 1`을 허용한다.
- `0 ≤ State ≤ stateCapacity`.
- `Saturation = State / stateCapacity`는 런타임 파생값이다.
- `Overflow`를 초과 상태량으로 사용하지 않는다.
- Solver 임시 데이터는 `TempState`로 둔다.
- Capacity를 초과한 양은 별도로 저장하지 않는다.

## Consequences

Transport의 상태 차이는 raw State가 아니라 Saturation을 사용할 수 있다. Accumulation은 `(State + Overflow)`가 아니라 `State`에서 계산한다. [[02_Architecture/0003_Surface-State|표면 상태]], [[02_Architecture/0008_Accumulation|적층]].

## Alternatives Considered

### 1. `State ∈ [0,1]`과 별도 `Overflow` 유지

정규화된 State 범위를 유지할 수 있지만, Overflow와 State가 같은 양의 서로 다른 표현이 되어 전파·적층·포화 계산에서 두 값을 계속 함께 다뤄야 한다. 용량과 포화도를 분리하는 결정을 선택했다.

### 2. Capacity 없이 State에 임의의 상한만 사용

State 값 범위가 재질이나 상태마다 달라질 때 상한의 의미가 명확하지 않고, Saturation 비교에 별도 기준이 필요하다. SRProfile에 상태별 `stateCapacity`를 두는 안을 선택했다.

### 3. Saturation을 영구 저장

State 변경 때마다 함께 갱신해야 하는 중복 파생 데이터가 된다. 저장량을 State 하나로 유지하고 필요한 시점에 `State / stateCapacity`로 계산한다.
