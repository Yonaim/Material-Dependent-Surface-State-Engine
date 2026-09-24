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

Transport의 상태 차이는 raw State가 아니라 Saturation을 사용할 수 있다. Accumulation은 `(State + Overflow)`가 아니라 `State`에서 계산한다. [[02_Architecture/Surface-State|표면 상태]], [[02_Architecture/Accumulation|적층]].
