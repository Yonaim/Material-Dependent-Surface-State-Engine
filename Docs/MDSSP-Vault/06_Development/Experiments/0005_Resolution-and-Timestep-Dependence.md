# 실험 초안 — 해상도와 시간 간격에 따른 Transport

> **한 줄 요약:** 면적 환산·Geometry 비례 전달·누적 시간 구현을 대상으로 같은 월드 초기조건과 진행 시간에서 128·256·512 결과를 비교한다.

- 상태: **전체 분포·성능 실험 계획 / 미실시**
- 마지막 소스 확인: 2026-09-29
- 범위: 현재 총량 모델의 해상도·시간 오차, alpha와 GPU 비용
- 구현 계약: [[../../05_ADR/0030-Texel-Area-and-State-Amounts|면적·총량]], [[../../05_ADR/0031-Geometry-Transport-Mobility|Geometry mobility]], [[../../05_ADR/0032-Accumulated-Simulation-Timestep|누적 시간]]

## 현재 구현과 구분할 사항

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

## 1. 같은 출발 조건 만들기

128·256·512에서 동일한 월드 영역에 동일한 **면적당 분포**를 준비하고 `State_i=density(WorldPosition_i)×Area_i`로 초기화한다. 총량 합이 초기 허용 오차 안에 드는지 먼저 확인한다. 같은 텍셀 수나 같은 클릭 횟수를 조건으로 사용하지 않는다.

- Macro analytic 경사 평면, 하나의 Registry channel, 같은 Profile부터 시작한다.
- 첫 실험은 Normal Map·Decay·입력 OFF, Curvature OFF, Profile 경계 없는 닫힌 영역이다.
- Shape·world transform·Profile Factor·ReferenceArea는 해상도별로 동일하게 둔다.
- 해상도 변경은 State·clock을 초기화하므로 매 run에 초기 분포를 다시 만든다.
- shader binary/hash, commit/diff, GPU·driver·build·validation 상태를 기록한다.

## 2. 비교 단계

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

현재 기본 Fixed ON·Auto OFF와 Auto ON을 별도 조건으로 비교한다 ([[../../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]]). Fixed ON·Auto ON에서는 1/60초 단위로 시작한 구간의 내부 substep을 기록한다. Auto OFF·Fixed OFF는 경과 시간을 한 번에 계산하므로 cadence에 따른 분포 차이도 측정한다.

## 3. 왜 세 해상도를 비교하는가

같은 밀도에서 간격을 절반으로 하면 면적·State·Capacity는 약 1/4, 포화도는 같고 Geometry 높이차는 약 1/2이다. 상대 DistanceWeight도 유지된다. 따라서 Geometry 원시 전달/보유량 비율은 약 두 배가 되고 이동 간격은 절반이다. 이를 통해 월드 이동 거리 차이가 줄어드는지 확인한다.

이 설명은 균일 평면과 alpha가 제한하지 않는 조건이다. 여러 방향 유출, UV 왜곡, Normal Map 재생성, 경계가 있으면 결과가 달라질 수 있다. HeightDrive를 거리로 나누거나 Weight를 추가 1/d로 변경하는 후보는 이번 구현에 포함하지 않는다.

## 4. 측정 지표와 판정

| 지표 | 기록 |
|---|---|
| 보존 장부 | `ΣState`, 입력 합, Decay 합, 경계 출입량; State에 면적을 다시 곱하지 않음 |
| 월드 분포 | State 총량으로 가중한 중심·폭·downhill 이동 거리 |
| 공통 영역 비교 | `density=State/Area`를 같은 월드 평가 영역에 환산한 L1/L2 차이 |
| 시간 | 실제 시간, 실행한 dt 합, backlog, frame별 step 수 |
| 제한 | alpha 최소·평균·alpha<1 비율, min State, NaN/Inf |
| 비용 | frame의 Pass 1·2 합, step당 비용, CPU 상한 계산, 면적 buffer와 cache 크기 |

잠정 기준은 닫힌 영역의 상대 총량 오차 `1e-4` 이내, 이동 거리 차이 5% 이내, 비음수·finite 유지다. 성능·분포의 최종 허용 기준은 미확정이다. 차이가 크면 alpha와 dt-halving 결과부터 확인하며 이유 없이 허용 오차를 완화하지 않는다.

## 결과와 구현 검증의 구분

**전체 월드 분포·GPU 성능 비교는 미실시다.** CPU clock·면적 및 작은 GPU fixture의 회귀 테스트는 [[0006_Area-and-Timestep-Regression|면적·시간 회귀 검증]]에 별도로 기록한다. 단위 테스트 통과를 실제 Cube Scene의 세 해상도 결과가 같다는 근거로 사용하지 않는다.

| Fixture | 해상도 | dt / step 수 | 진행 시간 | 이동 거리 | 총량 오차 | alpha<1 | frame GPU ms |
|---|---:|---|---:|---:|---:|---:|---:|
| 측정 대기 | — | — | — | — | — | — | — |

## 관련 문서

- [[../../04_Architecture/0006_Surface-State-Update|State Update]]
- [[../../04_Architecture/0010_UI-Interface|UI와 backlog]]
- [[../../02_Research/0002_Semi-Lagrangian-Transport|Semi-Lagrangian 후보]]
- [[../../02_Research/0003_Conservative-Advection|보존형 이류 배경]]
