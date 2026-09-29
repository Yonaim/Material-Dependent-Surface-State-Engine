# ADR 0015 — Geometry-Driven Transport의 높이·방향·거리 계약

> **한 줄 요약:** `GeometryDrive`를 `HeightDrive × DirectionDrive`로 계산해 `TransferWeight`와 결합한다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-27
- 관련 문서: [[0002-Transport-Drive-and-Weight|ADR 0002 — Transport Drive와 Weight]], [[../04_Architecture/0006_Surface-State-Update|Surface State Update]]

## 후속 결정 — 2026-09-29

ADR 0031에서 기존 HeightDrive×DirectionDrive에 출발 Saturation을 별도 mobility로 곱하도록 확장했다. 아래 높이·방향 정의는 유지한다.

## Context

ADR 0002는 State를 움직이는 구동력(`SaturationDrive`, `GeometryDrive`)과 이웃 관계의 통과성(`TransferWeight`)을 분리했다. Geometry Integration 설계는 Transport가 높이차에 따른 중력 영향과 이웃 표면 거리를 고려해야 한다고 정하지만, Solver의 구체 계산 계약은 정해져 있지 않았다.

특히 높이차를 이웃 거리로 나누면 `DistanceWeight`와 같은 거리를 중복 반영할 수 있다. Shared Geometry는 Mesh-local로 여러 instance가 공유되므로, 한 instance의 회전·스케일을 공유 데이터에 bake하면 다른 instance의 방향과 위치에 잘못 적용된다.

## Decision

- Geometry transport는 `GeometryDrive = HeightDrive × DirectionDrive`로 계산하고, 이후 다른 통과성 계수와 함께 `TransferWeight`를 곱한다.
- `EffectiveHeight`는 Macro Surface 높이와 Virtual Height(`MesoVirtualHeight`)의 합이다. 두 항은 instance transform을 반영한 world-length로 비교한다. 현재 Virtual Height가 0이면 Macro Surface 높이만 반영된다.
- `HeightDrive(i→j) = abs(EffectiveHeight_i - EffectiveHeight_j)`로 정의한다. 인접 texel 간 거리로 나누지 않는다.
- `DirectionDrive`는 source 면 방향에 투영한 World Gravity와 source→target의 world-space 이웃 방향의 정렬도로 정의한다. 정렬도가 높을수록 해당 방향의 geometry flux가 커지며, 투영 중력과 반대인 이웃 방향은 0으로 처리한다. 투영 중력 길이가 epsilon 이하이면 `DirectionDrive = 0`이다.
- DirectionDrive의 source normal은 기본적으로 복원 MesoNormal을 사용하며, GPU pack에서 sampled TransferNormal, macro normal 순으로 fallback한다. 비교용 UI `DirectionDrive: MesoNormal`을 OFF로 두면 macro mesh normal을 선택한다. 이 선택은 HeightDrive나 TransferWeight의 NormalWeight 입력을 변경하지 않는다. GeometryDrive가 OFF이면 선택 효과도 없다.
- Instance transform은 per-instance Solver evaluation에 적용한다. Shared Geometry position/normal을 world-space로 bake하거나 instance마다 복제하지 않는다. World position에는 instance transform을 적용하고, surface normal에는 inverse-transpose normal transform을 적용한다.
- 이웃 표면 간격은 `DistanceWeight`가 소유한다. HeightDrive에는 neighbor distance normalization을 다시 넣지 않는다. `DistanceWeight`의 구체 감쇠 곡선은 별도 구현 결정으로 둔다.
- Profile 입력은 초기 Rate 저장 방식에서 version 2의 `[0,1]` 무차원 `GeometryTransferFactor`로 변경했다. 아래 `GeometryTransferRate`는 `GeometryTransferFactor × BaseGeometryTransferRate(100.0)`로 구한 실제 속도다 ([[0029-Normalized-Transport-Factors|ADR 0029]]). 높이·방향·거리 계약은 유지한다.
- `HeightDrive`는 world-length, `DirectionDrive`와 `TransferWeight`는 무차원으로 둔다. `GeometryTransferRate` 단위는 `State / (world-length · second)`이며 시간 적분은 `Δt`를 곱한다.
- GeometryDrive는 Transport flux 안에서만 작동한다. 이는 직접 수신량을 더하지 않고 source에서 target으로 향하는 outgoing flux를 만든다. Pass 1/2는 같은 공통 flux 함수를 사용한다.

## Alternatives Considered

### 1. 높이차를 표면 거리로 나눠 slope를 사용

`HeightDrive = abs(ΔHeight) / SurfaceDistance`로 두면 같은 경사에 대해 해상도나 texel 간격에 덜 민감하다. 하지만 이 아키텍처는 neighbor 거리 효과를 `DistanceWeight`에 별도 배정한다. 같은 거리를 두 항에서 다시 적용하지 않도록 나눗셈을 채택하지 않는다.

### 2. 부호 있는 높이차 하나로 downhill 방향까지 표현

높이차 부호로 전달 방향을 결정하면 DirectionDrive의 별도 방향 계수가 필요 없어진다. 기존 GeometryDrive 분해와 면에 투영한 중력 방향의 정렬도를 명시적으로 검증하기 위해 높이차는 크기만 제공하고 DirectionDrive가 방향을 제한한다.

### 3. Instance transform을 Shared Geometry에 bake

월드 위치·법선을 instance별로 미리 만들어 둘 수 있다. 그러나 같은 Mesh Geometry를 공유하는 instance마다 값이 달라져 Shared Geometry의 공유 계약을 깨뜨리고 데이터 복제가 필요하므로 채택하지 않는다.

## Consequences

- GeometryDrive는 높이차의 크기를 구하고 DirectionDrive는 중력에 맞는 이웃 방향을 고른다. 둘은 동일한 방향 판정을 반복하지 않는다.
- `DistanceWeight`는 거리 감쇠를 별도로 표현하되 구체 함수와 기준 거리 선택을 정해야 한다.
- instance 회전과 스케일은 각 Solver invocation에 전달되는 per-instance Model Matrix로 반영한다. Solver는 mesh-local 위치를 world space로 변환하고, normal은 inverse-transpose normal transform으로 변환한다. World Gravity도 push constant로 함께 전달한다.
- normal texture에서 Virtual Meso Geometry의 height/normal/curvature fields를 생성하는 알고리즘과 dynamic accumulation geometry는 이 ADR의 범위가 아니다.
- outgoing flux 보유량 제한과 Capacity clamp는 기존 2-Pass 규칙을 그대로 따른다.

## Related

- [[0002-Transport-Drive-and-Weight|ADR 0002 — Transport Drive와 TransferWeight 분리]]
- [[../04_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[../04_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[../08_Assets/Documents/0006_Geometry-Integration|Geometry Integration]]
- [[../03_Planning/02_Weekly-Details/Week-05/0001_Branch-Solver-Geometry-Drive|Week 5 Branch 1 — Solver Geometry Drive]]
- [[../03_Planning/02_Weekly-Details/Week-06/0000_Week6-Branch-Plan|Week 6 Branch Plan]]
