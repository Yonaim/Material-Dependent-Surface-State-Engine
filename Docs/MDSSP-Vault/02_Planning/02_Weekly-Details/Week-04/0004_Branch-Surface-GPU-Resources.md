# Branch 4 — Surface GPU Resources

브랜치: `feat/surface-gpu-resources`  
선행 조건: `feat/shared-geometry-build` 병합  
관련 설계: [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

## 목표

CPU에서 검증한 Mapping/Geometry/Profile 데이터를 Vulkan Storage Buffer로 올리고, instance별 State A/B와 TempAlpha/InputDelta를 생성한다. 이 브랜치에서는 compute shader가 실제 수식을 실행하지 않아도 된다.

## 현재 기반에서 주의할 점

현재 `GPUBuffer`는 다음 특성을 가진다.

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
| Position | `vec4[]` |
| Normal | `vec4[]` |
| GeometryScalar | texel당 2 float (`MesoVirtualHeight`, `ConcavityWeight`) |
| NeighborIndex | texel당 `uvec4[2]` |

invalid texel은 `TexelSurfaceIndex == InvalidSurfaceID`로 판정한다. 거리와 전달 방향은 Position 차이에서 계산하므로 `NeighborDistance`는 GPU에 올리지 않는다.

### Profile

```cpp
struct alignas(16) SurfaceResponseProfileGPU
{
    glm::vec4 StateCapacity;
    glm::vec4 InputFactor;
    glm::vec4 SaturationTransferRate;
    glm::vec4 GeometryTransferRate;
    glm::vec4 DecayRate;
    glm::vec4 CavityRetentionFactor;
    glm::vec4 AccumulationFactor;
    glm::vec4 CavityFillFactor;
};
```

CPU 구조체 크기와 각 필드 offset에 `static_assert`를 둔다. 각 vec4의 component 순서는 Wetness, Heat, Burn, Mud다.

### Instance

```text
SurfaceInstanceStateGPU
├─ State A          vec4 × texelCount
├─ State B          vec4 × texelCount
├─ TempAlpha        vec4 × texelCount
├─ InputDelta       vec4 × texelCount
└─ SurfaceProfileIndex uint × surfaceCount
```

모든 resource를 0으로 초기화한다. 초기화되지 않은 GPU memory를 State로 사용하지 않는다.

## Local/Global Index 규칙

Shared Geometry의 `NeighborIndex`는 Geometry 내부 local texel index다. instance가 State buffer pool의 `stateBaseIndex`를 갖는 경우 Shader에서만 base를 더한다.

```text
stateIndex = stateBaseIndex + localTexelIndex
neighborStateIndex = stateBaseIndex + NeighborIndex[localTexelIndex][slot]
stateVectorIndex = stateIndex
stateComponent = channel
```

이 규칙을 지켜야 하나의 Geometry를 여러 instance가 공유할 수 있다.

## 클래스 구성 권장

- `SharedSurfaceGeometryData`: CPU field와 GPU buffer 소유 또는 GPU wrapper 참조
- `SurfaceInstanceStateData`: instance별 State resource 소유
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
Surface→Profile Table
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
2. CPU Geometry를 vec4/uvec4와 2-float scalar 구조체 upload 배열로 변환. ValidMask/NeighborDistance는 GPU buffer로 만들지 않는다.
3. Shared Geometry buffer 생성과 upload
4. Profile buffer 생성과 upload
5. instance State A/B, TempAlpha, InputDelta 생성 및 clear
6. Surface→Profile index upload
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
- CPU의 `Wetness`, `Heat`, `Burn`, `Mud`가 GPU `vec4`의 `x`, `y`, `z`, `w`에 각각 pack되는지 확인

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

1. `Feat: define surface GPU buffer layouts`
2. `Feat: upload shared surface geometry buffers`
3. `Feat: allocate instance state ping-pong buffers`
4. `Feat: create surface solver descriptor sets`
5. `Test: validate surface GPU resource initialization`

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
