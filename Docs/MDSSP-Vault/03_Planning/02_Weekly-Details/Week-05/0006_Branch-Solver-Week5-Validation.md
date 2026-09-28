# Branch 6 — Solver Week 5 통합 검증

> **한 줄 요약:** 5주차 Solver의 보존, source 보유량 제한·초과량 보존, 형상 가중치와 경계 동작을 테스트와 GPU 실행으로 검증한다.

브랜치: `test/solver-week5-validation`  
선행 조건: `feat/solver-debug-tools` 병합
관련 설계: [[04_Architecture/0006_Surface-State-Update|Surface State Update]], [[06_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]], [[07_Testing/0000_Testing-Guide|Testing Guide]]

## 목표

GeometryDrive, TransferWeight, 제한 alpha, Debug controls/stats를 함께 검증하고 Week 5의 결과와 수식·구현 간 차이를 기록한다. 앞 브랜치에서 작성한 단위 fixture를 대체하지 않고 통합 회귀 경로를 추가한다.

## 초과량 보존 계약의 선행 구현

[[../../../05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]은 전체 State A/B에 초과량을 보존하고 전달용 Saturation을 상한 clamp하지 않는다. Shader의 두 상한 clamp 제거는 구현했고 앱·shader·기존 GPU 검사 실행 파일은 빌드했다. 기존 Geometry fixture의 target 결과를 1.0에서 1.4로 바꿔 전체 보존량을 기대하도록 수정했다. 아래 새 계약의 GPU 실행 검증과 추가 fixture는 아직 수행하지 않았다.

## 테스트 범위

- 동일 saturation과 중립 geometry에서 saturation-driven flux가 0.
- 단일 source가 이웃으로 이동하며 decay/input을 끈 경우 총량이 보존.
- Capacity 차이, Profile 지원 여부, Profile 경계가 확정 수식과 일치.
- Geometry 높이/방향과 각 TransferWeight가 독립적으로 예상 flux에 반영.
- 평탄 Normal Map은 기존 geometric normal 기준과 동등하고, 방향 변화가 있는 맵은 해당 이웃의 `NormalWeight`를 낮춘다.
- Normal Map 없음·invalid sample·퇴화 tangent에서 정한 geometric-normal fallback을 적용하고 결과가 finite `[0,1]` 범위다.
- 큰 transfer rate/DeltaTime에서도 outgoing은 Decay 후 전체 available state를 넘지 않고 결과는 finite·비음수. Capacity 상한에 의한 양 삭제 없음.
- invalid texel, UV seam 이웃, dynamic Registry channel count의 동작 유지.
- 동일 총 시간의 `1/30`과 `1/60` 비교를 기록된 tolerance로 평가.
- Pause/Step/Reset 순서와 State/TransferWeight component debug view의 통합 동작 확인.

### 초과량 fixture

| 조건 | 기대 결과 |
|---|---|
| 단일 Inject 또는 여러 이웃 Incoming이 Capacity 초과 | 전체 양이 Next에 남고 InputDelta는 한 번 소비 |
| 동일 Capacity의 State 1.2→1.0, Geometry/Decay/Input OFF | 다음 step의 Saturation 차이로 전달, 닫힌 지원 graph의 State 합 보존 |
| 양쪽 State 1.2, 동일 Capacity, Geometry OFF | Saturation-driven 전달 0; 초과량 삭제 없음 |
| 이웃 없음 또는 rate/weight=0 | 초과량 유지 |
| 큰 rate·Δt, 다른 Capacity·Profile | source 유출 제한, finite·비음수, 기존 barrier·ping-pong 회귀 |

## 검증 방법

1. 작은 synthetic GPU fixture로 수식과 보존 불변 조건을 테스트한다.
2. 전체 build와 기존 CPU/GPU test suite를 실행한다.
3. 지원 GPU에서는 validation layer를 켜고 여러 step을 실행한다.
4. Demo Scene에서 Geometry 구동, Profile 경계, alpha view, pause/step/reset을 수동 확인한다.
5. Vulkan/GPU를 사용할 수 없는 환경의 skip은 테스트 실패와 구분해 결과에 기록한다.

## 결과 기록

[[06_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]] 또는 별도 주차 결과 기록에 다음을 남긴다.

- 테스트 환경, GPU/device 및 timestamp query 지원 여부
- timestep 비교 수치와 사용한 허용 오차
- 검증 warning/error와 해결 여부
- 수식 문서와 코드의 차이, 후속 작업

## 완료 조건

전체 회귀 테스트와 지원 GPU에서의 validation 실행 결과가 기록되고, 계산 불변 조건과 UI 통합이 통과한다. 장치 기능 부재로 실행하지 못한 검사는 skip 사유를 명시한다.

## 제외 범위

새로운 Solver 기능 구현, transition/Accumulation, staging-buffer 전환, 성능 최적화.
