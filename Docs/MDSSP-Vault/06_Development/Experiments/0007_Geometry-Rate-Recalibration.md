# 검증 — Geometry 전달 기준값 재보정

> **한 줄 요약:** Geometry 기준 Rate를 6000으로 보정한 뒤, 단순 수직면에서 기본 Factor 0.5의 국소 이동률 약 0.101 world-length/s와 CPU 안전 간격을 확인했다.

- 상태: **재보정 구현·회귀 통과 / 전체 Cube 이동·성능 비교 미실시**
- 날짜: 2026-09-29
- 브랜치: `feat/area-aware-transport-and-timestep`
- 계약: [[../../05_ADR/0033-Geometry-Rate-Recalibration|ADR 0033]], [[../../05_ADR/0030-Texel-Area-and-State-Amounts|면적·총량]], [[../../05_ADR/0031-Geometry-Transport-Mobility|Geometry mobility]], [[../../05_ADR/0032-Accumulated-Simulation-Timestep|누적 시간]]

## 재보정 대상

초기 Geometry 기준값 100과 Factor 0.5는 Rate 50이다. 기준 면적 1/256², Capacity 1, 균일한 수직면과 기존 TransferWeight를 사용한 CPU 계산에서 국소 질량 중심 이동률은 약 0.001684 world-length/s였다. Medium에서는 약 0.431 texel/s로, SaturationDrive를 꺼도 중력 방향 이동이 작다.

기준값을 100→6000으로 바꾸고 Factor는 유지했다. 기본 Factor 0.5의 Rate는 3000이다. 이 Rate는 월드 이동 속도 자체가 아니라 Flux를 만드는 전달량 계수다. State·면적·포화도·기존 GeometryDrive와 TransportWeight 식은 유지한다. C++와 GLSL은 `SurfaceSolverRates.h`의 같은 상수를 사용한다.

## GPU 검사 조건

`TestCalibratedGeometrySpeed`는 5×5 texel fixture를 사용한다. 중앙 source와 그 이웃 8개는 각자 8개 이웃을 모두 가지므로 바깥 경계의 영향 없이 첫 step의 이동량을 검사한다.

- 월드 위치: 수직 평면 `x=0`, 간격 `h=1/R`, normal `(1,0,0)`, identity transform.
- texel 면적: `h²`. 기존 CPU builder로 TransferWeight와 월드 면적을 만든다.
- Capacity: 기준 면적당 1. Geometry Factor: 0.5. Saturation Factor와 Decay: 0. 외부 입력: 0.
- 초기 State: 중앙만 `2 × AreaScale`, 나머지 0. 출발 포화도 2를 사용해 1 이상의 mobility도 검사한다.
- 중력: `(0,0,-1)`. `dt=0.001초`. GPU Solver를 한 step 실행한다.
- 이동률: `하향 질량 중심 이동 거리 / dt`. 총량 오차는 초기 총량 대비 10⁻⁶ 미만, 모든 값은 유한하고 0 이상이어야 한다.

## 측정 결과

| 해상도 R | cache ON 이동률 (world-length/s) | cache OFF 이동률 (world-length/s) | 총량·비음수 검사 |
|---|---:|---:|---|
| 128 | 0.101033 | 0.101033 | 통과 |
| 256 | 0.101033 | 0.101033 | 통과 |
| 512 | 0.101033 | 0.101033 | 통과 |

기대값 0.101033333과의 절대 오차 10⁻⁵ 미만을 검사한다. 같은 fixture의 초기 계수 CPU 계산값보다 약 60배 크다. 이전 계수의 GPU 측정을 새로 수행한 A/B 성능 실험은 아니다.

## 안전 시간 간격과 기존 회귀

`MDSS_SceneResources`의 수직 Medium triangle fixture에서 Geometry Factor 0.5의 안전 간격은 0.012~0.016초 사이였고, Factor를 1로 올리면 간격이 절반이 됐다. GeometryDrive를 끄면 1/60초 상한으로 돌아왔다. Shader 기준값만 올리고 CPU 상한을 그대로 두는 오류를 검출한다.

기존 GPU 검사의 목표 실제 Rate 1·2·8은 `Factor=목표 Rate/현재 기준값`으로 구성한다. 따라서 기존 mobility·alpha·Geometry 방향성 검증 조건은 유지되며, 별도 이동률 검사가 새 기본 Factor의 보정을 검증한다.

전체 C++·shader 빌드와 CTest 8개가 통과했다. 기본 Cube Wetness Scene의 8-frame 실행은 정상 종료했고 Vulkan validation 오류는 없었다. 실행 검증은 FPS 또는 장시간 backlog 측정이 아니다.

## 한계와 다음 실험

- 결과는 한 step의 국소 질량 중심 이동률이다. 1초 이상 이동한 전체 분포의 속도를 측정한 값이 아니다.
- 전체 Cube의 Meso 요철·UV 왜곡·seam·Profile 경계·alpha·Decay·GPU backlog에 따라 실제 이동과 퍼짐은 달라진다.
- Geometry 기준값은 공용이므로 Geometry Factor가 양수인 모든 Registry channel에 적용된다. 기존 자산의 Factor를 재환산하지 않는다.
- Auto substepping ON에서는 더 큰 Rate가 안전 간격을 줄여 필요한 Solver 반복 수를 늘릴 수 있다. 현재 기본 Fixed ON·Auto OFF에서는 dt=1/60초가 유지된다. frame당 8회 한도와 미처리 시간 이월은 유지된다 ([[../../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]]).
- 같은 월드 초기분포의 128·256·512 이동 거리·분포·총량, dt-halving 및 GPU 비용 비교는 [[0005_Resolution-and-Timestep-Dependence|해상도·시간 비교 실험 초안]]을 따른다.

관련 코드: `Source/SurfaceStateSystem/Types/SurfaceSolverRates.h`, `Tests/SurfaceGPUResourceTests.cpp`, `Tests/SceneResourceTests.cpp`.
