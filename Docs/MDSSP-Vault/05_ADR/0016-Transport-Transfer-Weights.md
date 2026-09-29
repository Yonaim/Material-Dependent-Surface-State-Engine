# ADR 0016 — Transport TransferWeight 계산 계약

> **한 줄 요약:** `TransferWeight(i→j)`는 `DistanceWeight × NormalWeight × CurvatureWeight × ProfileBoundaryWeight`다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-27
- 관련 문서: [[0002-Transport-Drive-and-Weight|ADR 0002 — Transport Drive와 Weight]], [[0015-Geometry-Driven-Transport|ADR 0015 — Geometry-Driven Transport]]

## Context

ADR 0002는 State를 이동시키는 Drive와 이웃 관계를 통과하는 정도인 TransferWeight를 분리했다. ADR 0015에서 GeometryDrive의 높이와 방향을 확정한 뒤, Solver는 이웃 간 거리, 면 방향, Profile 경계에 따른 전달 보정을 필요로 한다. CurvatureWeight와 기존 ConcavityWeight의 역할도 구별해야 한다.

## Decision

- `TransferWeight(i→j)`는 `DistanceWeight × NormalWeight × CurvatureWeight × ProfileBoundaryWeight`다. 모든 가중치는 유한한 `[0,1]` 값이다.
- `DistanceWeight`는 world-space 이웃 거리 `d(i,j)`를 endpoint들의 로컬 평균 이웃 간격 `dRef(i,j)`로 정규화한다.

  ```text
  meanDistance(i) = 유효한 이웃까지의 world-space 거리 평균
  dRef(i,j) = 0.5 × (meanDistance(i) + meanDistance(j))
  DistanceWeight(i,j) = clamp(dRef(i,j) / d(i,j), 0, 1)
  ```

  평균 이웃 간격과 endpoint 거리가 epsilon 이하이거나 유한하지 않으면 `DistanceWeight = 0`이다. 현재 유효 표면 Position과 Neighbor 관계에서 계산한다. 유효 Position은 base geometry에 MesoVirtualHeight와 AccumulationHeight를 반영하고 instance transform을 적용한 값이다. 평균과 endpoint 거리가 유한하고 epsilon보다 커야 한다. 초기 구현은 별도 distance buffer 없이 계산했으며, cache 정책은 [[0017-Solver-Transfer-Cache|ADR 0017]]을 따른다.
- `NormalWeight(i→j) = clamp(dot(NormalWorld_i, NormalWorld_j), 0, 1)`이다. map normal이 있으면 Simulation texel mapping의 triangle/barycentric 좌표로 Material UV sample을 얻고, texel tangent frame에서 복원한 mesh-local transfer normal을 사용한다. map normal이 없거나 sample/tangent frame이 유효하지 않으면 geometric normal로 fallback한다. 양쪽 normal은 instance transform의 inverse-transpose를 적용한 뒤 정규화한다. 유효 normal이 끝내 만들어지지 않으면 가중치 0이다.
- `ProfileBoundaryWeight`는 양 endpoint가 해당 State를 지원할 때 같은 Profile 사이에서는 `1.0`, 서로 다른 Profile 사이에서는 고정 `0.5`다. Profile별 설정값이 아니라 공통 Solver 규칙으로 둔다.
- 기본 `CurvatureWeight = 1.0`을 유지한다. 현재 선택적 사전 계산 모드와 식은 [[0019-Optional-Curvature-Transfer-Weight|ADR 0019]]를 따른다. 초기안에서는 고정값만 사용했다. 현재 `NormalWeight`가 양 endpoint의 유효 normal 차이를 이미 반영하고, 이웃 방향 법곡률로 별도 가중치를 만들면 같은 형상 굽힘을 다시 감쇠할 가능성이 있기 때문이다. 중립값은 곡률 데이터를 만들지 않는다는 뜻이 아니라, 해당 데이터가 Transport flux를 추가로 바꾸지 않는다는 뜻이다.
- 선택적 곡률 모드는 비교용이며 기본 ON 채택 여부는 중복되지 않는 역할과 측정 가능한 이득이 확인될 때 재검토한다. 재검토 시 `NormalWeight`만 적용한 결과와 비교해 곡률 항의 추가 효과를 검증하고, 수식·필요 데이터·단위를 별도로 결정한다.
- `ConcavityWeight`는 기존처럼 local Decay의 cavity retention에만 사용한다. 이를 `CurvatureWeight`로 재사용해 Transport flux를 감쇠하지 않는다.
- 초기 Branch 2는 새 distance buffer, Profile parameter, GPU Profile ABI 필드를 추가하지 않았다. 후속 ADR 0017은 인스턴스별 Solver 캐시 buffer를 추가하며 Profile ABI와 전달 수식은 유지한다. Pass 1과 Pass 2의 flux 정의는 동일하다.

## Alternatives Considered

### 1. 절대 거리로 직접 가중

`1 / (1 + d)`와 같은 식은 길이 단위와 씬 크기 선택에 따라 값이 달라진다. 주변 이웃 간격으로 거리 비율을 만든 뒤 `[0,1]`로 제한한다.

### 2. Profile마다 경계 전달 계수 추가

Profile별 조절성을 제공하지만 asset schema와 GPU Profile record를 확장해야 한다. MVP에서는 모든 Profile 경계에 같은 `0.5` 규칙을 사용한다.

### 3. ConcavityWeight를 CurvatureWeight로 재사용

ConcavityWeight는 texel-local Decay retention 데이터다. 이를 edge transport에 재사용하면 감쇠와 이동 억제가 결합되고, 오목한 영역의 State 보유 효과를 중복 적용할 수 있어 채택하지 않는다.

### 4. 별도 곡률 기반 전달 감쇠

주곡률·주방향 또는 별도 signed edge curvature를 만들 수 있지만 Geometry preprocessing 범위와 GPU data layout을 넓히며, NormalWeight와 형상 굽힘 효과를 중복할 수 있다. 독립적인 이득이 검증되기 전에는 CurvatureWeight를 중립값으로 둔다.

## Consequences

- 서로 다른 Simulation resolution과 크기의 Surface 사이에서도 `DistanceWeight`는 주변 이웃 간격에 대한 상대 거리로 동작한다.
- 급격히 다른 surface normal을 가진 이웃은 전달량이 줄며, 반대 방향 또는 직교 normal 내적은 0이 된다.
- 서로 다른 Profile 사이 전달은 동일 Profile 사이 전달의 절반이며 asset별 튜닝 값은 없다.
- 기본 OFF의 CurvatureWeight는 Transport 결과를 바꾸지 않으며, NormalWeight와 중복될 수 있어 의도적으로 중립값을 사용한다. 향후 곡률 효과를 채택하려면 ConcavityWeight/Decay뿐 아니라 NormalWeight와도 구별되는 의도와 검증 결과가 필요하다.

## Related

- [[0002-Transport-Drive-and-Weight|ADR 0002 — Transport Drive와 TransferWeight 분리]]
- [[0015-Geometry-Driven-Transport|ADR 0015 — Geometry-Driven Transport]]
- [[../04_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[../06_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[../03_Planning/02_Weekly-Details/Week-05/0002_00_Branch-Solver-Transfer-Weights|Week 5 Branch 2 — Solver Transfer Weights]]
