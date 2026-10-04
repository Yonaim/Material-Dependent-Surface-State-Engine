# Solver Validation

> **한 줄 요약:** 해상도, timestep, texel 면적과 Geometry rate가 Solver 결과에 미치는 영향을 한 문서에서 검증한다.

## 실험 초안 — 해상도와 시간 간격에 따른 Transport

- 상태: **전체 분포·성능 실험 계획 / 미실시**
- 마지막 소스 확인: 2026-09-29
- 범위: 현재 총량 모델의 해상도·시간 오차, alpha와 GPU 비용
- 구현 계약: [[0009_Texel-Area-and-State-Amounts|면적·총량]], [[0010_Geometry-Transport-Mobility|Geometry mobility]], [[0011_Accumulated-Simulation-Timestep|누적 시간]]

### 현재 구현과 구분할 사항

| 항목 | 현재 구현 | 실험으로 확인할 것 |
|---|---|---|
| State | texel별 총량, 보존 장부 `ΣState` | 입력·감쇠 장부를 뺀 총량 오차 |
| 면적 | Macro UV Jacobian footprint를 instance scale로 변환 | UV 왜곡·chart 경계 중심 sample 오차 |
| Capacity·입력·Decay | 고정 `ReferenceArea=1/256²`에 대한 값을 실제 면적 비율로 환산 | 같은 면적·밀도의 전체 Capacity·입력량 일치 |
| Geometry 전달 | 기존 높이차×방향×source Saturation, 기준 Rate 6000, 포화도 상한 없음 | 같은 밀도에서 월드 이동 거리의 해상도 의존 |
| Saturation 전달 | 이웃 포화도 차이, 기존 상대 DistanceWeight | 퍼짐과 이류를 분리한 공간 수렴 |
| 시간 | 실제 시간×배속 누적. 기본 Fixed ON·Auto OFF는 1/60초, Auto ON에서만 보수적 Transport 간격으로 세분화 | 옵션별 분포 차이, dt-halving, backlog 증가 여부 |
| GPU 반복 | frame당 최대 8회, 새 State·RawFlux 사용, 입력 한 번 | frame 합산 GPU 비용과 처리량 |

현재 모델은 외부 속도장을 정의하는 일정 속도 이류 모델이 아니다. Geometry coefficient를 월드 속도로 직접 사용하지 않는다. 아래 비교는 현재 모델의 동작 검증이며 물리 정확성 또는 해상도 독립성의 증명은 아니다.

### 1. 같은 출발 조건 만들기

128·256·512에서 동일한 월드 영역에 동일한 **면적당 분포**를 준비하고 `State_i=density(WorldPosition_i)×Area_i`로 초기화한다. 총량 합이 초기 허용 오차 안에 드는지 먼저 확인한다. 같은 텍셀 수나 같은 클릭 횟수를 조건으로 사용하지 않는다.

- Macro analytic 경사 평면, 하나의 Registry channel, 같은 Profile부터 시작한다.
- 첫 실험은 Normal Map·Decay·입력 OFF, Curvature OFF, Profile 경계 없는 닫힌 영역이다.
- Shape·world transform·Profile Factor·ReferenceArea는 해상도별로 동일하게 둔다.
- 해상도 변경은 State·clock을 초기화하므로 매 run에 초기 분포를 다시 만든다.
- shader binary/hash, commit/diff, GPU·driver·build·validation 상태를 기록한다.

### 2. 비교 단계

| 단계 | 조건 | 목적 |
|---|---|---|
| A | Geometry만 ON, 공통 작은 dt, 세 해상도 | downhill 중심 이동 거리·분포·총량 |
| B | Saturation만 ON, 공통 작은 dt, 세 해상도 | 포화도 차의 퍼짐·분포·총량 |
| C | 둘 다 ON, 같은 조건 | 두 경로의 합성 결과 |
| D | 한 해상도에서 dt, dt/2, dt/4, 같은 총 시간 | 시간 간격 오차와 alpha 제한 |
| E | 15·30·60·120 FPS cadence, 같은 실제 경과 시간, Fixed·Auto 네 조합 | 누적 시간·진행 시간·잔여 시간·Solver step 수 |
| F | 동일 월드 반경의 사건 입력, falloff 0/1 | 입력 총량의 해상도 의존과 부분 texel 오차 |
| G | 비균일 면적·UV 왜곡·seam·Profile 경계·비균일 scale | 단순 평면에서 벗어난 조건 |
| H | Cube Wetness Scene, 세 해상도·cache ON/OFF | 실제 분포와 GPU·메모리 비용 |

각 비교의 **실제 실행한 Solver 시간 합**을 맞춘다. 큰 dt 한 번과 작은 dt 여러 번을 같다고 가정하지 않는다. 반복 한도로 backlog가 남으면 같은 시뮬레이션 시간까지 추가로 소비한 비용도 기록한다.

현재 기본 Fixed ON·Auto OFF와 Auto ON을 별도 조건으로 비교한다 ([[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]). Fixed ON·Auto ON에서는 1/60초 단위로 시작한 구간의 내부 substep을 기록한다. Auto OFF·Fixed OFF는 경과 시간을 한 번에 계산하므로 cadence에 따른 분포 차이도 측정한다.

### 3. 왜 세 해상도를 비교하는가

같은 밀도에서 간격을 절반으로 하면 면적·State·Capacity는 약 1/4, 포화도는 같고 Geometry 높이차는 약 1/2이다. 상대 DistanceWeight도 유지된다. 따라서 Geometry 원시 전달/보유량 비율은 약 두 배가 되고 이동 간격은 절반이다. 이를 통해 월드 이동 거리 차이가 줄어드는지 확인한다.

이 설명은 균일 평면과 alpha가 제한하지 않는 조건이다. 여러 방향 유출, UV 왜곡, Normal Map 재생성, 경계가 있으면 결과가 달라질 수 있다. HeightDrive를 거리로 나누거나 Weight를 추가 1/d로 변경하는 후보는 이번 구현에 포함하지 않는다.

### 4. 측정 지표와 판정

| 지표 | 기록 |
|---|---|
| 보존 장부 | `ΣState`, 입력 합, Decay 합, 경계 출입량; State에 면적을 다시 곱하지 않음 |
| 월드 분포 | State 총량으로 가중한 중심·폭·downhill 이동 거리 |
| 공통 영역 비교 | `density=State/Area`를 같은 월드 평가 영역에 환산한 L1/L2 차이 |
| 시간 | 실제 시간, 실행한 dt 합, backlog, frame별 step 수 |
| 제한 | alpha 최소·평균·alpha<1 비율, min State, NaN/Inf |
| 비용 | frame의 Pass 1·2 합, step당 비용, CPU 상한 계산, 면적 buffer와 cache 크기 |

잠정 기준은 닫힌 영역의 상대 총량 오차 `1e-4` 이내, 이동 거리 차이 5% 이내, 비음수·finite 유지다. 성능·분포의 최종 허용 기준은 미확정이다. 차이가 크면 alpha와 dt-halving 결과부터 확인하며 이유 없이 허용 오차를 완화하지 않는다.

### 결과와 구현 검증의 구분

**전체 월드 분포·GPU 성능 비교는 미실시다.** CPU clock·면적 및 작은 GPU fixture의 회귀 테스트는 [[0004_Solver-Validation#검증 — 면적 환산과 누적 시간|면적·시간 회귀 검증]]에 별도로 기록한다. 단위 테스트 통과를 실제 Cube Scene의 세 해상도 결과가 같다는 근거로 사용하지 않는다.

| Fixture | 해상도 | dt / step 수 | 진행 시간 | 이동 거리 | 총량 오차 | alpha<1 | frame GPU ms |
|---|---:|---|---:|---:|---:|---:|---:|
| 측정 대기 | — | — | — | — | — | — | — |

### 관련 문서

- State Update
- UI와 backlog
- Semi-Lagrangian 후보
- 보존형 이류 배경

## 검증 — 면적 환산과 누적 시간

- 상태: **구현 회귀 통과 / 전체 Scene 해상도·성능 비교 미실시**
- 날짜: 2026-09-29
- 브랜치: `feat/area-aware-transport-and-timestep`
- 계약: [[0009_Texel-Area-and-State-Amounts|면적·총량]], [[0010_Geometry-Transport-Mobility|Geometry mobility]], [[0011_Accumulated-Simulation-Timestep|누적 시간]]

### 확인 대상

State는 texel 총량이다. Capacity·입력·Decay는 `AreaScale=WorldArea/(1/256²)`로 환산한다. Geometry는 출발 `State/Capacity`에 비례하고 1에서 자르지 않는다. 실제 경과 시간×배속을 누적해 갱신된 State로 여러 step을 실행한다.

### 테스트와 결과

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

Geometry 기준값 6000 재보정 후에도 위 검사의 목표 실제 Rate는 유지한다. 새 기본 Factor 0.5와 8-neighbor 국소 이동률 검사는 [[0004_Solver-Validation#검증 — Geometry 전달 기준값 재보정|Geometry 재보정 검증]]에 별도로 기록한다.

Fixed·Auto 분리의 실행 정책은 [[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]를 따른다. Auto ON·Fixed ON의 substep들은 1/60초 구간을 완성하며, 기본 Auto OFF는 CPU Transport 상한으로 dt를 변경하지 않는다.

`MDSS_SceneResources`는 실제 Debug UI 기본값 Fixed ON·Auto OFF와 Renderer의 15 FPS 경과 시간 입력을 검사했다. 한 frame에서 네 번의 고정 Solver 실행으로 약 1/15초가 진행됐다. CPU clock는 Auto·Fixed를 모두 끄고 다시 고정 모드로 돌아왔을 때 이전 미완료 구간이 다시 나타나지 않는지도 확인한다.

### 실행 검증

전체 C++·shader 빌드와 CTest 8개를 실행했다. 기본 Scene의 8-frame smoke 실행이 정상 종료했고 Vulkan validation 오류는 없었다. 기존 소규모 allocation 및 depth attachment의 best-practices 경고는 남아 있다.

관련 코드: `Tests/SimulationTransportTests.cpp`, `Tests/SurfaceGPUResourceTests.cpp`, `Tests/SceneResourceTests.cpp`, `Tests/SurfaceCacheTests.cpp`.

### 한계와 다음 실험

- CPU cadence 테스트는 GPU가 해당 step 수를 처리할 수 있다는 성능 증명이 아니다. 지속 과부하에서는 backlog가 증가한다.
- Macro footprint·chart 중심 sampling의 근사는 남아 있다. Meso 면적과 부분 texel clipping은 포함하지 않는다.
- 단일 이웃의 월드 이동량 일치는 일반 8-neighbor graph·seam·Profile 경계·Cube 전체의 해상도 독립성을 증명하지 않는다.
- dt-halving, 동일 월드 초기분포와 입력의 128·256·512 비교, alpha와 frame GPU 비용은 [[0004_Solver-Validation#실험 초안 — 해상도와 시간 간격에 따른 Transport|전체 비교 실험 초안]]을 따른다.

## 검증 — Geometry 전달 기준값 재보정

- 상태: **재보정 구현·회귀 통과 / 전체 Cube 이동·성능 비교 미실시**
- 날짜: 2026-09-29
- 브랜치: `feat/area-aware-transport-and-timestep`
- 계약: [[0012_Geometry-Rate-Recalibration|Decision 0012]], [[0009_Texel-Area-and-State-Amounts|면적·총량]], [[0010_Geometry-Transport-Mobility|Geometry mobility]], [[0011_Accumulated-Simulation-Timestep|누적 시간]]

### 재보정 대상

초기 Geometry 기준값 100과 Factor 0.5는 Rate 50이다. 기준 면적 1/256², Capacity 1, 균일한 수직면과 기존 TransferWeight를 사용한 CPU 계산에서 국소 질량 중심 이동률은 약 0.001684 world-length/s였다. Medium에서는 약 0.431 texel/s로, SaturationDrive를 꺼도 중력 방향 이동이 작다.

기준값을 100→6000으로 바꾸고 Factor는 유지했다. 기본 Factor 0.5의 Rate는 3000이다. 이 Rate는 월드 이동 속도 자체가 아니라 Flux를 만드는 전달량 계수다. State·면적·포화도·기존 GeometryDrive와 TransportWeight 식은 유지한다. C++와 GLSL은 `SurfaceSolverRates.h`의 같은 상수를 사용한다.

### GPU 검사 조건

`TestCalibratedGeometrySpeed`는 5×5 texel fixture를 사용한다. 중앙 source와 그 이웃 8개는 각자 8개 이웃을 모두 가지므로 바깥 경계의 영향 없이 첫 step의 이동량을 검사한다.

- 월드 위치: 수직 평면 `x=0`, 간격 `h=1/R`, normal `(1,0,0)`, identity transform.
- texel 면적: `h²`. 기존 CPU builder로 TransferWeight와 월드 면적을 만든다.
- Capacity: 기준 면적당 1. Geometry Factor: 0.5. Saturation Factor와 Decay: 0. 외부 입력: 0.
- 초기 State: 중앙만 `2 × AreaScale`, 나머지 0. 출발 포화도 2를 사용해 1 이상의 mobility도 검사한다.
- 중력: `(0,0,-1)`. `dt=0.001초`. GPU Solver를 한 step 실행한다.
- 이동률: `하향 질량 중심 이동 거리 / dt`. 총량 오차는 초기 총량 대비 10⁻⁶ 미만, 모든 값은 유한하고 0 이상이어야 한다.

### 측정 결과

| 해상도 R | cache ON 이동률 (world-length/s) | cache OFF 이동률 (world-length/s) | 총량·비음수 검사 |
|---|---:|---:|---|
| 128 | 0.101033 | 0.101033 | 통과 |
| 256 | 0.101033 | 0.101033 | 통과 |
| 512 | 0.101033 | 0.101033 | 통과 |

기대값 0.101033333과의 절대 오차 10⁻⁵ 미만을 검사한다. 같은 fixture의 초기 계수 CPU 계산값보다 약 60배 크다. 이전 계수의 GPU 측정을 새로 수행한 A/B 성능 실험은 아니다.

### 안전 시간 간격과 기존 회귀

`MDSS_SceneResources`의 수직 Medium triangle fixture에서 Geometry Factor 0.5의 안전 간격은 0.012~0.016초 사이였고, Factor를 1로 올리면 간격이 절반이 됐다. GeometryDrive를 끄면 1/60초 상한으로 돌아왔다. Shader 기준값만 올리고 CPU 상한을 그대로 두는 오류를 검출한다.

기존 GPU 검사의 목표 실제 Rate 1·2·8은 `Factor=목표 Rate/현재 기준값`으로 구성한다. 따라서 기존 mobility·alpha·Geometry 방향성 검증 조건은 유지되며, 별도 이동률 검사가 새 기본 Factor의 보정을 검증한다.

전체 C++·shader 빌드와 CTest 8개가 통과했다. 기본 Cube Wetness Scene의 8-frame 실행은 정상 종료했고 Vulkan validation 오류는 없었다. 실행 검증은 FPS 또는 장시간 backlog 측정이 아니다.

### 한계와 다음 실험

- 결과는 한 step의 국소 질량 중심 이동률이다. 1초 이상 이동한 전체 분포의 속도를 측정한 값이 아니다.
- 전체 Cube의 Meso 요철·UV 왜곡·seam·Profile 경계·alpha·Decay·GPU backlog에 따라 실제 이동과 퍼짐은 달라진다.
- Geometry 기준값은 공용이므로 Geometry Factor가 양수인 모든 Registry channel에 적용된다. 기존 자산의 Factor를 재환산하지 않는다.
- Auto substepping ON에서는 더 큰 Rate가 안전 간격을 줄여 필요한 Solver 반복 수를 늘릴 수 있다. 현재 기본 Fixed ON·Auto OFF에서는 dt=1/60초가 유지된다. frame당 8회 한도와 미처리 시간 이월은 유지된다 ([[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]).
- 같은 월드 초기분포의 128·256·512 이동 거리·분포·총량, dt-halving 및 GPU 비용 비교는 [[0004_Solver-Validation#실험 초안 — 해상도와 시간 간격에 따른 Transport|해상도·시간 비교 실험 초안]]을 따른다.

관련 코드: `Source/SurfaceStateSystem/Types/SurfaceSolverRates.h`, `Tests/SurfaceGPUResourceTests.cpp`, `Tests/SceneResourceTests.cpp`.
