# Branch 4 — Surface GPU Resources

브랜치: `feat/surface-gpu-resources`  
선행 조건: `feat/shared-geometry-build` 병합  
관련 설계: [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

## 목표

CPU에서 검증한 Mapping/Geometry/Profile 데이터를 Vulkan Storage Buffer로 올리고, instance별 State A/B와 TempAlpha/InputDelta를 생성한다. 이 브랜치에서는 compute shader가 실제 수식을 실행하지 않아도 된다.

State와 Profile parameter의 GPU 배치는 `.SRProfile`에서 생성된 `TSurfaceStateRegistry::ChannelCount`를 지원해야 한다. texel별 dense ProfileIndex map 기본안은 [[../../../../04_ADR/0009-Texel-Profile-Index-Map|ADR 0009]]를 따른다. 임의 개수 채널을 위한 AoS/SoA 및 buffer stride/indexing은 이 브랜치에서 결정·검증하고, 고정 4채널 `vec4` layout을 계약으로 사용하지 않는다.

## 현재 기반에서 주의할 점

현재 `TGPUBuffer`는 다음 특성을 가진다.

- 생성 시 memory type을 직접 지정
- host-visible + host-coherent buffer만 `Upload` 가능
- move 불가
- fill/copy/readback helper 없음

4주차 MVP에서는 Shared Geometry와 State buffer를 host-visible/coherent로 만들어 구조를 먼저 검증할 수 있다. device-local staging 최적화는 측정 후 별도 작업으로 둔다.

## 리소스 분리

### Shared Geometry

Mesh 전처리 결과 단위로 한 번 생성하고 여러 instance가 공유한다.

| Buffer | GPU 형식 |
|---|---|
| TexelSurfaceIndex | `uint[]`, invalid texel은 `InvalidSurfaceID = 0xFFFFFFFF` |
| TexelProfileIndex | `uint[]`, 유효 texel마다 Profile index 하나 |
| Position | `vec4[]` |
| Normal | `vec4[]` |
| GeometryScalar | texel당 2 float (`MesoVirtualHeight`, `ConcavityWeight`) |
| NeighborIndex | texel당 `uvec4[2]` |

invalid texel은 `TexelSurfaceIndex == InvalidSurfaceID`로 판정한다. 거리와 전달 방향은 Position 차이에서 계산하므로 `NeighborDistance`는 GPU에 올리지 않는다.

### Profile

```text
Logical lookup: (ProfileIndex, ChannelIndex) → per-State profile parameters
Physical buffer layout, stride, and AoS/SoA choice: decide and validate in this branch.
```

CPU/GPU serialization layout과 stride를 확정한 뒤 크기·offset·channel index 대응을 검증한다. Registry channel index는 Profile parameter 조회와 State buffer 접근에서 동일해야 한다.
Profile이 특정 Registry State를 정의하지 않은 경우를 구분할 수 있도록 Profile/Channel slot의 지원 여부를 보존한다. GPU 표현 방식은 이 브랜치에서 정하고, Solver와 Input 단계에서 unsupported slot을 어떻게 건너뛰는지는 Branch 5/6 계획을 따른다.

### Instance

```text
SurfaceInstanceStateGPU
├─ State A          texelCount × stateChannelCount scalar values
├─ State B          texelCount × stateChannelCount scalar values
├─ TempAlpha        texelCount × stateChannelCount scalar values
└─ InputDelta       texelCount × stateChannelCount scalar values
```

모든 resource를 0으로 초기화한다. 초기화되지 않은 GPU memory를 State로 사용하지 않는다.

## Local/Global Index 규칙

Shared Geometry의 `NeighborIndex`는 Geometry 내부 local texel index다. instance가 State buffer pool의 `stateBaseIndex`를 갖는 경우 Shader에서만 base를 더한다.

```text
channelIndex = registry.GetChannelIndex(stateId)
stateIndex = getStateIndex(instance, localTexelIndex, channelIndex)
neighborStateIndex = getStateIndex(
    instance, NeighborIndex[localTexelIndex][slot], channelIndex)
```

`getStateIndex`는 이 브랜치에서 선택하는 layout-specific helper다. AoS/SoA 선택에 따라 산식을 달리할 수 있으나 자기 texel과 이웃 texel에서 같은 `ChannelIndex`를 사용해야 한다.

이 규칙을 지켜야 하나의 Geometry를 여러 instance가 공유할 수 있다.

## 클래스 구성 권장

- `TSharedSurfaceGeometryData`: CPU field와 GPU buffer 소유 또는 GPU wrapper 참조
- `TSurfaceInstanceStateData`: instance별 State resource 소유
- `SurfaceGPUResourceLayout.h`: CPU↔GLSL pack 구조
- `SurfaceDescriptorSet`: descriptor pool/layout/set 관리

Vulkan handle과 lifetime을 한 클래스에 무작정 모으지 않는다. Shared resource와 instance resource의 파괴 시점이 다르다.

## GPUBuffer 보완

최소 필요 기능:

- 0 초기화 helper
- 작은 readback/debug helper 또는 host-visible mapping 접근
- usage flag에 `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`
- 필요 시 `VK_BUFFER_USAGE_TRANSFER_SRC/DST_BIT`

MVP에서 host-visible State를 사용하더라도 API 이름이 host-visible 구현에 고정되지 않게 한다.

## Descriptor 설계

논리적 binding은 다음 그룹을 제공해야 한다.

```text
Shared Geometry
Profile Table
Texel→Profile Index Map
Current State
Next State
TempAlpha
InputDelta
```

세부 buffer를 한 allocation에 pack하더라도 Shader contract는 offset을 통해 같은 논리 구조를 유지한다.

State ping-pong은 두 descriptor set을 미리 만든다.

```text
Descriptor AB: Current=A, Next=B
Descriptor BA: Current=B, Next=A
```

매 step `vkUpdateDescriptorSets`를 반복하지 않는다.

## 구현 순서

1. GPU pack 구조와 `static_assert`
2. CPU Geometry를 vec4/uvec4와 2-float scalar 구조체 upload 배열로 변환. State/Profile arrays는 Registry channel count에 맞춘 선택 layout으로 변환한다. ValidMask/NeighborDistance는 GPU buffer로 만들지 않는다.
3. Shared Geometry buffer 생성과 upload
4. Profile buffer 생성과 upload
5. instance State A/B, TempAlpha, InputDelta 생성 및 clear
6. Shared Geometry의 texel별 ProfileIndex map upload
7. descriptor set layout 생성
8. AB/BA descriptor set 생성
9. 생성·해제 로그와 validation 실행

## 테스트와 검증

### CPU/구조 검증

- pack 구조 크기와 offset
- 버퍼 크기(바이트)가 texel 수와 일치
- invalid SurfaceID가 유효 Surface ID와 겹치지 않음
- GeometryScalar가 texel당 8바이트인지 확인
- Position 차이에서 이웃 거리/방향이 올바르게 계산됨
- 8 neighbor가 두 `uvec4`에 올바른 순서로 pack됨
- 1개, 4개, 6개 이상의 Registry State에서 State와 Profile parameter의 index/stride mapping이 일치하는지 확인
- Channel count가 달라도 State A/B, TempAlpha, dense InputDelta의 경계와 크기가 맞는지 확인

### Vulkan 검증

- 모든 descriptor binding이 유효
- buffer range가 실제 allocation을 넘지 않음
- instance 두 개의 State buffer가 서로 다름
- Shared Geometry handle은 동일함
- AB/BA descriptor가 반대 buffer를 가리킴
- 생성/파괴 시 validation warning 없음

가능하면 작은 readback test로 초기 State/Temp/Input이 모두 0인지 확인한다.

## 메모리 확인

각 instance의 State A/B, TempAlpha, InputDelta 버퍼는 각각 texel당 16바이트를 사용한다. 네 버퍼의 합계는 texel당 64바이트다.

```text
State A    16 B
State B    16 B
TempAlpha  16 B
InputDelta 16 B
```

DebugUI에 instance 수, texel 수, 공유 Geometry 크기, instance별 State resource 크기를 바이트 단위로 표시할 수 있도록 통계를 제공한다.

## 권장 커밋 분할

1. `Feat: Surface GPU Buffer Layout 정의`
2. `Feat: Shared Surface Geometry Buffer 업로드`
3. `Feat: Instance State Ping-Pong Buffer 할당`
4. `Feat: Surface Solver Descriptor Set 생성`
5. `Test: Surface GPU Resource 초기화 검증`

## 완료 조건

- GPU resource 생성과 파괴가 안정적이다.
- Shared/Instance 소유권이 실제 handle 수준에서 분리된다.
- A/B/TempAlpha/InputDelta가 모두 명시적으로 초기화된다.
- Solver가 사용할 descriptor set과 push constant 계약이 준비된다.

## 제외 범위

- Pass 1/2 shader 수식
- compute dispatch
- Contact Input 생성
- 렌더링용 filtered texture
- device-local 최적화
