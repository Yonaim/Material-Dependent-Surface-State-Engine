# Decision 0012 — Geometry 전달 기준값 재보정

> **한 줄 요약:** 느리던 Geometry 이동을 조정하고 CPU와 GPU가 같은 기준 속도를 사용하도록 했다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-29

## 쉽게 읽기

Geometry 이동이 지나치게 느렸던 공통 기준 속도를 6000으로 올린다. Profile 계수와 Solver의 계산은 C++·GLSL 공용 상수를 사용한다.

## Context — 왜 필요했나

Geometry Factor 0.5·기준 Rate 100은 실제 Rate 50이다. 이 숫자는 월드 이동 속도 50이 아니다. 고정 기준 면적 1/256², Capacity 1, 평평한 수직면의 균일한 8-neighbor graph에서는 퍼짐·감쇠 없이도 국소 질량 중심 이동률이 약 0.001684 world-length/s다. Medium의 약 0.431 texel/s에 해당해 흐름을 눈으로 확인하기 어렵다.

면적·총량·출발 포화도 모델은 유지하고 전달 기준값을 재보정한다. 단순 평면의 해상도 보정과 목표 흐름 속도의 튜닝은 별도 문제다.

## Decision — 무엇을 정했나

- `BaseGeometryTransferRate`를 100→6000으로 변경한다. `BaseSaturationTransferRate=1`은 유지한다.
- Profile version 2와 Geometry Factor의 `[0,1]` 범위, 자산·runtime override의 Factor 수치는 유지한다. 기본 0.5의 실제 Geometry Rate는 3000이다.
- `Source/SurfaceState/Types/SurfaceSolverRates.h`를 C++/GLSL 공용 상수 정의로 사용한다. Shader include 경로와 빌드 의존성에 이 파일을 포함한다.
- CPU의 안전 transport 간격 계산도 같은 Geometry 기준값을 사용한다. 한쪽에 별도의 숫자 100이나 6000을 하드코딩하지 않는다.
- State는 총량, Capacity·입력·Decay는 면적 환산을 유지한다. 출발 `State/Capacity`를 1에서 자르지 않고 SaturationDrive를 별도로 유지한다.
- HeightDrive·DirectionDrive·DistanceWeight·source alpha·2-Pass와 시간 누적 정책은 유지한다. 당시 cache ON/OFF도 유지했으며 후속 Decision 0025에서 제거했다.

| Profile | Factor | 초기 Geometry Rate | 재보정 Geometry Rate |
|---|---:|---:|---:|
| DemoWetness | 0.5 | 50 | 3000 |
| DemoStone | 0.5 | 50 | 3000 |
| DemoMud | 0.005 | 0.5 | 30 |

이 기준값은 Solver 공통이므로 Geometry Factor가 양수인 모든 Registry channel에 영향을 준다. 특정 State 종류를 고정하지 않는다.

## Alternatives Considered — 다른 방법

- Profile Factor를 0.5→1로 높이기: 두 배의 변화로 기본 흐름의 낮은 이동률을 충분히 보정하지 못한다.
- Time scale 높이기: 모든 과정의 진행 속도를 함께 바꾸므로 Geometry만 재보정하는 목적과 다르다.
- HeightDrive/DistanceWeight 수식 변경: 흐름 방향·해상도 의존 모델을 바꾸므로 이번 기준값 보정에 포함하지 않는다.

## Consequences — 결정의 영향

단순 균일 수직면의 국소 이동률은 기본 Factor 0.5에서 약 0.101033 world-length/s다. 이는 Meso 요철·UV 왜곡·seam·alpha·Decay·GPU backlog가 있는 전체 Cube의 실제 이동 속도를 보장하지 않는다.

Auto substepping ON에서는 Rate가 커지면 안전 간격이 작아져 더 많은 Solver 반복이 필요할 수 있다. CPU 안전 간격은 같은 공용 Rate를 사용하며 frame당 8회 한도를 넘긴 시간은 이월한다. 현재 기본값은 Fixed ON·Auto OFF로, dt=1/60초는 Rate에 따라 바뀌지 않는다 ([[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]). 지속 GPU 과부하의 지연은 제거하지 못한다. 전체 분포·성능 비교 계획은 유지한다.

기존 geometry·alpha 테스트의 실제 Rate 1·2·8 조건은 Factor=목표 Rate/현재 기준 Rate로 구성해 기존 검증 조건을 유지한다. 별도 재보정 GPU fixture는 Factor 0.5를 고정해 128·256·512와 cache ON/OFF에서 이동률 약 0.101033을 검사한다. Scene fixture는 Geometry Factor 변경에 맞춰 안전 간격이 줄어드는지 확인한다.

기존 cache는 정적 Geometry이므로 재생성할 필요가 없다. Shader는 재빌드해야 한다. 이전 Profile 수치에 맞춘 분포와 benchmark 결과는 측정 당시 기준 Rate를 함께 기록한다.

## Related — 관련 문서

- [[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008]]
- [[05_Decisions/0009_Texel-Area-and-State-Amounts|Decision 0009]]
- [[05_Decisions/0010_Geometry-Transport-Mobility|Decision 0010]]
- [[05_Decisions/0011_Accumulated-Simulation-Timestep|Decision 0011]]
- [[03_Architecture/0006_Surface-State-Update|State Update]]
- 재보정 검증
