# Branch 2 — Solver Transfer Weights

브랜치: `feat/solver-transfer-weights`  
선행 조건: `feat/solver-geometry-drive` 병합  
관련 설계: [[03_Architecture/0004_Surface-State-Update|Surface State Update]], [[04_ADR/0016-Transport-Transfer-Weights|ADR 0016]], [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

## 목표

이웃별 거리·법선·곡률·Profile 경계에 따른 TransferWeight를 정리하고 Solver flux에 적용한다. 각 가중치는 전달 구동 방향(Drive)과 구분되는 통과 계수로 `[0,1]` 범위에서 작동한다.

## 구현 범위

```text
RawFlux(i→j)
= (SaturationDrive × SaturationTransferRate
 + GeometryDrive × GeometryTransferRate)
 × DistanceWeight
 × NormalWeight
 × CurvatureWeight
 × ProfileBoundaryWeight
 × DeltaTime
```

- 각 weight 식은 [[04_ADR/0016-Transport-Transfer-Weights|ADR 0016]]을 따른다.
- `DistanceWeight`는 world-space 이웃 거리와 양 endpoint의 평균 유효 이웃 간격으로 계산한다. 기존 Position/Neighbor buffer를 사용하며 NeighborDistance 전용 buffer를 만들지 않는다.
- `NormalWeight`는 per-instance transform이 적용된 두 endpoint의 world-space normal 내적으로 계산한다.
- 이 브랜치에서 `CurvatureWeight = 1.0`으로 둔다. 이는 전달 곡률의 최종 설계 결정이 아니라 중립값을 쓰는 임시 구현 범위다. 곡률이 전달에 어떤 효과를 주는지와 필요한 geometry 입력은 후속 설계 결정으로 남긴다.
- `ConcavityWeight`는 현재 계약대로 Decay의 cavity retention에만 사용한다. 이 브랜치에서 `CurvatureWeight` 계산에 재사용하지 않는다.
- `ProfileBoundaryWeight`는 같은 Profile 사이에서 `1.0`, 서로 다른 Profile 사이에서 고정 `0.5`로 둔다. 이는 Profile parameter가 아닌 Solver의 공통 규칙이므로 `.SRProfile` schema와 GPU Profile ABI를 늘리지 않는다.
- 두 endpoint가 State를 모두 지원하는 경우에만 flux를 계산한다.
- 모든 weight는 Pass 1/2에서 같은 공통 함수로 적용한다.
- 이 결정으로 새 Geometry buffer, Profile parameter 또는 GPU ABI 필드는 추가하지 않는다.

## 설계 경계

`GeometryDrive`는 이동을 일으키는 방향·구동력이고 `TransferWeight`는 그 이웃 관계를 통한 전달량을 조절한다. weight로 중력 방향이나 State 차이를 중복 계산하지 않는다. 필요한 buffer/ABI 변경이 확인되면 [[05_Development/Notes/0003_Surface-State-GPU-Resource|GPU Resource note]]를 함께 갱신한다.

## 검증

- Distance/Normal/Profile 경계 규칙의 기대값과 GPU flux를 비교하고 각 계산이 finite하며 `[0,1]` 범위인지 검사한다.
- 중립 `CurvatureWeight = 1.0`에서 Branch 1 결과가 유지되는지 확인한다.
- Profile 내부와 Profile 경계의 flux를 확정한 규칙과 비교한다.
- seam으로 이어진 이웃과 invalid neighbor에서 기존 계약을 유지한다.
- Geometry weight를 모두 중립값으로 두었을 때 Branch 1 결과가 유지된다.

## 완료 조건

수식, 파라미터 소유권, GPU ABI 변경이 문서화되고 branch 테스트에서 weight별 독립 효과와 상호작용을 재현한다.

## 제외 범위

- State transitions
- 동적 Accumulation geometry
- 실시간 Normal Map integration
- 성능 최적화와 저장형 distance field

## 성능 후속 브랜치

초기 즉시 계산과 buffer 추가 없음은 이 브랜치의 기준이다. 후속 [[02_Planning/02_Weekly-Details/Week-05/0002_01_Branch-Solver-Transfer-Cache|Branch 2.1 — Solver Transfer Cache]]는 현재 HEAD에서 `perf/solver-transfer-cache`로 직접 분기해 TransferWeight 캐시와 RawOutgoing 재사용을 구현한다. 수식은 ADR 0016을 유지하며 저장 정책은 ADR 0017을 따른다.
