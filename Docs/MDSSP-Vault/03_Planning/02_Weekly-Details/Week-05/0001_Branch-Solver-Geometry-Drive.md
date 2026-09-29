# Branch 1 — Solver Geometry Drive

> **한 줄 요약:** 높이차와 중력 방향을 반영해 GeometryDrive를 계산하고 GPU Solver의 전달에 연결한다.

브랜치: `feat/solver-geometry-drive`
선행 조건: 없음. 4주차 Solver가 병합된 최신 `main`에서 생성한다.
관련 설계: [[04_Architecture/0006_Surface-State-Update|Surface State Update]], [[05_ADR/0015-Geometry-Driven-Transport|ADR 0015]], [[06_Development/Notes/0002_Next-State-Calculation|Next State 계산 메모]]

> **초기 구현 기록:** 이 브랜치의 Capacity clamp 및 기존 검증 결과는 초기 상한 계약 기준이다. [[05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]에서 Capacity를 포화 기준량으로 변경했으며 초과량 보존의 Shader 변경은 구현했고 새 GPU 실행 검증은 대기 중이다. 아래 완료 기록은 당시 결과로 유지한다.

## 목표

현재 Saturation 차이로만 계산하는 flux에 ADR 0015에서 확정한 `GeometryDrive × GeometryTransferRate`를 추가한다. 이 브랜치에서는 수식 결정을 다시 열지 않고 공통 Pass 1/2 경로에 구현하고 검증한다.

## 현재 기준선

- 2-Pass, barrier, A/B ping-pong, InputDelta 소비는 이미 구현되어 있다.
- `GeometryTransferRate`, Position, Normal, MesoVirtualHeight가 GPU에 전달되지만 GeometryDrive 계산은 아직 없다.
- Decay는 기존 Concavity 기반 cavity retention을 사용한다. Geometry transport 식과 혼동하지 않도록 역할을 구분한다.

## 구현 범위

- SaturationDrive와 GeometryDrive를 같은 `rawFlux` 경로에서 합산한다.
- 정적 geometry만 사용한다. Virtual Height 생성 알고리즘이나 동적 geometry 갱신은 다루지 않는다.
- Pass 1과 Pass 2가 동일한 공통 GLSL 함수를 사용한다.
- 비정상 height/방향, 0에 가까운 거리와 invalid neighbor가 flux를 만들지 않게 한다.
- 최소 GPU fixture를 추가하고 확정한 수식·단위와 일치하는지 검증한다. Architecture와 ADR은 [[05_ADR/0015-Geometry-Driven-Transport|ADR 0015]]를 기준으로 삼는다.

## 확정된 계산 계약

- `EffectiveHeight`는 Macro 표면 높이와 `MesoVirtualHeight`를 합쳐 instance transform을 반영한 월드 공간에서 평가한다. 현재 `MesoVirtualHeight = 0`이면 Macro Geometry의 높이만 작용한다.
- `HeightDrive(i → j) = abs(EffectiveHeight_i - EffectiveHeight_j)`로 둔다. **이웃 표면 거리로 나누지 않는다.** 높이차의 크기는 `GeometryDrive`에, 이웃 간 실제 거리는 별도의 `DistanceWeight`에 맡긴다.
- `DirectionDrive(i → j)`는 instance transform을 적용한 면 방향에 World Gravity를 투영하고, 투영된 중력 방향과 월드 공간의 이웃 방향이 얼마나 일치하는지로 계산한다. 중력 반대 방향의 전달은 0으로 제한한다.
- `GeometryTransferRate`는 `State / (world-length · second)` 단위를 갖는다. `DirectionDrive`와 나머지 무차원 계수를 곱하고 `DeltaTime`을 적용하면 flux 단위는 State가 된다.
- `DistanceWeight`는 이웃 표면 거리 효과를 별도로 반영한다. `HeightDrive`에 같은 거리 나눗셈을 다시 넣지 않는다.

구현은 local position에 local normal 방향의 `MesoVirtualHeight` offset을 더한 뒤 per-instance Model Matrix를 적용해 world height를 계산한다. Non-uniform scale을 포함한 면 normal은 inverse-transpose normal transform으로 변환한다. World Gravity의 표면 투영 길이가 epsilon 이하이거나 geometry/방향 값이 비정상이면 Geometry flux를 0으로 처리한다. `DistanceWeight`의 구체식은 2번 브랜치에서 정한다.

## 검증

- [x] GeometryDrive가 0인 기존 saturation-only GPU 회귀 검사가 통과한다.
- [x] instance Z 회전 뒤 World Gravity 방향으로 전달되고, 중력을 반전하면 반대 방향 전달이 차단되는 GPU fixture가 통과한다.
- [x] GeometryDrive source/sink fixture에서 outgoing 제한과 Capacity clamp를 지킨다.
- [x] decay/input을 끈 조건에서 내부 이웃 전달의 총량이 허용 오차 내 보존된다.
- [x] 전체 CTest 4개가 통과한다.
- [x] Apple M1에서 Vulkan validation 활성화 상태로 `--frames 5` 실행이 정상 종료하고 Validation 오류가 없다. 소형 GPU allocation과 depth attachment 관련 권고 warning은 남아 있다.

## 완료 조건

- [x] 수식과 단위가 문서화되고 Pass 1/2가 같은 공통 계산을 사용한다.
- [x] 기존 Saturation 전달 회귀 테스트와 새 Geometry 방향 테스트를 통과한다.
- [x] Vulkan validation 오류가 없다.

## 제외 범위

- Distance/Normal/Curvature/Profile Boundary 가중치 적용
- Normal Map에서 Virtual Height/Curvature 생성
- Accumulation geometry 갱신
- OutgoingFluxScale UI
