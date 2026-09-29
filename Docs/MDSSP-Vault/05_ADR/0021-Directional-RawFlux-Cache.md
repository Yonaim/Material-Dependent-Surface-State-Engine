# ADR 0021 — 방향·채널별 RawFlux 캐시와 역방향 gather

> **한 줄 요약:** Pass 1에서 방향별 RawFlux를 저장하고 Pass 2에서 이웃 source의 역방향 값을 재사용한다.

- 분류: **Simulation**
- Status: **Accepted**
- Date: 2026-09-28
- 관련 문서: [[0017-Solver-Transfer-Cache|ADR 0017 — Solver Transfer Cache]], [[../04_Architecture/0008_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- 구현 상태: **구현 완료 · GPU 회귀 검증 완료 · 실제 Scene FPS 개선은 미확정**

후속 ADR 0022는 Pass 1 source 재사용과 가용량 0 생략을, ADR 0023은 해상도 프리셋과 기본 Medium 256을 추가했다. 아래 512 유지 결정과 측정은 이 ADR 채택 당시 기준이며 High 해상도에서 그대로 사용할 수 있다.

> **파라미터 표현 변경:** 아래 Rate 수치는 측정 당시의 실제 속도다. 현재 Profile/GPU 레코드는 `[0,1]` TransferFactor를 저장하며 실제 속도는 각각 기준 속도 `1.0`, `100.0`을 곱한다. 기존 Geometry Rate `50`, `1`은 Factor `0.5`, `0.01`에 대응한다. 과거 측정 결과는 재측정값이 아니다 ([[05_ADR/0029-Normalized-Transport-Factors|ADR 0029]]).

## Context

ADR 0017의 초기 캐시는 TransferWeight와 RawOutgoing 합계만 보관했다. Pass 1은 최대 8개 outgoing RawFlux를 계산하고 Pass 2는 최대 8개 incoming RawFlux를 다시 계산했다. GeometryDrive의 displaced endpoint와 source normal 계산도 Pass 2에서 반복했다. 빈 source 생략은 State가 퍼지면 효과가 줄어든다. Surface당 512×512의 해상도를 유지하며 이 재평가를 제거한다.

## Decision

1. instance별·방향별·Registry channel별 float32 `RawFluxBuffer`를 추가한다. Pass 1은 unscaled RawFlux를 저장하고 기존 RawOutgoing 합계와 source alpha를 함께 기록한다. Pass 2는 source alpha를 곱해 incoming을 구하며 rawFlux·GeometryDrive를 재계산하지 않는다.
2. scratch는 slot-major plane을 사용한다. 주소는 `slot × TexelCount × ChannelCount + texel × ChannelCount + channel`이다. 같은 방향을 처리하는 인접 invocation의 store가 연속되도록 한다. State A/B와 RawOutgoing의 기존 texel-major AoS는 유지한다.
3. Shared Geometry에 texel당 uint32 `ReverseNeighborSlots`를 추가한다. 8개 슬롯의 역방향 번호를 각 4 bit에 packed하고 0xf를 invalid로 쓴다. CPU packing은 이웃의 실제 배열에서 자신을 가리키는 슬롯을 찾는다. UV seam의 슬롯이 고정 반대 방향이라는 가정을 하지 않는다. Mapping validation의 상호 이웃 계약을 유지한다.
4. Pass 1은 invalid 이웃, unsupported channel, invalid texel까지 모든 RawFlux 슬롯을 매 step 덮어쓴다. RawFlux의 초기·reset clear를 생략하며, 다음 Pass 1 이전에 이 scratch를 소비하지 않는다.
5. AB/BA descriptor는 같은 instance RawFlux와 공유 역방향 슬롯을 참조한다. binding 18은 역방향 슬롯, 19는 RawFlux다. 전체 binding count는 20이며 기존 device limit 검사에 반영한다.
6. RawFlux를 Pass 1 write→Pass 2 read barrier 및 Pass 2 read→다음 Pass 1 write barrier에 포함한다. RawOutgoing·alpha와 입력 소비·A/B 전환은 기존 계약을 유지한다.
7. RawFlux 크기는 size overflow, shader uint32 인덱스 범위, `maxStorageBufferRange`를 확인한다. 한도를 넘으면 명시적인 오류를 내며 해상도나 채널을 자동으로 줄이지 않는다.

## Alternatives Considered

- RawOutgoing만 저장하는 기존 경로: 추가 방향별 메모리가 없지만 Pass 2 incoming RawFlux 재평가가 남는다.
- texel/channel-major RawFlux 저장: 같은 payload로 구현 가능하다. 합성 측정에서 Pass 1 비용 증가와 sparse State의 전체 시간 악화가 관측되어 slot-major plane을 채택했다.
- 매 step 이웃의 역방향 슬롯 탐색: 별도 공유 정보가 필요 없지만 최대 8개 슬롯의 추가 순회가 생긴다. topology packing 때 한 번 준비하는 packed 역방향 슬롯을 채택했다.
- 해상도 512→256 축소: texel 수를 줄이지만 공간 정밀도와 현행 이산화 결과가 달라진다. 이번 변경은 512를 유지한다.

## Consequences

- rawFlux의 소스 수준 호출 상한은 직전 구현의 16→8회/texel·channel이다. Pass 2 재평가는 0회가 된다. 실제 호출 수는 valid 이웃·지원 채널에 의존하며 GPU 시간 감소와 같은 비율이 아니다.
- 6 Surface × 512×512 = 1,572,864 texel, 8슬롯, 1 Registry channel, float32 4 B, 원소 padding 없음에서 RawFlux는 **instance당 48 MiB**가 증가한다. 역방향 슬롯은 uint32 4 B, texel당 8 × 4 bit packed이며 **Shared Geometry당 6 MiB**가 증가한다. channel 수에 비례하는 것은 RawFlux뿐이다. allocator alignment·CPU packing scratch는 제외하며 invalid 슬롯도 할당한다. 기본 Cube Wetness Scene 실행에서 4개 instance·2개 공유 Surface data variant를 확인했다. 같은 512×512·1채널 가정에서 증가분은 `4 × 48 + 2 × 6 = 204 MiB`다.
- TransferWeights·RawOutgoing를 포함한 세 cache payload는 같은 가정에서 instance당 102 MiB다. 역방향 슬롯은 같은 runtime Surface data handle을 사용하는 instance끼리 공유한다.
- 방향별 scratch write와 gather bandwidth가 추가된다. topology가 바뀌면 역방향 슬롯을 Geometry와 함께 다시 준비해야 한다. State·Profile 수치·중력·Solver flag 변경은 다음 Pass 1의 RawFlux 계산에 즉시 반영된다.
- Capacity 초과량 보존, source 보유량 제한, EventInput의 Pass 2 소비와 다음 step 전파 규칙은 유지한다.

## Validation

전체 build와 CTest 5개가 통과했다. Vulkan validation 및 synchronization validation을 요청한 verbose CTest 로그에서 VUID·SYNC validation 오류가 없었다. GPU fixture는 다른 역방향 슬롯 번호, UV chart·Surface range 간 연결, 여러 Registry channel, unsupported/invalid 항목, poisoned scratch 전체 덮어쓰기, 서로 다른 Capacity/Profile, source alpha·Decay·초과량 보존·InputDelta 소비를 포함한다. zero timestep과 Profile rate 편집 후 연속 AB/BA step을 같은 command submission에 기록해 오래된 RawFlux가 재사용되지 않는지 확인한다. 기존 Virtual Height·normal, 비균일 scale, 중력 반전 및 term toggle fixture도 통과했다. 기본 `Demo_Cubes_Wetness.Scene`을 8프레임 실행해 정상 종료(exit 0)와 4개 instance·2개 공유 data variant 생성을 확인했다. 앱 로그에는 Vulkan validation error가 없으며 작은 buffer의 개별 allocation에 대한 기존 성능 권고 warning은 남아 있다.

## 합성 GPU 비교

Apple M1, 6개 Surface × 512×512의 모든 texel 유효 8-neighbor grid, 1 Registry channel, identity transform, world gravity `(0,0,-1)`이다. 위치는 `(0.01x, 0.01y, 0.0025x)`, normal은 `normalize(-0.25,0,1)`이고 MesoNormal은 macro fallback이다. 실제 Cube Mesh·Normal Map·렌더링을 포함하지 않는다. Capacity=1, SaturationTransferRate=0.2, GeometryTransferRate=50, DecayRate=0.03, CavityRetentionFactor=0.5로 DemoWetness 수치를 사용한다. ConcavityWeight=0, dt=1/60, EventInput=0이다.

초기 State는 `0.25 + 0.5 × (i % 31) / 30`이다. 전체 State는 모든 texel에 이 값을 두고, 1.56% State는 `i % 1024 < 16`인 texel에만 두며, 빈 State는 모두 0이다. 각 dispatch는 같은 State A를 읽고 B에 쓰므로 같은 입력 조건을 유지한다. CPU preparation·upload는 시간 밖이다.

변경 전 shader는 이번 변경 직전 파일을 별도로 보관해 동일 glslc 기본 옵션으로 컴파일했다. 전후 모두 변경 후 CPU resource layout·barrier를 사용해 shader 경로만 비교했다. 5회 warmup 뒤 30개 timestamp 표본의 중앙값을 구하고, 순서를 번갈아 3번 반복했다. 아래는 각 run 중앙값의 중앙값이며 Total은 Pass 1 시작→Pass 2 끝으로 barrier 구간도 포함한다. pass별 중앙값의 합을 Total로 사용하지 않는다.

| State 분포 | 이전 Pass 1 / 2 (ms) | 현재 Pass 1 / 2 (ms) | Total (ms) | Total 감소 |
|---|---|---|---|---|
| 전체 State | 13.943 / 14.176 | 13.576 / 3.289 | 28.129 → 16.903 | 39.9% |
| 1.56% State | 14.267 / 4.742 | 13.691 / 3.014 | 18.995 → 16.869 | 11.2% |
| 빈 State | 16.687 / 6.886 | 13.689 / 2.312 | 23.478 → 16.464 | 29.9% |

각 반복의 Next State·RawOutgoing·alpha를 전체 배열로 비교했다. 최대 절대 오차는 전체·1.56% State에서 `5.96e-8`, 빈 State에서 `4.66e-10`이다. 검증 fixture 허용 오차 이내다. 모든 방향이 valid인 내부 grid에서도 RawFlux 저장과 gather의 부하를 포함했다.

run별 시간 편차가 있고 실제 네 큐브 Scene의 FPS는 측정하지 않았다. 위 시간 감소를 Scene FPS 개선률이나 15fps에서의 예상 FPS로 환산하지 않는다. 실제 Scene에서 동일 카메라·State·해상도로 별도 확인해야 한다.

## Related

- [[0017-Solver-Transfer-Cache|ADR 0017 — 초기 Solver cache]]
- [[0020-State-Overcapacity-Transport|ADR 0020 — State 초과량 보존]]
- [[../04_Architecture/0007_Simulation-Optimization|Simulation Optimization]]
- [[../04_Architecture/0008_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[../06_Development/Experiments/0002_Solver-Geometry-Cost|Solver GeometryDrive 비용 실험]]
