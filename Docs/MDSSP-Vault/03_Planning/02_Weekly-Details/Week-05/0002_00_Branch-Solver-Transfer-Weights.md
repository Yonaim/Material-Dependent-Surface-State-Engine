# Branch 2 — Solver Transfer Weights

> **한 줄 요약:** 이웃 사이 거리·normal·곡률·Profile 경계에 따른 TransferWeight 계산을 정의하고 구현한다.

브랜치: `feat/solver-transfer-weights`  
선행 조건: `feat/solver-geometry-drive` 병합  
관련 설계: [[04_Architecture/0006_Surface-State-Update|Surface State Update]], [[05_ADR/0016-Transport-Transfer-Weights|ADR 0016]], [[06_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

상태: **구현 및 GPU 실행 검증 완료**

## 후속 변경 (2026-09-28)

초기 구현은 CurvatureWeight 고정 1.0이었다. 현재는 기본 OFF(1.0)를 유지하며 ON에서 Virtual Height로부터 사전 계산한 mean curvature 기반 비교용 감쇠를 적용한다. [[05_ADR/0019-Optional-Curvature-Transfer-Weight|ADR 0019]]가 현재 결정이며 아래의 고정값 구현 범위는 초기 branch 기록이다.

## 구현 현황

TransferWeight 계산과 flux 적용은 Branch 2.1의 캐시 구현에 포함되어 현재 브랜치에 병합되었다. 중복되는 계산 경로를 추가하지 않는다.

| 계약                           | 현재 구현                                                                                    |
| ---------------------------- | ---------------------------------------------------------------------------------------- |
| DistanceWeight               | CPU cache builder가 MesoVirtualHeight 및 instance transform이 반영된 위치와 이웃 정보를 사용해 계산한다.      |
| NormalWeight                 | 현재 구현은 Mesh geometric normal의 world-space 내적으로 계산한다. Normal Map 입력은 Branch 2.2에서 추가할 계획이다. |
| CurvatureWeight              | 중립값 `1.0`을 사용한다.                                                                         |
| ProfileBoundaryWeight        | 동일 Profile은 `1.0`, 다른 Profile은 `0.5`를 사용한다.                                              |
| Flux 적용                      | `rawFlux`가 cache 가중치를 읽으며 Pass 1/2 모두 같은 규칙을 적용한다. 양방향 incoming은 대칭인 cache 가중치를 공유해 읽는다. |
| State 미지원 / invalid geometry | State 지원 검사에서 flux를 0으로 처리하며, invalid 이웃 슬롯의 cache 값은 0이다.                               |

주 구현은 `BuildSurfaceGPUTransferWeights`와 `SurfaceSolverCommon.glsl`에 있다. `Tests/SurfaceGPUResourceTests.cpp`에는 거리/Profile 경계와 법선 가중치의 GPU flux 사례가 추가되어 있다.

## 검증 결과

- `MDSS_SurfaceGPUResource` GPU 실행 테스트: 통과.
- Apple M1에서 `MDSS --frames 5` 실행: 5프레임 처리 후 정상 종료.
- Vulkan validation: 활성화 상태에서 core 또는 synchronization 오류는 보고되지 않았다.
- 남은 best-practices 경고: 작은 buffer/image마다 별도 메모리 할당을 사용하는 점과 depth attachment에 `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`를 사용할 수 있다는 권고. Solver cache 접근이나 flux 계산 관련 validation 오류는 관찰되지 않았다.

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

- 각 weight 식은 [[05_ADR/0016-Transport-Transfer-Weights|ADR 0016]]을 따른다.
- `DistanceWeight`는 world-space 이웃 거리와 양 endpoint의 평균 유효 이웃 간격으로 계산한다. 기존 Position/Neighbor buffer를 사용하며 NeighborDistance 전용 buffer를 만들지 않는다.
- 현재 `NormalWeight`는 per-instance transform이 적용된 두 endpoint의 Mesh geometric normal 내적으로 계산한다. 이 branch의 기존 구현 범위를 기록하며, Normal Map 방향 입력은 [[03_Planning/02_Weekly-Details/Week-05/0002_02_Branch-Solver-Normal-Map-Weights|Branch 2.2]]에서 다룬다.
- 이 브랜치에서 `CurvatureWeight = 1.0`으로 둔다. 이는 전달 곡률의 최종 설계 결정이 아니라 중립값을 쓰는 임시 구현 범위다. 곡률이 전달에 어떤 효과를 주는지와 필요한 geometry 입력은 후속 설계 결정으로 남긴다.
- `ConcavityWeight`는 현재 계약대로 Decay의 cavity retention에만 사용한다. 이 브랜치에서 `CurvatureWeight` 계산에 재사용하지 않는다.
- `ProfileBoundaryWeight`는 같은 Profile 사이에서 `1.0`, 서로 다른 Profile 사이에서 고정 `0.5`로 둔다. 이는 Profile parameter가 아닌 Solver의 공통 규칙이므로 `.SRProfile` schema와 GPU Profile ABI를 늘리지 않는다.
- 두 endpoint가 State를 모두 지원하는 경우에만 flux를 계산한다.
- 모든 weight는 Pass 1/2에서 같은 공통 함수로 적용한다.
- 이 결정으로 새 Geometry buffer, Profile parameter 또는 GPU ABI 필드는 추가하지 않는다.

## 설계 경계

`GeometryDrive`는 이동을 일으키는 방향·구동력이고 `TransferWeight`는 그 이웃 관계를 통한 전달량을 조절한다. weight로 중력 방향이나 State 차이를 중복 계산하지 않는다. 필요한 buffer/ABI 변경이 확인되면 [[06_Development/Notes/0003_Surface-State-GPU-Resource|GPU Resource note]]를 함께 갱신한다.

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
- Normal Map에서 MesoVirtualHeight를 복원하는 적분 및 non-integrable fallback은 [[03_Planning/02_Weekly-Details/Week-05/0002_03_Branch-Solver-Meso-Geometry|Branch 2.3]]의 PCG least-squares 경로에 구현한다.
- 성능 최적화와 저장형 distance field

## 성능 후속 브랜치

초기 즉시 계산과 buffer 추가 없음은 이 브랜치의 기준이다. 후속 [[03_Planning/02_Weekly-Details/Week-05/0002_01_Branch-Solver-Transfer-Cache|Branch 2.1 — Solver Transfer Cache]]는 현재 HEAD에서 `perf/solver-transfer-cache`로 직접 분기해 TransferWeight 캐시와 RawOutgoing 재사용을 구현한다. 수식은 ADR 0016을 유지하며 저장 정책은 ADR 0017을 따른다.
