# 실험 — Solver GeometryDrive 비용과 구현 점검

> **한 줄 요약:** Solver GeometryDrive 경로의 연산 비용과 현재 구현·측정 한계를 점검한다.

- Date: 2026-09-28
- 상태: **기능 검증 완료 / 실제 Scene FPS 개선은 미확정**
- 관련: [[../../04_Architecture/0007_Simulation-Optimization|Simulation Optimization]], [[05_ADR/0019-Optional-Curvature-Transfer-Weight|ADR 0019]]

## RawFlux 방향별 캐시 적용 전 확인한 비용

- Pass 1은 최대 8개 outgoing RawFlux를, Pass 2는 최대 8개 incoming RawFlux를 texel·채널마다 계산한다. Pass 2는 outgoing 합계를 캐시에서 재사용하지만 incoming RawFlux는 재평가한다. 따라서 두 pass 모두 같은 비싼 GeometryDrive 경로를 실행한다.
- 기존 경로는 RawFlux마다 두 height의 world 변환, direction용 두 world 변환과 source normal inverse-transpose를 반복한다. TransferWeight cache와 RawOutgoing 저장은 이미 구현되어 있으므로 캐시 부재를 현재 병목으로 설명하지 않는다.
- 기본 Simulation resolution은 Surface당 512×512이며 invalid texel 슬롯도 dispatch된다. 작업량은 instance·Surface·Registry 채널 수·유효 이웃 수에 따라 늘어난다.
- 선형 instance transform 또는 weight toggle이 cache를 dirty로 만들면 CPU 전체 cache rebuild와 graphics queue idle이 발생한다. 고정 transform에서 매 프레임 발생하는 비용은 아니다.
- 기존 pass 시작 timestamp는 TOP_OF_PIPE, 끝은 COMPUTE_SHADER였으므로 앞선 작업 대기가 포함될 수 있다. 현재 시작·끝을 COMPUTE_SHADER로 맞춘다. timestamp는 driver가 더 늦은 stage에 latch할 수 있어 순수 shader instruction 시간으로 해석하지 않는다.

## 최종 변경

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

## 합성 GPU 측정

Apple M1, Vulkan compute queue, 256×256의 모든 texel 유효 grid, 최대 8-neighbor, 1/4채널, 동일 Profile, 추가 이벤트 없음. local 위치는 `(0.01x, 0.01y, 0.0025x)`, normal은 `normalize(-0.25, 0, 1)`, identity instance transform, world gravity `(0,0,-1)`이다. Normal Map 없이 MesoNormal은 macro fallback이므로 normal 연결 수정에 따른 물리 결과 차이는 없다.

Capacity 1, SaturationTransferRate 0.5, GeometryTransferRate 1, DecayRate 0.01, dt=1/60. 초기 State는 0.25~0.75의 반복 패턴이며 각 dispatch는 같은 State A를 읽고 State B에 쓴다. cache rebuild와 buffer upload는 측정 밖이다. 5회 warmup 뒤 30회 표본의 pass별 중앙값을 기록했다. baseline은 변경 전 HEAD solver shader이며 CPU timestamp stage는 baseline/최종 모두 COMPUTE_SHADER로 같게 했다. 같은 compiler 기본 옵션으로 SPIR-V를 생성했다.

| 채널 | GeometryDrive | 이전 Pass 1 / Pass 2 (ms) | 최종 Pass 1 / Pass 2 (ms) |
|---|---|---|---|
| 1 | ON | 4.891 / 0.826 | 4.885 / 0.800 |
| 1 | OFF | 4.127 / 0.376 | 4.221 / 0.412 |
| 4 | ON | 8.882 / 10.601 | 7.692 / 8.766 |
| 4 | OFF | 6.287 / 2.060 | 6.356 / 1.961 |

이는 한 측정 run이며 반복 run 간 시간 편차가 크다. 1채널의 개선을 입증하지 못했고, 4채널 표본도 실제 Scene FPS 개선 배수로 사용하지 않는다. ON/OFF 차이는 GeometryDrive가 비용을 추가한다는 관찰을 제공하지만 ALU/메모리/driver 점유율은 이 실험만으로 분리할 수 없다. 현재 DemoStone.SRProfile은 wetness 1채널이고 accumulationFactor=0이다. 실제 Scene의 고정 카메라·동일 State와 GPU 부하 조건에서 별도 검증이 필요하다.

## 구현 상태 점검

- Next State의 event Input, saturation/geometry transport, alpha 보유량 제한, Decay/ConcavityRetention, Capacity clamp, InputDelta 소비 및 A/B swap은 구현되어 있다.
- HeightDrive와 DirectionDrive는 같은 공통 함수로 양 pass에 적용한다. Virtual Height와 MesoNormal은 연결되어 있으며 non-uniform instance scale GPU 검증을 추가했다.
- Virtual Height에서 유도한 mean/Gaussian curvature는 전처리된다. 독립 Macro curvature field는 없으며 Gaussian curvature는 Transport에 연결하지 않는다.
- AccumulationFactor/CavityFillFactor는 Profile/GPU record에 존재하지만 SurfaceAccumulation.comp와 SurfaceGeometryUpdate가 placeholder다. Cavity Filling, Surface Following, AccumulationHeight, 적층 후 normal/distance/curvature 갱신은 미구현이다.
- Curvature UI는 OFF=1.0, ON=Virtual Height에서 유도한 mean curvature 기반 cache 감쇠이며 기본 OFF다. 물리 응집·응결 구현의 완성을 뜻하지 않는다.

## 검증

CMake 전체 build 및 CTest의 5개 테스트를 실행한다. 새 검증은 Virtual Height/normal 기반 GPU 전달과 비균일 scale, mass conservation, curvature 기본값·평탄·오목/볼록 대칭·scale 독립성·NaN 차단을 포함한다. 실제 UI 클릭과 실제 Scene FPS는 이 실험에서 검증하지 않았다.

## 30fps 데모 후속 점검

데모에서 약 30fps라는 실행 관측이 추가되었다. 실제 Scene의 Solver/Render GPU 시간과 Pause 전후 FPS는 아직 확보하지 못했다. 코드상 고정 30fps 제한은 없고 PresentMode는 IMMEDIATE → MAILBOX → FIFO 순으로 선택한다. 실제 선택된 mode와 화면 주사율은 이 기록에서 확인되지 않았다. MDSS 프로세스는 실행 중이지만 UI 자동화가 해당 native 실행 파일을 앱으로 인식하지 못해 직접 성능 표시를 읽지 못했다. 합성 측정 당시 다른 GPU workload가 없었는지도 보장되지 않아 이전 시간 편차를 고려해야 한다.

Pass 2는 source Current State가 0 이하, alpha가 0 이하, 또는 간선 가중치가 0 이하이면 incoming rawFlux 평가를 생략한다. inverse-transpose 준비도 첫 실제 incoming 평가까지 지연한다. Pass 1의 RawOutgoing/alpha scratch 값과 최종 갱신 식은 유지한다. 빈 source의 입력 이벤트가 소실되지 않고 다음 step부터 전달되며 InputDelta가 한 번 소비되는 GPU regression을 추가했다. 전체 build 및 CTest 5개가 통과했다. 실제 Scene FPS 개선은 아직 측정되지 않았다.

후속 비교는 동일 카메라·해상도·State에서 Pause/Run의 frame time, pass별 GPU 시간, Render GPU를 기록한다. Simulation resolution 512→256은 Surface당 texel 수를 262,144→65,536으로 줄이지만 공간 정밀도와 현행 전달 이산화 결과에 영향을 준다. 기본 해상도를 조용히 변경하지 않는다. 512를 유지하는 후보는 Pass 1의 방향별·채널별 rawFlux 저장과 Pass 2의 역방향 슬롯 gather이며 추가 buffer·대역폭 비용을 별도 측정해야 한다.

## 방향별 RawFlux 캐시 후속 구현

[[05_ADR/0021-Directional-RawFlux-Cache|ADR 0021]]에서 마지막 후보를 채택했다. Pass 1의 방향·채널별 RawFlux를 저장하고 Pass 2는 공유 역방향 슬롯으로 gather한다. 기존의 Pass 2 RawFlux·GeometryDrive 재평가는 제거했다. 위 측정과 빈 source 최적화 설명은 방향별 캐시 적용 이전 기록이다. 새 측정과 메모리 가정은 ADR 0021에 기록한다.
