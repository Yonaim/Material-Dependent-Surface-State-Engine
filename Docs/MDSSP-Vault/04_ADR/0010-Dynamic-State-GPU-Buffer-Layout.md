# ADR 0010 — Dynamic State GPU Buffer Layout

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련: [[0005-Per-Texel-GPU-Data-Layout|ADR 0005]], [[0006-Dynamic-State-Registry|ADR 0006]], [[0009-Texel-Profile-Index-Map|ADR 0009]], [[../03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]

## Context

State Registry는 Profile에서 모은 State의 수를 런타임에 정한다. 따라서 State A/B, OutgoingFluxScale, InputDelta는 고정 4채널이 아니라 임의의 `ChannelCount`를 저장해야 한다. 기존 ADR 0005는 이 원칙과 dense InputDelta를 정했지만, GPU의 실제 원소 배치, Profile parameter 배치, stride는 결정하지 않았다.

## Decision

### 검토안과 결정 — State channel layout

| 검토안 | 설명 | 결과 |
|---|---|---|
| AoS / texel-major | 한 텍셀의 채널 값을 연속 저장 | **채택**. Solver invocation이 텍셀 하나의 모든 채널을 처리하므로 접근이 단순하다. |
| SoA / channel-major | 한 채널의 모든 텍셀 값을 연속 저장 | 미선택. 한 텍셀의 채널 순회가 흩어지고 plane offset 관리가 필요하다. |
| 혼합 / chunked | 채널을 정해진 폭으로 묶어 저장 | 미선택. 임의 채널 수에서 tail/padding 처리가 추가된다. |

선택한 AoS의 인덱스는 `index = texelIndex * channelCount + channelIndex`다. `State A`, `State B`, `OutgoingFluxScale`, `InputDelta`는 동일한 규칙과 32-bit float 원소를 사용한다.

### 검토안과 결정 — 채널 패딩

Padding은 정렬을 위해 실제 데이터가 없는 빈 칸을 넣는 것이다. 5개 channel을 `vec4` block으로 저장한다면 8칸이 필요해 3칸이 빈다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| packed scalar | 텍셀마다 정확히 `C`개의 float 저장 | **채택**. padding 없이 임의의 `C`를 지원한다. 텍셀 stride는 `C` float다. |
| 고정 폭 vector block | 4개 등 고정 수의 channel을 묶어 저장 | 미선택. 마지막 block에 빈 slot이 생길 수 있고 이점은 미측정이다. |

### 검토안과 결정 — Profile parameter layout

논리 lookup은 `(ProfileIndex, ChannelIndex) → parameters`다. 이는 State 값이 아니라 Profile이 채널마다 정하는 Capacity, InputFactor, TransferRate 등의 설정값이다. **레코드(record)**는 Profile 하나와 State channel 하나의 조합에 속하는 parameter 묶음 한 항목을 뜻한다. 새 ID 종류가 아니라, Profile table GPU 배열의 한 원소다. `recordIndex`는 Profile index와 channel index 두 값을 1차원 배열 위치로 평탄화한 번호다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| Profile-major AoS | Profile/channel별 설정 묶음을 연속 저장 | **채택**. 조회 키로 해당 묶음을 직접 찾는다. |
| field-major SoA | Capacity, Rate 등 parameter 종류별로 배열 분리 | 미선택. 사용하지 않는 필드를 건너뛸 수 있으나 여러 buffer/주소 계산이 늘어난다. |
| 독립 packed vector layout | 각 Profile/channel 설정을 고정 vector block으로 pack | 별도 layout으로는 미선택. 채택한 레코드 내부에서 `vec4` 두 개를 사용한다. |

Profile/channel 레코드는 8개의 32-bit float를 `vec4` 두 개로 담아 32 byte로 고정한다. Profile-major 순서이므로 channel이 4개일 때 Profile 1, channel 2는 `1 * 4 + 2 = 6`번 레코드다.

```text
vec4 0 = StateCapacity, InputFactor, SaturationTransferRate, GeometryTransferRate
vec4 1 = DecayRate, CavityRetentionFactor, AccumulationFactor, CavityFillFactor
recordIndex = profileIndex * channelCount + channelIndex
```

CPU domain parameters에서 GPU 전용 record로 pack한다. Profile이 정의하지 않은 State는 값 0과 구분되도록 별도 dense `uint32` support map으로 나타낸다. map은 `(profileIndex, channelIndex)`마다 한 항목이며 동일한 `recordIndex`를 쓴다.

Profile parameter는 State buffer와 다르게 Profile-major AoS를 쓰며, State의 texel-major 배열 규칙을 강제하지 않는다.

### Buffer size and limits

Packed scalar layout에서 State 계열 buffer 하나의 payload 크기는 다음과 같다.

```text
texelCount × channelCount × sizeof(float)
```

구현은 allocation 전에 곱셈 overflow를 검사하고, 계산된 크기가 Vulkan buffer 및 storage-buffer range 한도를 넘지 않는지 확인한다. overflow나 기기 한도 초과는 잘린 크기로 계속하지 않고 명확한 오류로 처리한다. Padding이나 레코드 layout을 택하면 payload와 실제 allocation 크기를 구분해 기록한다.

## Alternatives Considered

- **SoA / channel-major State 배열**은 연속 texel에서 같은 channel 접근이 유리할 수 있지만, 한 invocation이 한 texel의 모든 channel을 처리하는 현재 solver workload에 맞춰 AoS를 채택했다.
- **고정 폭 vector block 및 padding**은 channel 수가 임의이므로 tail slot 처리와 낭비 공간이 생긴다. 정렬 또는 성능 이점이 측정되기 전에는 packed scalar를 사용한다.
- **field-major Profile parameter 배열**은 사용하지 않는 필드 접근을 줄일 수 있으나 여러 배열과 주소 계산이 필요하다. 조회 시 필요한 Profile/channel 레코드를 연속으로 두는 Profile-major AoS를 채택했다.
- 미지원 Profile/channel을 값 sentinel로 표시하는 대신 별도 dense `uint32` support map을 둔다. parameter 값 0과 미지원 상태를 분리하기 위해서다.

## Consequences

- State와 OutgoingFluxScale/InputDelta의 memory layout이 채널 수에 따라 동적으로 달라진다.
- 한 texel의 여러 State channel을 연속으로 읽을 수 있다.
- 채널 수가 임의이므로 shader와 CPU packer는 고정 `vec4` channel 수를 가정하면 안 된다.
- 계산 비용과 GPU access 성능은 실제 구현 후 검증해야 한다. AoS 선택만으로 성능 우위가 측정된 것은 아니다.

## Implementation checks for Branch 4

- `float32` channel array 및 32-byte Profile record의 CPU↔GLSL offset/stride 일치
- Profile support map이 Registry/Profile channel index와 일치
- allocation size 곱셈 overflow 및 기기 storage-buffer 한도 확인
- 1, 4, 6개 이상 채널에서 State와 Profile lookup 경계 검증

## Related

- [[0005-Per-Texel-GPU-Data-Layout|ADR 0005 — Per-Texel GPU Data Layout]]
- [[0006-Dynamic-State-Registry|ADR 0006 — Dynamic State Registry]]
- [[0009-Texel-Profile-Index-Map|ADR 0009 — Texel Profile Index Map]]
- [[../03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
