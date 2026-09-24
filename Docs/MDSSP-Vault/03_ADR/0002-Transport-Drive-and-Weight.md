# ADR 0002 — Transport Drive와 TransferWeight 분리

- 상태: **Accepted**

## Context

기존 TransferWeight 안에 Height / Direction을 모두 넣으면 Geometry 자체가 이동을 발생시키는 효과와, 이웃 관계가 전달을 통과시키는 효과가 섞인다.

## Decision

Transport를 다음 세 역할로 분리한다.

1. `SaturationDrive`: Saturation 차이에 의한 이동 구동력.
2. `GeometryDrive = HeightDrive × DirectionDrive`: 높이·중력 방향에 의한 이동 구동력.
3. `TransferWeight`: 실제 이웃 관계의 통과 정도.

```text
TransferWeight
= DistanceWeight
× NormalWeight
× CurvatureWeight
× ProfileBoundaryWeight
```

`CurvatureWeight`는 이동 방향을 만드는 것이 아니라 홈·요철에 의해 이동이 억제되는 정도를 표현한다. `ProfileBoundaryWeight`는 SRProfile이 달라지는 경계의 전달 정도를 표현한다.

## Consequences

Height / Direction과 Curvature의 역할이 겹치지 않는다. UV Seam은 Profile Boundary가 아니므로 가중치가 아니라 Neighbor 연결 문제로 처리한다. [[02_Architecture/Propagation-Solver|Propagation Solver]].
