# 실험 — Pass 1 비용 분리

- Date: 2026-09-28
- 상태: **원인 분리 및 ADR 0022 적용 완료 · 합성 성능/GPU 회귀 검증 완료**
- 관련: [[05_ADR/Simulation/0021-Directional-RawFlux-Cache|ADR 0021]], [[../../04_Architecture/0007_Surface-Solver-Cache|Solver cache]]

## 실행 화면 관측과 집계

기본 Cube Wetness Scene의 관측은 Frame 13.4 FPS / 74.50 ms, Render GPU 0.38 ms, Solver GPU 74.01 ms, Pass 1 62.61 ms, Pass 2 11.40 ms다. Pass 1은 Solver 표시 시간의 약 84.6%다.

`TRenderer::RenderFrame`은 모든 instance의 Pass 1 timestamp 구간을 합산한다. UI는 1초 동안 이 값을 평균한다. 따라서 62.61 ms는 단일 dispatch나 큐브 한 개의 값이 아니며, 4개 cube instance의 합계다. 균등 분할하면 cube당 평균 15.65 ms이고 각 cube 비용이 같다는 뜻은 아니다. 이전 6×512×512·1채널 합성 측정과 이 화면을 같은 표본으로 비교하지 않는다.

한 장의 화면으로 순간 급등이나 frame별 jitter를 판정하지 않는다. 지속적인 Pass 1의 높은 비용을 설명하는 코드와 합성 분리 결과를 아래에 기록한다.

## 확인한 작업량 — ADR 0022 적용 전

6 Surface × 512×512 × 4 instances = 6,291,456 texel invocation/step이며 Registry channel은 DemoWetness의 1개다. 유효 이웃이 모두 8개인 소스 수준 상한은 50,331,648 directed rawFlux 평가/step이다. 실제 이웃 수·지원 여부·가중치에 따라 더 작다. 화면에 보이지 않는 표면도 Solver 대상이다.

- Pass 1에는 State=0 또는 감쇠 후 가용량=0인 source의 비싼 flux 평가를 생략하는 경로가 없다. Current와 AvailableState는 이웃별 RawFlux를 다 합친 뒤에 계산한다.
- `initializeGeometryDrive`는 각 유효 texel invocation에서 instance 공통 inverse-transpose와 gravity up 축을 준비한다. 실제 기계 명령 수는 compiler 최적화에 따라 달라진다.
- `geometryDrive(source,target)`는 이웃마다 source 법선 읽기·행렬 변환·길이·정규화, source 중력 투영·길이, source displaced position을 다시 계산한다. source가 같으면 이 값들은 이웃마다 동일하다. target 위치·edge 길이·방향·높이 차이는 이웃별 계산이 필요하다.
- ADR 0021은 Pass 2의 RawFlux 재평가를 없앴다. 위 Pass 1 계산은 유지되며 RawFlux 저장 쓰기가 추가됐다.
- RawFlux 쓰기는 instance별 float32, slot-major plane, 6×512×512·1채널·8슬롯, scalar 원소 padding 없음에서 instance당 48 MiB/step이다. 4 instances는 192 MiB/step의 payload store다. allocator padding 및 다른 buffer 접근은 제외하며 실제 DRAM transaction 양과 동일시하지 않는다.
- 기본 DemoWetness의 GeometryTransferRate=50은 곱셈 계수다. 값이 크다고 반복 횟수가 늘지 않는다.

## 합성 분리 측정

Apple M1, 한 instance에 6개 Surface × 512×512, 1채널, 모든 texel 유효 grid, 최대 8개 이웃이다. 실제 Cube Mesh·Normal Map·Render는 포함하지 않는다. 위치 `(0.01x,0.01y,0.0025x)`, normal `normalize(-0.25,0,1)`, identity transform, gravity `(0,0,-1)`, dt=1/60이다. Profile은 ADR 0021과 같은 DemoWetness 수치를 사용하며 Input=0, ConcavityWeight=0이다.

각 dispatch는 같은 State A를 읽고 B에 쓴다. State는 `0.25 + 0.5 × (i % 31) / 30`이며 dry 조건은 전부 0이다. 현행 shader를 사용해 GeometryDrive/SaturationDrive push flag만 변경했다. 5회 warmup + 30회 표본 중앙값을 사용하며 두 번 반복하고 두 번째는 측정 순서를 뒤집었다. CPU geometry/cache 준비·upload는 측정 밖이다.

| 조건 | Pass 1 run 1 / run 2 (ms) |
|---|---|
| 기본 ON / State 전체 양수 | 19.572 / 19.981 |
| GeometryDrive OFF | 3.938 / 11.216 |
| SaturationDrive OFF | 17.816 / 18.138 |
| GeometryDrive와 SaturationDrive 모두 OFF | 5.834 / 4.020 |
| State 전부 0 / 기본 ON | 16.816 / 16.249 |
| State 전부 0 / GeometryDrive OFF | 7.321 / 4.261 |

시간 편차가 커서 항목별 비용을 차이 값으로 정확히 분해하거나 더해서 전체 시간을 예측하지 않는다. GeometryDrive를 끈 조건의 큰 감소와 dry에서도 유지되는 비용은, Pass 1 형상 경로가 주된 비용이며 빈 source 생략이 없다는 코드 관찰을 뒷받침한다. SaturationDrive만 끄는 효과는 이 표본에서 작았다. CPU 전처리와 TransferWeight cache rebuild는 이 query 구간에 포함되지 않는다. 형상 연산과 관련 buffer read 중 어느 쪽이 GPU 실행을 제한하는지는 이 측정만으로 분리하지 않는다.

## source 중복 계산을 제거한 임시 후보

분석용 shader에서 source 법선 변환·정규화, 중력 투영과 길이, displaced source position을 invocation당 한 번 준비해 이웃 평가가 재사용하도록 했다. source inverse-transpose 준비와 이웃별 target/edge 계산, RawFlux 쓰기, Profile/State 규칙은 유지했다. 추가 GPU buffer는 없다. 현행 runtime shader에 적용한 변경은 아니다.

별도 두 반복에서 source 재사용 후보와 현행을 번갈아 측정했으며 모든 배열의 Next State·RawOutgoing·alpha를 비교했다.

| 조건 | 현행 Pass 1 (ms) | source 재사용 후보 (ms) | 최대 절대 오차 |
|---|---:|---:|---:|
| 전체 State / run 1 | 25.198 | 15.621 | 5.96e-8 |
| 전체 State / run 2 | 19.082 | 12.993 | 5.96e-8 |
| dry / run 1 | 13.808 | 12.813 | 1.40e-9 |
| dry / run 2 | 14.106 | 13.262 | 1.40e-9 |

임시 후보 shader로 기존 Surface GPU resource/solver 회귀 검사도 통과했다. Virtual Height·normal, 비균일 scale, 중력 반전, term toggle, 여러 channel·seam 슬롯·초과량 보존·입력 소비 fixture를 포함하며 validation 오류가 없었다.

source 재사용의 개선은 전체 State 조건에서 약 32–38%, dry에서 약 6–7%다. 시간 편차와 위 다른 실험 run을 섞지 않으며 실제 Scene 성능 개선률로 환산하지 않는다. 여러 Profile/channel의 GeometryTransferRate=0 경로에서는 불필요한 source 준비를 피하도록 실제 적용 시 준비 시점을 검토한다.

## 가용량 0의 source 생략 범위

생략 후보는 텍셀 전체 갱신이 아니라 Pass 1의 **해당 texel·channel에서 나가는 전달량 평가**다. `AvailableState = max(Current - Decay, 0)`이고 실제 outgoing은 `RawOutgoing × alpha`이므로, 가용량이 0이면 계산 전에도 실제 outgoing이 0임을 알 수 있다. 이 판단에는 별도 활동 비트 마스크가 필요하지 않다.

Pass 2는 계속 이웃 source의 캐시를 gather하고 InputDelta를 소비해 `Next = max(Current + Input + Incoming - Outgoing - Decay, 0)`를 기록한다. 자기 가용량이 0이어도 이웃 유입이나 외부 입력을 받을 수 있다. 이번 step에 받은 양은 다음 step의 Current가 되므로 다음 step부터 outgoing 평가 대상이다. 여러 Registry channel 중 하나의 가용량이 0인 경우에도 다른 channel은 독립적으로 처리한다.

생략 경로에서도 재사용 scratch의 이전 값이 남지 않도록 해당 channel의 8개 RawFlux 슬롯과 RawOutgoing·alpha를 0으로 덮어써야 한다. 이는 Next State와 실제 전달량을 보존하지만, 가용량이 0일 때의 제한 전 RawOutgoing과 alpha 디버그 값은 초기 구현과 달라질 수 있다. 후속 ADR 0022에서 이 경로를 runtime shader에 적용했다.

## 추가 중복 연산 검토 — ADR 0022 적용 전

아래 횟수는 유효 이웃 8개인 소스 코드의 평가 상한이다. GPU compiler가 일부 계산과 읽기를 합칠 수 있으며, source 형상 재사용 임시 후보 외의 각 항목은 별도 성능 측정을 하지 않았다.

| 항목 | 현행 반복 범위 | 재사용 가능한 범위와 적용 제한 |
|---|---|---|
| source 포화도 `Current / Capacity` | SaturationDrive가 켜져 있으면 이웃당 한 번, 최대 8회/channel | source·channel당 한 번. target의 포화도는 이웃마다 다르다. |
| source 지원 검사·profile index·profile parameters | Pass 1 외부 지원 검사 이후에도 각 `rawFlux`에서 source 지원 검사를 반복하고 profile을 다시 조회한다. source Capacity도 포화도 함수에서 재조회한다. | 이미 지원을 확인한 source·channel의 Parameters와 Current를 이웃 루프 밖에서 준비한다. target의 지원 여부와 Capacity 검사는 유지한다. |
| instance normal matrix·gravity up 축 | 각 유효 texel invocation의 `initializeGeometryDrive` | transform·gravity가 같은 instance의 dispatch 공통 값이다. CPU 또는 instance 공통 자원에서 준비하는 후보이며 push constant/layout 변경 비용을 함께 검토한다. 이 행렬은 이웃당 8회가 아니라 현행에서도 texel당 한 번 준비한다. |
| source projected gravity의 방향 정규화·공통 유효성 검사 | `length(SolverUp)`와 source 방향 `GravityOnSurface / SurfaceGravityLength`가 `geometryDrive`마다 평가된다. | source 형상 준비에 유효 여부와 정규화된 방향을 포함해 이웃들이 재사용한다. 기존 임시 후보도 source 방향 나눗셈은 이웃 평가 안에 남아 있다. target edge 길이·정규화는 이웃별로 필요하다. |
| 같은 edge의 GeometryDrive·neighbor index·TransferWeight | channel 루프 안에서 이웃 루프를 반복하므로 여러 channel에서 동일 값을 재평가/재조회한다. | 형상·topology·가중치는 channel 독립이다. 여러 channel에서 재사용할 수 있으나 현재 1채널 wetness 데모에는 channel 간 절감이 없다. edge 배열의 register 압력과 1채널 성능을 검증해야 하며 기존 배열 후보는 안정적인 개선이 확인되지 않아 미채택이다. |
| 동일 texel·channel의 `decayAmount` | Pass 1에서 가용량 계산, Pass 2에서 Next 계산으로 두 번 | 두 pass가 같은 Current·Profile·형상·dt·flags를 읽는다. 재사용하려면 pass 간 저장과 읽기가 필요하므로 단순 루프 이동과 다르다. 추가 메모리 접근 대비 이득을 측정하기 전에는 우선순위를 낮춘다. |

이웃별 target 위치·포화도와 source→target 방향은 실제로 다르므로 source와 같은 값으로 취급하지 않는다. 반대 방향 `rawFlux(j,i)`도 포화도 차이·source 계수·중력 방향에 따라 달라지므로 `rawFlux(i,j)`를 그대로 재사용할 수 없다.

## 후속 우선순위

아래는 원인 분리 시점의 우선순위다. 1–4의 가용량 0 생략·source 공통 형상·instance 공통 행렬·source channel 공통 조회는 ADR 0022에서 적용했다. 여러 channel의 edge 배열과 pass 간 Decay 캐시는 보류하며 실제 Scene 시계열은 별도 과제다. 적용 후 동일 512 해상도의 합성 비교 결과는 [[05_ADR/Simulation/0022-Pass1-Source-Reuse|ADR 0022]]에 기록했다. 기본 해상도는 후속 ADR 0023에서 Medium 256으로 변경했으므로 최초 화면 관측과 기본 실행 시간을 직접 비교하지 않는다.

1. 가용 State=0인 source의 실제 outgoing은 source alpha 제한으로 0이다. Pass 1의 형상·포화도 계산을 생략하는 경로를 검토한다. Input은 기존대로 Pass 2에 반영하므로 새 입력의 전달 시점은 다음 step이다. 생략 시 RawFlux·RawOutgoing·alpha scratch의 0 기록 의미와 디버그 계약을 함께 정의해야 한다.
2. 위 source 공통 geometry 재사용을 검토한다. channel 수가 달라도 source geometry는 동일하며, GeometryDrive가 실제 필요한 경우에 한 번 준비한다.
3. instance 공통 inverse-transpose와 gravity up 축을 instance 단위로 준비해 invocation당 반복을 줄이는 방안을 검토한다. 비용 절감 폭은 따로 측정한다.
4. source·channel 공통 포화도·Profile 조회·지원 검사를 이웃 루프 밖으로 옮기는 후보를 검토한다. 여러 channel의 edge 재사용과 pass 간 Decay 재사용은 별도 측정 후 판단한다.
5. 실제 Scene의 네 instance별 Pass 1/2와 frame별 timestamp를 수집해 Virtual Meso Geometry·회전·State 분포·시간 편차를 분리한다. 이 기록은 상시 높은 Pass 1의 원인을 분석했으며 순간 급등 원인은 별도 시계열이 필요하다.
