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

Height / Direction과 Curvature의 역할이 겹치지 않는다. UV Seam은 Profile Boundary가 아니므로 가중치가 아니라 Neighbor 연결 문제로 처리한다. [[02_Architecture/0004_Surface-State-Update|Propagation Solver]].

## Alternatives Considered

### 1. Height와 Direction을 `TransferWeight`에 합치기

단일 가중치 식으로 구현할 수 있지만, 실제로 State를 움직이는 구동력과 이웃 연결을 통과시키는 정도가 섞인다. 두 역할을 따로 조정하고 검증할 수 있도록 Drive와 Weight를 분리했다.

### 2. Saturation 차이만으로 Transport 계산

구현이 단순하고 상태량이 높은 곳에서 낮은 곳으로 흐른다. 다만 높이와 중력 방향에 따른 이동을 표현할 수 없어 별도 `GeometryDrive` 항을 둔다.

### 3. 모든 Geometry 효과를 GeometryDrive에 포함

이동 방향과 홈·요철의 보유 효과를 하나로 묶을 수 있지만 서로 다른 역할을 튜닝하기 어렵다. Curvature는 통과량을 조절하는 `TransferWeight`의 별도 항으로 둔다.
