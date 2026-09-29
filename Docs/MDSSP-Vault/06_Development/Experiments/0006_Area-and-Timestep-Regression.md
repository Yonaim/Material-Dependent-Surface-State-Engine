# 검증 — 면적 환산과 누적 시간

> **한 줄 요약:** CPU clock·면적, GPU mobility·Decay·입력 반복 및 Scene 접촉 경로를 회귀 테스트로 검증한다.

- 상태: **구현 회귀 통과 / 전체 Scene 해상도·성능 비교 미실시**
- 날짜: 2026-09-29
- 브랜치: `feat/area-aware-transport-and-timestep`
- 계약: [[../../05_ADR/0030-Texel-Area-and-State-Amounts|면적·총량]], [[../../05_ADR/0031-Geometry-Transport-Mobility|Geometry mobility]], [[../../05_ADR/0032-Accumulated-Simulation-Timestep|누적 시간]]

## 확인 대상

State는 texel 총량이다. Capacity·입력·Decay는 `AreaScale=WorldArea/(1/256²)`로 환산한다. Geometry는 출발 `State/Capacity`에 비례하고 1에서 자르지 않는다. 실제 경과 시간×배속을 누적해 갱신된 State로 여러 step을 실행한다.

## 테스트와 결과

| 범위 | 조건 | 확인 결과 |
|---|---|---|
| CPU 시간 | 15·30·60·120 FPS cadence, 배속 1, step 1/60초 | 모두 1초에 60 step, 진행 시간 1초 |
| 반복 한도 | 1초 누적 후 한 frame 소비 | 최대 8회 실행, 약 0.867초 미처리 시간 유지 |
| Pause·Step | Pause 시간 추가, 수동 step | Pause 시간 누적 없음, 수동 1회, 기존 backlog 유지 |
| 배속·부분 step | 배속 2, Auto ON의 작은 안전 간격, Fixed ON/OFF | 배속은 시간 budget에 적용, Fixed는 전체 구간이 모일 때 시작, OFF는 마지막 부분 소비 |
| Fixed·Auto 분리 | 네 옵션 조합, 기본 Fixed ON·Auto OFF, 가상의 작은 Transport 상한 | 기본 dt=1/60초는 상한을 무시. variable·Auto OFF는 누적 시간 한 번, ON은 상한으로 분할 |
| 고정 구간 세분화 | 1/60초 미만 누적, 비정수 분할, 구간당 16 substep | 전체 구간이 모일 때 시작, 마지막 짧은 step으로 완료, 8회 한도 이후 다음 frame 재개 |
| 옵션 전환·Reset | 진행 중 고정 구간에서 Auto OFF, 미완료 구간 Reset | 잔여 시간 한 번 완료 후 새 고정 구간 사용, Reset은 미완료 구간도 제거 |
| CPU 면적 | 기울어진 UV 평면, 128·256·512, 비균일 scale | 월드 면적 합과 전체 Capacity 일치, 반사에서도 양의 면적 |
| Cache | `.Surface` format 4 round-trip | AreaVector 포함 모든 texel 필드 정확히 복원 |
| GPU Geometry | source 면적 2×기준, Capacity=2, State 4→8 | 포화도 2→4, 전달량 0.2→0.4; SaturationDrive OFF에서도 작동 |
| GPU 면적·Saturation | 면적 비율 2:4, State 2:4 | 같은 포화도라 Saturation 전달 0 |
| GPU Decay | 같은 포화도, 면적 2:4, 기준 rate 1, dt 0.1 | 총량 감소 0.2:0.4 |
| GPU 연속 반복 | 한 command buffer에 두 step, 입력 0.3 | 입력 한 번, 두 번째 step에서 갱신된 State의 0.015 전달, 합 0.3 |
| GPU 국소 해상도 | 단일 수직 이웃, 간격 1/R, 면적 1/R², R=128·256·512 | 한 step의 총량 보존과 질량 중심 월드 이동량 일치 |
| GPU 면적 0 | 기존 State 3,7 유지 | 전달·감쇠 없이 총량 유지 |
| 실제 접촉 경로 | Scene fixture의 균일 falloff 입력 | 지원 texel의 양이 InputFactor×AreaScale과 일치, 미지원 channel 제외 |

GPU 검사는 RawFlux cache ON/OFF 양쪽에서 같은 기대값을 사용한다. 국소 해상도 검사는 2-texel fixture에서 공간 계수의 스케일을 확인하는 검사이며 전체 초기 분포의 이동·퍼짐 검증은 아니다.

Geometry 기준값 6000 재보정 후에도 위 검사의 목표 실제 Rate는 유지한다. 새 기본 Factor 0.5와 8-neighbor 국소 이동률 검사는 [[0007_Geometry-Rate-Recalibration|Geometry 재보정 검증]]에 별도로 기록한다.

Fixed·Auto 분리의 실행 정책은 [[../../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]]를 따른다. Auto ON·Fixed ON의 substep들은 1/60초 구간을 완성하며, 기본 Auto OFF는 CPU Transport 상한으로 dt를 변경하지 않는다.

`MDSS_SceneResources`는 실제 Debug UI 기본값 Fixed ON·Auto OFF와 Renderer의 15 FPS 경과 시간 입력을 검사했다. 한 frame에서 네 번의 고정 Solver 실행으로 약 1/15초가 진행됐다. CPU clock는 Auto·Fixed를 모두 끄고 다시 고정 모드로 돌아왔을 때 이전 미완료 구간이 다시 나타나지 않는지도 확인한다.

## 실행 검증

전체 C++·shader 빌드와 CTest 8개를 실행했다. 기본 Scene의 8-frame smoke 실행이 정상 종료했고 Vulkan validation 오류는 없었다. 기존 소규모 allocation 및 depth attachment의 best-practices 경고는 남아 있다.

관련 코드: `Tests/SimulationTransportTests.cpp`, `Tests/SurfaceGPUResourceTests.cpp`, `Tests/SceneResourceTests.cpp`, `Tests/SurfaceCacheTests.cpp`.

## 한계와 다음 실험

- CPU cadence 테스트는 GPU가 해당 step 수를 처리할 수 있다는 성능 증명이 아니다. 지속 과부하에서는 backlog가 증가한다.
- Macro footprint·chart 중심 sampling의 근사는 남아 있다. Meso 면적과 부분 texel clipping은 포함하지 않는다.
- 단일 이웃의 월드 이동량 일치는 일반 8-neighbor graph·seam·Profile 경계·Cube 전체의 해상도 독립성을 증명하지 않는다.
- dt-halving, 동일 월드 초기분포와 입력의 128·256·512 비교, alpha와 frame GPU 비용은 [[0005_Resolution-and-Timestep-Dependence|전체 비교 실험 초안]]을 따른다.
