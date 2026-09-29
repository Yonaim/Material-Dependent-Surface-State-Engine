# ADR 0020 — State A/B에 초과량을 보존하는 Transport

> **한 줄 요약:** State A/B에 Capacity 초과량을 포함해 보존하고, Saturation 차이에 따른 기존 Transport 경로로 다음 Solver step부터 이동시킨다.

- 분류: **Simulation**
- Status: **Accepted — 구현 완료 · 일부 GPU 회귀 검증 완료**
- Date: 2026-09-28
- 관련 문서: [[0001-Capacity-and-Saturation|ADR 0001 — Capacity와 Saturation]], [[../04_Architecture/0002_Surface-State|Surface State 데이터 계약]], [[../04_Architecture/0006_Surface-State-Update|Surface State 갱신식]]
- Supersedes: [[0001-Capacity-and-Saturation|ADR 0001]]의 State 상한 및 초과량 처리 계약

## 후속 결정 — 2026-09-29

ADR 0030에서 State를 texel 총량으로 명시하고 Capacity를 고정 기준 면적에서 실제 texel 면적으로 환산한다. 저장 상한 없음과 source alpha 제한은 유지한다.

## Context

초기 계약은 `0 ≤ State ≤ stateCapacity`이며 Pass 2에서 Capacity로 clamp했다. 전달·이벤트 입력으로 기준량을 초과하면 저장하지 못한 양이 사라졌다. 초기 Shader의 Saturation도 `[0,1]`로 clamp해 초과 상태를 Saturation 차이에 반영하지 못했다.

초과량을 다음 Solver step의 기존 Transport로 이동시키되 GPU 메모리 payload와 pass 수를 늘리지 않는 계약이 필요하다. 초과량을 이웃으로 즉시 연쇄 재분배하는 별도 알고리즘은 이번 결정의 목표가 아니다.

## Decision

### State / Capacity

| 항목 | 채택 계약 |
|---|---|
| State | 입력·유입·유출·Decay를 반영한 전체 상태량. 유효하고 지원되는 채널에서 finite, 비음수이며 Capacity 초과 허용 |
| stateCapacity | 저장 상한에서 **포화 기준량**으로 의미 변경. 유한한 양수이며 기존 Profile 필드·기본값 유지 |
| Saturation | `State / stateCapacity`. 전달 계산에서는 1을 초과할 수 있고 상한 clamp하지 않음 |
| 초과량 | `max(State - stateCapacity, 0)`인 파생값. State A/B에 포함되며 별도 저장하지 않음 |
| 표시용 정규화 | Heatmap 등 `[0,1]` 표시에는 Saturation을 별도로 clamp 가능. 전달 계산에 되돌려 쓰지 않음 |

### 갱신과 전달

아래 Rate는 Profile의 `[0,1]` TransferFactor에 Solver 기준 속도 `1.0`, `100.0`을 곱한 실제 속도다 ([[0029-Normalized-Transport-Factors|ADR 0029]]). 보유량·Saturation·alpha 계약은 유지한다.

```text
Saturation_i = Current_i / Capacity_i
SaturationDrive(i→j) = max(Saturation_i - Saturation_j, 0)
RawFlux(i→j) = (SaturationDrive × source SaturationTransferRate
               + GeometryDrive × source GeometryTransferRate)
               × TransferWeight(i,j) × Δt
Available_i = max(Current_i - Decay_i, 0)
alpha_i = RawOutgoing_i > 0 ? min(1, Available_i / RawOutgoing_i) : 1
Flux(i→j) = RawFlux(i→j) × alpha_i
Next_i = max(Current_i + EventInput_i + Incoming_i - Outgoing_i - Decay_i, 0)
```

- RawFlux의 Drive/Weight 및 source Profile rate 구조는 유지한다. Capacity는 각 endpoint의 Saturation 계산에 사용하며 목적지의 남은 저장 공간을 검사하거나 전달량의 상한으로 사용하지 않는다.
- Pass 1의 OutgoingFluxScale(alpha)은 감쇠 후 source 보유량만 제한한다. 초과분을 전량 즉시 보내기 위해 alpha를 1보다 크게 만들지 않는다. alpha는 State가 Capacity보다 커도 전체 Current를 가용량으로 사용한다.
- 후속 [[0022-Pass1-Source-Reuse|ADR 0022]]는 가용량=0 또는 dt=0인 항목의 raw 평가를 생략하고 RawFlux·RawOutgoing·alpha를 0으로 기록한다. 이 분기의 제한 전 디버그 값은 위 초기 alpha 식과 다를 수 있으나 실제 outgoing과 Next는 같다.
- Pass 2는 여러 이웃의 유입과 EventInput을 합산해 Capacity 상한 없이 Next에 보관한다. 입력은 한 번 적용하고 InputDelta를 비운다.
- Next는 A/B 역할 교환 뒤 **다음 Solver step**에서 Current로 읽는다. 받은 양의 후속 이동도 기존 RawFlux와 rate·Δt에 따른다. 렌더 프레임과 Solver step은 같은 개념으로 고정하지 않는다.
- 자신의 Next만 쓰는 gather 및 2-Pass를 유지한다. 목적지 수용 비율(beta), 별도 Overflow buffer, 추가 채널·pass·descriptor·동기화는 도입하지 않는다.
- 이동 경로가 없거나 TransferRate/Weight가 0이면 초과량은 State에 남는다. 양쪽 Saturation이 같고 `GeometryDrive`가 없으면 Saturation-driven flux는 0이다. 주변의 모든 텍셀이 기준량을 넘더라도 State 저장이 허용되며, 기준량 이하로 반드시 내려가는 것을 보장하지 않는다.
- invalid/unsupported texel의 State=0 계약은 유지한다. Accumulation은 기존대로 **전체 State × accumulationFactor**를 사용하며 별도 초과량을 중복 더하지 않는다. 실제 적층은 미구현이다.

### 구현 상태

`Shaders/SurfaceSolverCommon.glsl::saturation()`은 상한 없는 `Current / Capacity`를 반환하고 `SurfaceSolverPass2.comp`는 `max(Current + Input + Incoming - Outgoing - Decay, 0)`을 기록한다. Pass 1의 가용량·alpha, 두 pass·barrier·descriptor·GPU ABI는 유지한다. 기존 Geometry source/sink fixture의 기대값을 기준량 초과 보존에 맞췄다. 앱·compute shader 빌드와 CTest 5개가 통과했다. 기존 Geometry fixture와 ADR 0021의 multichannel cache fixture에서 source 유출 제한, 여러 이웃 유입·EventInput의 초과량 보존, 서로 다른 Capacity/Profile, 입력 소비, unsupported/invalid 처리를 GPU로 확인했다. 아래 후속 항목의 전수 검증과 timestep 비교는 별도 수행한다. ADR 0020 자체의 추가 payload는 0 B이며, 이후 ADR 0021의 성능 캐시 payload와 구분한다.

## Alternatives Considered

| 방안 | 평가 |
|---|---|
| 기존 Capacity clamp | 메모리는 같지만 유입·입력 초과량이 사라져 채택하지 않음 |
| State A/B에 전체 양 보존 | 추가 GPU payload 없이 RawFlux로 후속 이동 가능하여 채택 |
| State와 별도 Overflow buffer | 저장 상한은 유지하나 texel·채널마다 추가 float32 저장과 양의 합산 필요하여 채택하지 않음 |
| 목적지 수용 비율(beta)을 추가한 3-Pass | 목적지 상한을 유지하며 거절된 양을 source에 남길 수 있지만 이번의 후속 전달 정책과 다르고 임시 자원·pass를 추가하므로 채택하지 않음 |
| 같은 step에서 초과량을 연쇄 재분배 | 별도 반복·종료·경로 규칙이 필요하여 채택하지 않음 |

## Consequences

- State A/B는 기존 texel-major AoS, instance별 float32, channel padding 없음의 타입·stride·개수를 유지한다. 이 결정에 따른 **추가 GPU payload는 0 B**다. 기존 scratch/cache 크기도 유지한다.
- 계산 결과가 바뀐다. State가 1을 초과하는 Saturation을 만들고 Input·유입이 상한 clamp로 손실되지 않는다. 미지원/invalid 처리와 수치 오류·다른 sink까지 포함해 무조건 총량 보존을 주장하지 않는다.
- 양 endpoint에 동일한 Flux를 빼고 더하며 입력·Decay를 끈 지원된 닫힌 graph에서는 텍셀 State 합 보존을 검증한다. 이는 면적·물리 부피를 반영한 보존 검증과 구분한다.
- Capacity 초과는 정상 상태다. finite/nonnegative 검사와 float32 overflow·작은 Capacity에 의한 큰 비율 진단은 필요하다. 안전을 이유로 초과량을 조용히 삭제하는 상한 clamp로 되돌리지 않는다.
- 표시용 Heatmap의 최상위 색은 Saturation≥1 구간에서 동일할 수 있다. 표시 정규화와 실제 저장량을 구분한다.
- Transition의 threshold는 새 Saturation 기준과 함께 검토한다. Wetness와 SurfaceWater의 물리적 의미, 고정 timestep/substep, 해상도 독립성은 이 결정으로 자동 해결되지 않는다.

## 검증 및 구현 후속

- 단일 Inject가 Capacity를 초과해도 State A/B에 전체 입력량이 남고 InputDelta는 한 번 소비됨.
- 여러 이웃의 동시 유입으로 Capacity를 넘어도 초과분이 보존됨.
- `[1.2, 1.0]`, 동일 Capacity, Geometry/Decay/Input OFF에서 saturation-driven 전달이 발생하고 합이 보존됨.
- `[1.2, 1.2]`, 동일 Capacity, Geometry OFF에서는 saturation-driven flux가 0.
- Rate/Weight=0, 이웃 없음, 전부 기준량 초과인 graph에서 임의의 초과량 삭제 없음.
- 큰 Rate·Δt에서도 source 유출이 감쇠 후 전체 보유량을 초과하지 않고 finite/nonnegative 유지.
- 서로 다른 Capacity/Profile, seam, 여러 Registry 채널, Pause/Step/Reset 및 기존 fixture 회귀.
- 동일 총 시간의 1/30·1/60 비교는 별도 수행. 무조건 timestep 독립을 보장하지 않음.

## Related

- [[0001-Capacity-and-Saturation|ADR 0001 — 초기 상한 계약]]
- [[0017-Solver-Transfer-Cache|ADR 0017 — 기존 pass/cache 재사용]]
- [[../04_Architecture/0002_Surface-State|State 데이터 계약]]
- [[../04_Architecture/0006_Surface-State-Update|Solver 갱신식]]
- [[../04_Architecture/0008_Surface-GPU-Data-Layout|GPU 배치]]
- [[../04_Architecture/0007_Simulation-Optimization|Simulation Optimization]]
- [[../02_Research/0001_Bound-Preserving-Transport|포화와 전달 연구 노트]]

- [[0021-Directional-RawFlux-Cache|ADR 0021 — 방향별 RawFlux 캐시 및 GPU 회귀]]
