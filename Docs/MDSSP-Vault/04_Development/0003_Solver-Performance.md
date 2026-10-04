# Solver Performance

> **한 줄 요약:** Solver의 병목 가설·비교 실험·측정 규칙과 결과 해석을 한 문서에서 관리한다. 원자료는 `Performance-Results/`에 둔다.

## 성능 측정 규칙

### 기본 원칙

- 한 번에 하나의 비교 변수만 바꾸고 Scene·입력·해상도·Profile·Solver 설정은 고정한다.
- 준비 구간을 제외한 고정 frame/step 구간을 반복 측정한다. 기본 benchmark matrix는 30 warm-up frame과 180 measurement frame을 사용한다.
- 평균 FPS 하나가 아니라 **GPU Solver total / Pass 1 / Pass 2 / geometry update / transfer weight**, 반복별 중앙값과 p95를 함께 본다.
- 기준과 후보를 같은 머신·빌드에서 번갈아 실행하고, 속도와 함께 총량 보존·NaN/Inf·분포 오차를 확인한다.
- 한 Scene이나 한 GPU의 개선률을 일반적인 개선률로 표현하지 않는다.

### 대표 Scene

| Scene | 용도 |
|---|---|
| `BrickCube.Scene` | 기본 Solver workload, 다중 Surface 기준 |
| `Mountain.Scene` | 다중 instance workload |
| `Bunny.Scene` | 복잡 Mesh / mapping·geometry·render 비용이 가설에 포함될 때 |

세 Scene의 workload는 동일하지 않으므로 모든 실험에서 무조건 함께 비교하지 않는다.

### 자동 측정

저장소 루트에서 기본 matrix를 실행한다.

```sh
Scripts/run_performance_benchmarks.sh
```

앱 단독 실행 예시는 다음과 같다.

```sh
Build/bin/MDSS --benchmark-scene Assets/Scenes/BrickCube.Scene \
  --benchmark-resolution 256 --benchmark-warmup-frames 30 \
  --benchmark-measure-frames 180 \
  --benchmark-output /tmp/brickcube-frames.jsonl
```

각 run set의 `manifest.json`, `raw/*.jsonl`, 앱 로그, `summary.md`는 `Performance-Results/`에 보존한다. 이 파일들은 **재현용 원자료**이고, 가설과 결론은 아래 실험 기록에 적는다.

> **한 줄 요약:** Solver pass, GeometryDrive, RawFlux cache와 셰이더 핫패스의 비용을 한 문서에서 비교한다.

## 실험 — Solver Pass 비교

- 상태: **계획**
- 근거: [[06_Assets/Documents/0005_Next-State-Calculation.pdf|Next State 계산]]

### 가설

이웃 alpha를 매번 재계산하는 1-Pass보다 `alpha`를 저장하는 2-Pass가 중복 계산을 줄인다.

### 비교 조건과 방법

- 동일한 State Field와 8-neighbor 조건.
- 동일한 SRProfile과 입력 이벤트.
- texel 수와 활성 texel 비율 변화.
- GPU 시간, 임시 메모리, 메모리 대역폭, Barrier 비용 측정.

### 결과

미실시. 현재 설계 기본안은 **2-Pass + alpha 저장**이다.

## 실험 — Solver GeometryDrive 비용과 구현 점검

- Date: 2026-09-28
- 상태: **기능 검증 완료 / 실제 Scene FPS 개선은 미확정**
- 관련: Simulation Optimization, Optional Curvature Transfer Weight

> **파라미터 표현과 기준값 변경:** 아래 Rate 수치는 측정 당시의 전달량 계수다. Profile/GPU 레코드는 `[0,1]` TransferFactor를 저장한다. 초기 기준 Geometry Rate 100에서는 Rate `50`, `1`이 Factor `0.5`, `0.01`에 대응했다 ([[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008]]). 현재 기준값은 6000으로 재보정되어 Factor `0.5`의 Rate는 3000이다 ([[0012_Geometry-Rate-Recalibration|Decision 0012]]). 아래 과거 측정 결과는 새 기준값으로 재측정한 결과가 아니다.

### RawFlux 방향별 캐시 적용 전 확인한 비용

- Pass 1은 최대 8개 outgoing RawFlux를, Pass 2는 최대 8개 incoming RawFlux를 texel·채널마다 계산한다. Pass 2는 outgoing 합계를 캐시에서 재사용하지만 incoming RawFlux는 재평가한다. 따라서 두 pass 모두 같은 비싼 GeometryDrive 경로를 실행한다.
- 기존 경로는 RawFlux마다 두 height의 world 변환, direction용 두 world 변환과 source normal inverse-transpose를 반복한다. TransferWeight cache와 RawOutgoing 저장은 이미 구현되어 있으므로 캐시 부재를 현재 병목으로 설명하지 않는다.
- 기본 Simulation resolution은 Surface당 512×512이며 invalid texel 슬롯도 dispatch된다. 작업량은 instance·Surface·Registry 채널 수·유효 이웃 수에 따라 늘어난다.
- 선형 instance transform 또는 weight toggle이 cache를 dirty로 만들면 CPU 전체 cache rebuild와 graphics queue idle이 발생한다. 고정 transform에서 매 프레임 발생하는 비용은 아니다.
- 기존 pass 시작 timestamp는 TOP_OF_PIPE, 끝은 COMPUTE_SHADER였으므로 앞선 작업 대기가 포함될 수 있다. 현재 시작·끝을 COMPUTE_SHADER로 맞춘다. timestamp는 driver가 더 늦은 stage에 latch할 수 있어 순수 shader instruction 시간으로 해석하지 않는다.

### 최종 변경

각 compute invocation은 한 texel을 처리한다. GeometryDrive가 켜져 있으면 이웃 간선과 State channel을 계산하기 전에 `initializeGeometryDrive()`가 두 값을 준비한다. `SolverUp = normalize(-GravityWorld)`는 월드 중력에 반대인 높이 축이고, `SolverNormalMatrix = transpose(inverse(mat3(ModelMatrix)))`는 mesh-local normal을 world normal로 바꾸는 행렬이다. 두 값은 해당 invocation의 모든 이웃 간선과 channel에서 재사용한다. 따라서 역행렬과 중력 정규화는 간선·channel마다 반복하지 않지만, 여전히 dispatch의 각 invocation에서 계산한다. Pass 1은 valid texel에서 계산을 시작할 때 준비하고, Pass 2는 실제 incoming flux를 처음 평가할 때까지 준비를 미룬다.

방향성 flux를 계산할 source texel `i`와 target texel `j`마다 Virtual Height를 반영한 mesh-local endpoint를 만든다. 여기서 높이 displacement는 현재 구현처럼 각 endpoint의 macro normal 방향으로 적용된다.

```text
Q_i = Position_i + MacroNormal_i × MesoVirtualHeight_i
Q_j = Position_j + MacroNormal_j × MesoVirtualHeight_j
DeltaWorld_ij = mat3(ModelMatrix) × (Q_j - Q_i)
```

`Q_j - Q_i`는 벡터이므로 instance translation은 적용할 필요가 없다. 두 world 위치를 따로 변환해 빼는 것과 결과가 같고, translation은 차이에서 상쇄된다. 한 번 얻은 `DeltaWorld_ij`를 두 항에 함께 쓴다.

```text
HeightDrive    = abs(dot(DeltaWorld_ij, SolverUp))
DirectionDrive = clamp(dot(normalize(GravityOnSurface_i),
                            normalize(DeltaWorld_ij)), 0, 1)
GeometryDrive  = HeightDrive × DirectionDrive
```

`HeightDrive`는 endpoint 사이의 월드 높이 차이 크기를 나타낸다. `abs`를 쓰므로 위·아래 부호는 여기서 구분하지 않는다. `DirectionDrive`가 source 표면에 투영한 중력이 이웃 방향과 얼마나 같은지를 평가하고, 반대 방향의 이동은 0으로 제한한다.

`GravityOnSurface_i`는 `GravityWorld`에서 source normal 방향 성분을 제거해 구한다. 기본 경로의 source normal은 이미 업로드된 `MesoNormal`(set 0, binding 17)을 mesh-local에서 읽어 `SolverNormalMatrix`로 world space에 변환한 값이다. 이 리소스는 tangent-space Normal Map의 원시 픽셀이 아니다. 전처리가 Macro normal로 tangent frame을 세우고 Virtual Height의 국소 기울기를 결합해 만든 mesh-local 복원 normal이므로 Macro 방향이 포함된다. 따라서 DirectionDrive가 macro normal 대신 이를 쓰면 Virtual Meso Geometry의 요철까지 반영한 표면 방향을 사용한다. 비교용 `DirectionDrive: MesoNormal` 설정을 끄면 macro normal을 사용한다. 유효 복원 normal이 없는 texel은 pack 단계에서 sampled `TransferNormal`, macro normal 순으로 대체한다.

요약하면 높이 비교와 이웃 이동 방향은 같은 displaced endpoint 차이에서 나오며, 표면에서 중력이 향하는 방향은 source의 복원 normal로 결정한다. 위치/방향 벡터에는 model transform의 선형 부분을 쓰고, normal에는 inverse-transpose를 쓰는 이유가 서로 다르다.

여러 채널의 GeometryDrive를 간선 배열에 준비해 재사용하는 후보는 4채널의 일부 표본에서 개선됐으나 1채널에서 안정적인 이득을 확인하지 못해 최종 변경에서 제외했다.

### 합성 GPU 측정

Apple M1, Vulkan compute queue, 256×256의 모든 texel 유효 grid, 최대 8-neighbor, 1/4채널, 동일 Profile, 추가 이벤트 없음. local 위치는 `(0.01x, 0.01y, 0.0025x)`, normal은 `normalize(-0.25, 0, 1)`, identity instance transform, world gravity `(0,0,-1)`이다. Normal Map 없이 MesoNormal은 macro fallback이므로 normal 연결 수정에 따른 물리 결과 차이는 없다.

Capacity 1, SaturationTransferRate 0.5, GeometryTransferRate 1, DecayRate 0.01, dt=1/60. 초기 State는 0.25~0.75의 반복 패턴이며 각 dispatch는 같은 State A를 읽고 State B에 쓴다. cache rebuild와 buffer upload는 측정 밖이다. 5회 warmup 뒤 30회 표본의 pass별 중앙값을 기록했다. baseline은 변경 전 HEAD solver shader이며 CPU timestamp stage는 baseline/최종 모두 COMPUTE_SHADER로 같게 했다. 같은 compiler 기본 옵션으로 SPIR-V를 생성했다.

| 채널 | GeometryDrive | 이전 Pass 1 / Pass 2 (ms) | 최종 Pass 1 / Pass 2 (ms) |
|---|---|---|---|
| 1 | ON | 4.891 / 0.826 | 4.885 / 0.800 |
| 1 | OFF | 4.127 / 0.376 | 4.221 / 0.412 |
| 4 | ON | 8.882 / 10.601 | 7.692 / 8.766 |
| 4 | OFF | 6.287 / 2.060 | 6.356 / 1.961 |

이는 한 측정 run이며 반복 run 간 시간 편차가 크다. 1채널의 개선을 입증하지 못했고, 4채널 표본도 실제 Scene FPS 개선 배수로 사용하지 않는다. ON/OFF 차이는 GeometryDrive가 비용을 추가한다는 관찰을 제공하지만 ALU/메모리/driver 점유율은 이 실험만으로 분리할 수 없다. 초기 실험은 wetness 1채널·accumulationFactor=0인 DemoStone.SRProfile을 사용했다. 실제 Scene의 고정 카메라·동일 State와 GPU 부하 조건에서 별도 검증이 필요하다.

### 구현 상태 점검

- Next State의 event Input, saturation/geometry transport, alpha 보유량 제한, Decay/ConcavityRetention, Capacity clamp, InputDelta 소비 및 A/B swap은 구현되어 있다.
- HeightDrive와 DirectionDrive는 같은 공통 함수로 양 pass에 적용한다. Virtual Height와 MesoNormal은 연결되어 있으며 non-uniform instance scale GPU 검증을 추가했다.
- Virtual Height에서 유도한 mean/Gaussian curvature는 전처리된다. 독립 Macro curvature field는 없으며 Gaussian curvature는 Transport에 연결하지 않는다.
- AccumulationFactor/CavityFillFactor는 Profile/GPU record에 존재하지만 SurfaceDynamicGeometryUpdate.comp와 SurfaceDynamicWeightsUpdate.comp는 placeholder다. Cavity Filling, Surface Following, AccumulationHeight, 적층 후 normal/distance/curvature 갱신은 미구현이다.
- Curvature UI는 OFF=1.0, ON=Virtual Height에서 유도한 mean curvature 기반 cache 감쇠이며 기본 OFF다. 물리 응집·응결 구현의 완성을 뜻하지 않는다.

### 검증

CMake 전체 build 및 CTest의 5개 테스트를 실행한다. 새 검증은 Virtual Height/normal 기반 GPU 전달과 비균일 scale, mass conservation, curvature 기본값·평탄·오목/볼록 대칭·scale 독립성·NaN 차단을 포함한다. 실제 UI 클릭과 실제 Scene FPS는 이 실험에서 검증하지 않았다.

### 30fps 데모 후속 점검

데모에서 약 30fps라는 실행 관측이 추가되었다. 실제 Scene의 Solver/Render GPU 시간과 Pause 전후 FPS는 아직 확보하지 못했다. 코드상 고정 30fps 제한은 없고 PresentMode는 IMMEDIATE → MAILBOX → FIFO 순으로 선택한다. 실제 선택된 mode와 화면 주사율은 이 기록에서 확인되지 않았다. MDSS 프로세스는 실행 중이지만 UI 자동화가 해당 native 실행 파일을 앱으로 인식하지 못해 직접 성능 표시를 읽지 못했다. 합성 측정 당시 다른 GPU workload가 없었는지도 보장되지 않아 이전 시간 편차를 고려해야 한다.

Pass 2는 source Current State가 0 이하, alpha가 0 이하, 또는 간선 가중치가 0 이하이면 incoming rawFlux 평가를 생략한다. inverse-transpose 준비도 첫 실제 incoming 평가까지 지연한다. Pass 1의 RawOutgoing/alpha scratch 값과 최종 갱신 식은 유지한다. 빈 source의 입력 이벤트가 소실되지 않고 다음 step부터 전달되며 InputDelta가 한 번 소비되는 GPU regression을 추가했다. 전체 build 및 CTest 5개가 통과했다. 실제 Scene FPS 개선은 아직 측정되지 않았다.

후속 비교는 동일 카메라·해상도·State에서 Pause/Run의 frame time, pass별 GPU 시간, Render GPU를 기록한다. Simulation resolution 512→256은 Surface당 texel 수를 262,144→65,536으로 줄이지만 공간 정밀도와 현행 전달 이산화 결과에 영향을 준다. 기본 해상도를 조용히 변경하지 않는다. 512를 유지하는 후보는 Pass 1의 방향별·채널별 rawFlux 저장과 Pass 2의 역방향 인덱스 gather이며 추가 buffer·대역폭 비용을 별도 측정해야 한다.

### 방향별 RawFlux 캐시 후속 구현

Directional RawFlux Cache에서 마지막 후보를 채택했다. Pass 1의 방향·채널별 RawFlux를 저장하고 Pass 2는 공유 역방향 인덱스으로 gather한다. 기존의 Pass 2 RawFlux·GeometryDrive 재평가는 제거했다. 위 측정과 빈 source 최적화 설명은 방향별 캐시 적용 이전 기록이다. 새 측정과 메모리 가정은 Decision 0021에 기록한다.

## 실험 — Pass 1 비용 분리

- Date: 2026-09-28
- 상태: **원인 분리 및 Decision 0022 적용 완료 · 합성 성능/GPU 회귀 검증 완료**
- 관련: Directional RawFlux Cache, Simulation Optimization

> **파라미터 표현과 기준값 변경:** 아래 Rate 수치는 측정 당시의 전달량 계수다. Profile/GPU 레코드는 `[0,1]` TransferFactor를 저장한다. 초기 기준 Geometry Rate 100에서는 Rate `50`, `1`이 Factor `0.5`, `0.01`에 대응했다 ([[05_Decisions/0008_Normalized-Transport-Factors|Decision 0008]]). 현재 기준값은 6000으로 재보정되어 Factor `0.5`의 Rate는 3000이다 ([[0012_Geometry-Rate-Recalibration|Decision 0012]]). 아래 과거 측정 결과는 새 기준값으로 재측정한 결과가 아니다.

### 실행 화면 관측과 집계

기본 Cube Wetness Scene의 관측은 Frame 13.4 FPS / 74.50 ms, Render GPU 0.38 ms, Solver GPU 74.01 ms, Pass 1 62.61 ms, Pass 2 11.40 ms다. Pass 1은 Solver 표시 시간의 약 84.6%다.

`TRenderer::RenderFrame`은 모든 instance의 Pass 1 timestamp 구간을 합산한다. UI는 1초 동안 이 값을 평균한다. 따라서 62.61 ms는 단일 dispatch나 큐브 한 개의 값이 아니며, 4개 cube instance의 합계다. 균등 분할하면 cube당 평균 15.65 ms이고 각 cube 비용이 같다는 뜻은 아니다. 이전 6×512×512·1채널 합성 측정과 이 화면을 같은 표본으로 비교하지 않는다.

한 장의 화면으로 순간 급등이나 frame별 jitter를 판정하지 않는다. 지속적인 Pass 1의 높은 비용을 설명하는 코드와 합성 분리 결과를 아래에 기록한다.

### 확인한 작업량 — Decision 0022 적용 전

6 Surface × 512×512 × 4 instances = 6,291,456 texel invocation/step이며 Registry channel은 DemoWetness의 1개다. 유효 이웃이 모두 8개인 소스 수준 상한은 50,331,648 directed rawFlux 평가/step이다. 실제 이웃 수·지원 여부·가중치에 따라 더 작다. 화면에 보이지 않는 표면도 Solver 대상이다.

- Pass 1에는 State=0 또는 감쇠 후 가용량=0인 source의 비싼 flux 평가를 생략하는 경로가 없다. Current와 AvailableState는 이웃별 RawFlux를 다 합친 뒤에 계산한다.
- `initializeGeometryDrive`는 각 유효 texel invocation에서 instance 공통 inverse-transpose와 gravity up 축을 준비한다. 실제 기계 명령 수는 compiler 최적화에 따라 달라진다.
- `geometryDrive(source,target)`는 이웃마다 source 법선 읽기·행렬 변환·길이·정규화, source 중력 투영·길이, source displaced position을 다시 계산한다. source가 같으면 이 값들은 이웃마다 동일하다. target 위치·edge 길이·방향·높이 차이는 이웃별 계산이 필요하다.
- Decision 0021은 Pass 2의 RawFlux 재평가를 없앴다. 위 Pass 1 계산은 유지되며 RawFlux 저장 쓰기가 추가됐다.
- RawFlux 쓰기는 instance별 float32, direction-index-major plane, 6×512×512·1채널·8개 방향 이웃, scalar 원소 padding 없음에서 instance당 48 MiB/step이다. 4 instances는 192 MiB/step의 payload store다. allocator padding 및 다른 buffer 접근은 제외하며 실제 DRAM transaction 양과 동일시하지 않는다.
- 측정 당시 DemoWetness의 GeometryTransferRate=50은 곱셈 계수로, 그 값만으로 한 dispatch의 이웃 반복 횟수가 늘지 않았다. 현재 Auto substepping ON에서는 Rate 증가가 안전 step 간격을 줄여 frame당 Solver 반복 수를 늘릴 수 있다. 기본 Auto OFF·Fixed ON의 dt=1/60초는 계수로 바뀌지 않는다 ([[0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]]).

### 합성 분리 측정

Apple M1, 한 instance에 6개 Surface × 512×512, 1채널, 모든 texel 유효 grid, 최대 8개 이웃이다. 실제 Cube Mesh·Normal Map·Render는 포함하지 않는다. 위치 `(0.01x,0.01y,0.0025x)`, normal `normalize(-0.25,0,1)`, identity transform, gravity `(0,0,-1)`, dt=1/60이다. Profile은 Decision 0021과 같은 DemoWetness 수치를 사용하며 Input=0, ConcavityWeight=0이다.

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

### source 중복 계산을 제거한 임시 후보

분석용 shader에서 source 법선 변환·정규화, 중력 투영과 길이, displaced source position을 invocation당 한 번 준비해 이웃 평가가 재사용하도록 했다. source inverse-transpose 준비와 이웃별 target/edge 계산, RawFlux 쓰기, Profile/State 규칙은 유지했다. 추가 GPU buffer는 없다. 현행 runtime shader에 적용한 변경은 아니다.

별도 두 반복에서 source 재사용 후보와 현행을 번갈아 측정했으며 모든 배열의 Next State·RawOutgoing·alpha를 비교했다.

| 조건 | 현행 Pass 1 (ms) | source 재사용 후보 (ms) | 최대 절대 오차 |
|---|---:|---:|---:|
| 전체 State / run 1 | 25.198 | 15.621 | 5.96e-8 |
| 전체 State / run 2 | 19.082 | 12.993 | 5.96e-8 |
| dry / run 1 | 13.808 | 12.813 | 1.40e-9 |
| dry / run 2 | 14.106 | 13.262 | 1.40e-9 |

임시 후보 shader로 기존 Surface GPU resource/solver 회귀 검사도 통과했다. Virtual Height·normal, 비균일 scale, 중력 반전, term toggle, 여러 channel·seam direction index·초과량 보존·입력 소비 fixture를 포함하며 validation 오류가 없었다.

source 재사용의 개선은 전체 State 조건에서 약 32–38%, dry에서 약 6–7%다. 시간 편차와 위 다른 실험 run을 섞지 않으며 실제 Scene 성능 개선률로 환산하지 않는다. 여러 Profile/channel의 GeometryTransferRate=0 경로에서는 불필요한 source 준비를 피하도록 실제 적용 시 준비 시점을 검토한다.

### 가용량 0의 source 생략 범위

생략 후보는 텍셀 전체 갱신이 아니라 Pass 1의 **해당 texel·channel에서 나가는 전달량 평가**다. `AvailableState = max(Current - Decay, 0)`이고 실제 outgoing은 `RawOutgoing × alpha`이므로, 가용량이 0이면 계산 전에도 실제 outgoing이 0임을 알 수 있다. 이 판단에는 별도 활동 비트 마스크가 필요하지 않다.

Pass 2는 계속 이웃 source의 캐시를 gather하고 InputDelta를 소비해 `Next = max(Current + Input + Incoming - Outgoing - Decay, 0)`를 기록한다. 자기 가용량이 0이어도 이웃 유입이나 외부 입력을 받을 수 있다. 이번 step에 받은 양은 다음 step의 Current가 되므로 다음 step부터 outgoing 평가 대상이다. 여러 Registry channel 중 하나의 가용량이 0인 경우에도 다른 channel은 독립적으로 처리한다.

생략 경로에서도 재사용 scratch의 이전 값이 남지 않도록 해당 channel의 RawFlux의 8개 방향별 항목과 RawOutgoing·alpha를 0으로 덮어써야 한다. 이는 Next State와 실제 전달량을 보존하지만, 가용량이 0일 때의 제한 전 RawOutgoing과 alpha 디버그 값은 초기 구현과 달라질 수 있다. 후속 Decision 0022에서 이 경로를 runtime shader에 적용했다.

### 추가 중복 연산 검토 — Decision 0022 적용 전

아래 횟수는 유효 이웃 8개인 소스 코드의 평가 상한이다. GPU compiler가 일부 계산과 읽기를 합칠 수 있으며, source 형상 재사용 임시 후보 외의 각 항목은 별도 성능 측정을 하지 않았다.

| 항목 | 현행 반복 범위 | 재사용 가능한 범위와 적용 제한 |
|---|---|---|
| source Saturation `Current / Capacity` | SaturationDrive가 켜져 있으면 이웃당 한 번, 최대 8회/channel | source·channel당 한 번. target의 Saturation은 이웃마다 다르다. |
| source 지원 검사·profile index·profile parameters | Pass 1 외부 지원 검사 이후에도 각 `rawFlux`에서 source 지원 검사를 반복하고 profile을 다시 조회한다. source Capacity도 Saturation 함수에서 재조회한다. | 이미 지원을 확인한 source·channel의 Parameters와 Current를 이웃 루프 밖에서 준비한다. target의 지원 여부와 Capacity 검사는 유지한다. |
| instance normal matrix·gravity up 축 | 각 유효 texel invocation의 `initializeGeometryDrive` | transform·gravity가 같은 instance의 dispatch 공통 값이다. CPU 또는 instance 공통 자원에서 준비하는 후보이며 push constant/layout 변경 비용을 함께 검토한다. 이 행렬은 이웃당 8회가 아니라 현행에서도 texel당 한 번 준비한다. |
| source projected gravity의 방향 정규화·공통 유효성 검사 | `length(SolverUp)`와 source 방향 `GravityOnSurface / SurfaceGravityLength`가 `geometryDrive`마다 평가된다. | source 형상 준비에 유효 여부와 정규화된 방향을 포함해 이웃들이 재사용한다. 기존 임시 후보도 source 방향 나눗셈은 이웃 평가 안에 남아 있다. target edge 길이·정규화는 이웃별로 필요하다. |
| 같은 edge의 GeometryDrive·neighbor index·TransferWeight | channel 루프 안에서 이웃 루프를 반복하므로 여러 channel에서 동일 값을 재평가/재조회한다. | 형상·topology·가중치는 channel 독립이다. 여러 channel에서 재사용할 수 있으나 현재 1채널 wetness 데모에는 channel 간 절감이 없다. edge 배열의 register 압력과 1채널 성능을 검증해야 하며 기존 배열 후보는 안정적인 개선이 확인되지 않아 미채택이다. |
| 동일 texel·channel의 `decayAmount` | Pass 1에서 가용량 계산, Pass 2에서 Next 계산으로 두 번 | 두 pass가 같은 Current·Profile·형상·dt·flags를 읽는다. 재사용하려면 pass 간 저장과 읽기가 필요하므로 단순 루프 이동과 다르다. 추가 메모리 접근 대비 이득을 측정하기 전에는 우선순위를 낮춘다. |

이웃별 target 위치·Saturation과 source→target 방향은 실제로 다르므로 source와 같은 값으로 취급하지 않는다. 반대 방향 `rawFlux(j,i)`도 Saturation 차이·source 계수·중력 방향에 따라 달라지므로 `rawFlux(i,j)`를 그대로 재사용할 수 없다.

### 후속 우선순위

아래는 원인 분리 시점의 우선순위다. 1–4의 가용량 0 생략·source 공통 형상·instance 공통 행렬·source channel 공통 조회는 Decision 0022에서 적용했다. 여러 channel의 edge 배열과 pass 간 Decay 캐시는 보류하며 실제 Scene 시계열은 별도 과제다. 적용 후 동일 512 해상도의 합성 비교 결과는 Pass1 Source Reuse에 기록했다. 기본 해상도는 후속 Decision 0023에서 Medium 256으로 변경했으므로 최초 화면 관측과 기본 실행 시간을 직접 비교하지 않는다.

1. 가용 State=0인 source의 실제 outgoing은 source alpha 제한으로 0이다. Pass 1의 형상·Saturation 계산을 생략하는 경로를 검토한다. Input은 기존대로 Pass 2에 반영하므로 새 입력의 전달 시점은 다음 step이다. 생략 시 RawFlux·RawOutgoing·alpha scratch의 0 기록 의미와 디버그 계약을 함께 정의해야 한다.
2. 위 source 공통 geometry 재사용을 검토한다. channel 수가 달라도 source geometry는 동일하며, GeometryDrive가 실제 필요한 경우에 한 번 준비한다.
3. instance 공통 inverse-transpose와 gravity up 축을 instance 단위로 준비해 invocation당 반복을 줄이는 방안을 검토한다. 비용 절감 폭은 따로 측정한다.
4. source·channel 공통 Saturation·Profile 조회·지원 검사를 이웃 루프 밖으로 옮기는 후보를 검토한다. 여러 channel의 edge 재사용과 pass 간 Decay 재사용은 별도 측정 후 판단한다.
5. 실제 Scene의 네 instance별 Pass 1/2와 frame별 timestamp를 수집해 Virtual Meso Geometry·회전·State 분포·시간 편차를 분리한다. 이 기록은 상시 높은 Pass 1의 원인을 분석했으며 순간 급등 원인은 별도 시계열이 필요하다.

## RawFlux 캐시 ON/OFF 비교 실험

- Date: 2026-09-28
- Status: **초기 ON/OFF 경로의 합성 GPU 비교 완료 · Decision 0005 이전 측정 기록**
- 적용 범위: 아래 수치는 빈 source의 RawFlux를 0으로 덮어쓰던 초기 ON 구현이다. [[0005_Inactive-RawFlux-Write-Elision|Decision 0005]] 이후에는 해당 쓰기를 생략하므로 현재 성능 수치로 사용하지 않는다.
- 결정: RawFlux 캐시 ON/OFF 비교

### 비교 조건

- Apple M1, Vulkan/MoltenVK, 6 Surface × 512×512 = 1,572,864 texel, Registry 1채널, 1 instance.
- 같은 GPU 리소스와 Profile을 사용하며 ON/OFF 각각 specialization pipeline을 선택한다. source 재사용, 빈 source 생략, CPU instance 행렬 계산, 다른 Solver 항목은 양쪽에 유지한다. 메모리 할당은 바꾸지 않는다.
- 각 Surface의 texel `(x,y)` 위치는 `(0.01x, 0.01y, 0.0025x)`, 법선은 `normalize(-0.25,0,1)`이다. 8방향 grid 이웃이며 경계 밖 이웃은 invalid다. 모든 texel은 같은 Profile을 지원하며 Normal Map, 렌더링, 이벤트 입력은 제외한다.
- Capacity=1, saturation rate=0.2, geometry rate=50, decay rate=0.03, cavity retention factor=0.5, instance transform=identity, gravity=(0,0,-1), dt=1/60초.
- dense는 모든 source의 State가 `0.25 + 0.5 × (global index % 31)/30`이다. sparse는 `global index % 1024 < 16`에만 같은 양수를 넣고 나머지는 0이다. dry는 전체 0이다.
- 각 모드의 측정 전 동일한 initial State를 A에 업로드한다. 모든 dispatch는 같은 A를 읽고 B에 기록하여 시뮬레이션 진행에 따른 조건 변화를 제거한다. 5회 warmup 후 30회 GPU timestamp의 중앙값을 구한다. 두 번째 반복에서는 OFF→ON으로 순서를 뒤집는다.
- CPU 준비·업로드·pipeline 생성·download는 시간 측정 밖이다. 전체 구간은 Pass 1 시작부터 Pass 2 종료까지이며 중간 barrier 구간을 포함한다. UI의 Solver GPU 값은 두 pass 구간 합을 평균하므로 이 전체 중앙값과 집계 방식이 다르다.
- 비교 fixture의 버퍼 payload는 RawFlux float32 `1,572,864 × 1 × 8 × 4 B` = 48 MiB, 공유 reverse direction indices uint32 `1,572,864 × 4 B` = 6 MiB로 합계 54 MiB다. 두 배열은 원소당 4 B이며 추가 원소 padding이 없다. ON/OFF 모두 동일하게 할당한다. allocator overhead, pipeline 메모리 및 기존 State·형상 버퍼는 제외한다. 실제 Scene의 instance·공유 조합 수와 구분한다.

### 결과

시간 단위는 ms이며 ON/OFF 열은 각각 반복 1 / 반복 2다.

| State 분포 | ON Pass 1 | OFF Pass 1 | ON Pass 2 | OFF Pass 2 | ON 전체 | OFF 전체 |
|---|---|---|---|---|---|---|
| 전체 양수 | 8.937 / 9.255 | 11.708 / 8.758 | 3.308 / 3.404 | 13.878 / 13.400 | 12.321 / 12.615 | 25.535 / 22.290 |
| 약 1.56% 양수 | 1.952 / 1.978 | 1.188 / 1.105 | 2.170 / 2.195 | 2.438 / 2.336 | 4.181 / 4.241 | 3.675 / 3.592 |
| 전체 0 | 2.101 / 1.850 | 1.198 / 1.043 | 2.528 / 2.123 | 2.714 / 2.194 | 4.494 / 4.007 | 3.901 / 3.303 |

- 전체 양수에서는 ON의 전체 GPU 구간이 OFF보다 약 43–52% 짧았다. Pass 2의 방향별 geometry·saturation 재평가를 없애는 이득이 크다.
- 약 1.56% 양수에서는 ON의 전체 구간이 OFF보다 약 14–18% 길었다. 빈 source의 계산은 이미 생략되므로 재계산 제거 이득은 작고 ON의 방향별 cache zero store 비용은 남는다.
- 전체 0에서는 ON의 전체 구간이 OFF보다 약 15–21% 길었다. 두 모드 모두 incoming 재평가가 없어 Pass 1의 cache zero store 제거가 OFF에 유리했다.
- 모든 결과 배열 Next·RawOutgoing·alpha의 ON/OFF 최대 절대 차이는 전체 양수·sparse에서 5.96×10⁻⁸ 이하, dry에서 0이었다. specialization에 따른 부동소수점 반올림 수준이다.
- GPU clock·열 상태와 compiler 최적화에 따른 반복 편차가 있다. 캐시 사용이 항상 빠르다는 결론이나 실제 Cube Scene FPS 개선률로 환산하지 않는다. 반복 1의 dense Pass 1 편차도 유지하여 기록했다.

### 실제 Scene에서 비교

1. Solver 탭의 Cache Comparison을 펼쳐 RawFlux Cache를 선택한다. 이 실험 당시 기본값은 ON이었으며 현재 기본값은 OFF다.
2. 동일한 해상도·다른 Solver 항목·Profile·transform을 유지하고 맨 오른쪽 Global Settings 탭에서 Fixed timestep ON·Auto substepping OFF를 사용한다. 양쪽에서 같은 Time scale을 사용한다. Solver step당 1/60초이며 배속은 누적 시간에 적용된다. frame당 Solver 반복 수와 backlog도 함께 기록한다.
3. 각 모드에서 Reset State 후 같은 입력을 재현한다. 같은 초기 State 없이 실행 중 토글한 숫자는 동등한 조건의 A/B 측정으로 해석하지 않는다. 입력 위치·강도·횟수와 경과 step을 맞춰야 한다.
4. warmup과 전환 직후 첫 평균을 지나서 Pass 1·Pass 2·Solver GPU를 읽는다. 화면의 모드 표시와 cache buffer MiB를 함께 기록한다. OFF에서도 할당은 유지되며 추가 VRAM 절감은 발생하지 않는다.

UI는 토글·고정 시간 간격·실제 buffer 크기·모드별 평균 초기화를 제공한다. 자동 State snapshot/replay와 정해진 step 수의 benchmark 실행은 현재 제공하지 않는다.

## 2026-10-05 셰이더 핫패스 최적화 기록

- 작업일: 2026-10-05 (KST)
- 범위: Solver Pass 2, stencil State 샘플링, 동적 concavity, surface lighting
- 상태: 구현 및 셰이더 컴파일 확인 완료. Shared-memory 타일 및 렌더링용 State texture는 후속 검토 항목이다.

### 반영한 변경

#### Solver Pass 2

- Pass 2는 이웃 flux를 모으기 전에 현재 texel/channel이 지원되는지 이미 확인한다. OFF 재계산 경로는 이 결과를 전제로 하여 이웃별 `rawFlux` 안의 중복 `supportsChannel(target)` 확인을 없앴다.
- 현재 texel의 포화도, concavity, effective position은 모든 이웃 flux가 공통으로 참조하므로 이웃 loop 전에 한 번 계산해 재평가 함수에 넘긴다. 이웃 source의 Profile, saturation, transfer weight와 source geometry는 방향마다 달라지므로 source 측 계산은 계속 이웃별로 한다.
- texel의 8개 packed reverse-direction index를 나타내는 `uint`를 loop 전에 한 번 읽고, loop 안에서는 shift/mask만 수행한다.
- RawFlux cache ON 분기에서는 이 값들이 재계산 경로에서만 쓰이므로 specialization된 cache 파이프라인에서 재계산 경로를 분리할 수 있다.

```text
기존 OFF: 이웃마다 target 지원 검사 + target saturation/concavity/position 조회 + reverse index word 읽기
현재 OFF: 앞선 target channel guard 재사용 + target 공통값 한 번 준비 + reverse index word 한 번 읽기
ON:       Pass 1의 방향별 RawFlux를 gather; OFF 재평가 전용 값은 필요하지 않음
```

각 방향의 source flux 수식과 Pass 1/Pass 2 역할, Current/Next State 계산은 유지한다. 재사용 대상은 모든 incoming edge에서 같은 target 항목이고, 서로 다른 source가 요구하는 값은 합치지 않는다.

관련 구현: [SurfaceSolverPass2.comp](../../../Shaders/Simulation/SurfaceSolver/SurfaceSolverPass2.comp), [SurfaceSolverCommon.glsl](../../../Shaders/Simulation/SurfaceSolver/SurfaceSolverCommon.glsl)

#### Stencil State 샘플링

- `StateSaturationForProfile`을 추가했다. 일반 `StateSaturation`은 texel→Surface/Profile 대응, Profile/channel 지원, buffer 길이를 검증한 뒤 이 helper를 호출한다. helper는 Profile index를 받아 capacity와 총량만 읽고 면적 환산·유효성·`[0,1]` clamp를 수행한다.
- `SampleStateSaturation`은 center에서 Surface, Profile, chart와 channel 지원을 확인하고, 2×2 bilinear 이웃에 대해 chart/Profile 동일 여부를 검사한 뒤 helper를 쓴다. `HeightFieldSmoothing`도 center 지원 channel 검사 이후 같은 Profile인 이웃에 helper를 사용한다. `OverlayCoverageSmoothing`은 중심 Profile의 지원 여부를 loop 전에 한 번 확인하고, 최대 5×5 이웃에서 같은 Surface/Profile/chart texel만 처리한다.
- 이 검사 순서로 이웃마다 중복되던 Surface/Profile/support table 조회를 줄인다. 이웃의 면적·State 값은 달라질 수 있으므로 texel별 capacity·amount read는 유지한다. 기존 kernel weight, 면적 보정과 출력 clamp는 변경하지 않았다.

관련 구현: [StateSampling.glsl](../../../Shaders/Rendering/Surface/StateSampling.glsl), [HeightFieldSmoothing.comp](../../../Shaders/Rendering/Surface/HeightFieldSmoothing.comp), [OverlayCoverageSmoothing.comp](../../../Shaders/Rendering/StateOverlay/OverlayCoverageSmoothing.comp)

#### 동적 concavity와 조명

- `SurfaceDynamicGeometryUpdate`는 동적 geometry의 두 번째 `vec4.xyz`에 `normalize(Normal)`을 저장한다. `SurfaceDynamicWeightsUpdate`의 concavity fit은 이 local 값을 그대로 사용해 중심 및 최대 8개 이웃의 불필요한 `normalize` 호출을 제거한다. `worldNormal`은 NormalMatrix 변환이 길이를 바꿀 수 있어 변환 후 정규화를 유지한다. 정적 fallback Meso normal도 builder에서 정규화된 값이라는 데이터 계약을 따른다.
- Lit 경로는 `V`, `L`, `H`와 `NoV`, `NoL`, `NoH`, `VoH`를 base direct-light 계산에 사용한다. Wet/film `EvaluateSpecularLobe`가 이 공통 항을 입력으로 받게 해 lobe마다 수행하던 vector normalization과 dot product를 제거하고, 각 roughness에 필요한 GGX distribution·visibility·Fresnel은 유지한다.
- `Wet == 0` 또는 Wetness specular strength가 0이면 Wet lobe를, `Film == 0`이면 Film lobe를 생략한다. 색상·coverage·roughness 값 자체는 기존 식과 같다.

관련 구현: [SurfaceDynamicWeightsUpdate.comp](../../../Shaders/Simulation/SurfaceDynamicWeightsUpdate.comp), [Lighting.glsl](../../../Shaders/Rendering/Surface/Lighting.glsl)

### 캐시 비교 UI

RawFlux Cache ON/OFF는 이번 셰이더 변경으로 새로 도입한 UI가 아니다. 기존 Debug UI의 `Cache Comparison > RawFlux Cache` 체크박스와 specialization pipeline 전환을 확인했다. OFF는 Pass 2에서 flux를 재계산하고, ON은 Pass 1의 방향별 flux를 재사용한다. 비교 옵션은 유지한다.

관련 구현: `Source/DebugUI/DebugUI.cpp`, `Source/SurfaceState/State/SurfaceStateSolver.cpp`; Directional RawFlux Cache

### 검증 및 성능 기록

- 수정한 Pass 1/2, dynamic weights, SurfaceLit, HeightFieldSmoothing, OverlayCoverageSmoothing 셰이더를 `glslc`로 컴파일했고, 이후 Pass 2 helper signature와 Overlay 중심 지원 검증을 보완한 뒤 Pass 1/2 및 Overlay 셰이더도 다시 컴파일했다. `git diff --check`도 통과했다.
- 이 노트 작성 시점에는 테스트 suite를 실행하지 않았다.
- 2026-10-04 23:54 결과는 직전 23:20 결과와 비교해 여섯 Scene/resolution 조합 모두 Solver median이 낮았으며 감소폭은 약 39–49%였다. 다만 두 실행 모두 dirty working tree였고 revision도 다르므로, 이 수치를 이번 셰이더 변경만의 효과로 해석하지 않는다. 상세값과 실행 protocol은 [[20261004-235404-solver-resolution/summary|23:54 solver-resolution 결과]] 및 [[20261004-232011-solver-resolution/summary|23:20 직전 결과]]를 참조한다.

### 후속 검토

- `HeightFieldSmoothing`의 2D shared-memory tile은 dispatch 좌표와 경계 조건을 함께 검토하는 별도 실험으로 둔다. `local_size_x = 64`를 단독으로 `8×8`로 바꾸는 것은 적용하지 않았다.
- SurfaceLit용 State texture 변환은 추가 GPU resource, 동기화, channel packing과 정밀도 검토가 필요하다. 이번에는 SSBO 경로를 유지했다.
- Dirty height epsilon은 형상 갱신을 억제해 시뮬레이션 결과를 바꿀 수 있으므로 도입하지 않았다. epsilon은 Scene 단위 오차 기준을 정하고 별도 비교한 뒤 결정한다.
- GPU timestamp 비교를 통해 개별 변경의 효과를 분리할 수 있는 benchmark run이 필요하다.

### 관련 문서

- Simulation Optimization
- Rendering
- Directional RawFlux Cache
