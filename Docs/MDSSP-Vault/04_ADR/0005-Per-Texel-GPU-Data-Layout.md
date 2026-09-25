# ADR 0005 — Per-Texel GPU Data Layout과 Dense InputDelta

- 상태: **Partially Superseded by [[0006-Dynamic-State-Registry]]**
- 날짜: 2026-09-25

## Context

초기 GPU 데이터 배치 초안은 `ValidMask`, `NeighborDistance`, 예약 component가 포함된 `GeometryScalar vec4`와 State channel을 확장하기 위한 texel당 `vec4` 두 개를 제안했다. 이후 ADR 0006에서 State 종류를 `.SRProfile` 기반 동적 Registry로 결정했으므로, 네 데모 State를 고정 채널로 취급하거나 해당 배치를 기본 layout으로 간주하지 않는다.

이 GPU 배치를 그대로 유지하면 다른 데이터와 중복되거나 사용하지 않는 값과 component가 메모리를 차지한다. 반면 `InputDelta`를 sparse 목록으로 바꾸면 입력 적용을 위한 gather/scatter 및 중복 이벤트 처리 방식까지 추가로 설계해야 한다.

## Decision

> [!warning] 대체 범위
> `ValidMask` sentinel, GPU `NeighborDistanceBuffer` 제거, 두 float `GeometryScalar`, dense `InputDelta` 재사용 결정은 유지한다. State channel은 Registry 크기에 따라 동적으로 배치한다. 물리적인 buffer layout과 channel stride는 `feat/surface-gpu-resources`에서 결정한다.

- GPU의 invalid texel 판정은 별도 `ValidMaskBuffer` 대신 `TexelSurfaceIndexBuffer`의 예약값 `InvalidSurfaceID = 0xFFFFFFFF`로 표현한다. 이 값은 유효 Surface ID로 사용할 수 없다. CPU Runtime mapping은 필요하면 별도 validity 정보를 유지할 수 있다.
- 이웃 Distance는 CPU Runtime mapping 및 GPU buffer 어느 쪽에도 저장하지 않는다. Solver가 `SurfacePosition[j] - SurfacePosition[i]`에서 거리와 방향을 필요할 때 계산한다. CPU 검증 코드도 필요하면 같은 위치에서 임시 계산한다.
- GPU `GeometryScalar`는 texel마다 실제 사용하는 `MesoVirtualHeight`와 `ConcavityWeight` 두 float만 저장한다. `vec4`로 올리거나 예약 component를 두지 않는다.
- State 종류는 고정하지 않으며 `.SRProfile`에서 수집한 Registry channel count에 따른다. 기본 demo Profile의 `Wetness`, `Heat`, `Burn`, `Mud`는 예시 workload다. texel별 State와 TempAlpha/InputDelta의 buffer layout은 임의 channel count를 지원해야 한다.
- `InputDelta`는 texel별 dense buffer로 유지하고 State Registry의 channel count를 반영한다. GPU buffer는 instance resource 생성 시 한 번 할당해 재사용하고, 이벤트 입력이 없거나 소비된 뒤 값을 clear한다. 매 frame buffer를 새로 할당하지 않는다.
- sparse InputDelta는 이 ADR에서 채택하지 않는다. 실제 입력 밀도와 성능을 측정한 뒤 별도 ADR 또는 변경으로 판단한다.

## Alternatives Considered

### 1. `NeighborDistance`를 GPU에 저장

이웃별 거리를 사전에 계산해 전송 연산을 줄일 수 있다. 다만 `SurfacePosition` 차이로 같은 거리를 구할 수 있어 중복 데이터가 된다. 이웃 위치로부터 거리와 방향을 계산하는 안을 선택한다. 추가 shader 연산과 메모리 사용량을 실제로 비교할 수 있다.

### 2. 별도 `ValidMaskBuffer` 유지

별도 mask는 역할이 분명하고, Surface ID 조회 전에 유효성을 검사할 수 있다. 대신 texel당 추가 `uint` 읽기와 저장 공간이 필요하다. 현재는 항상 조회하는 `TexelSurfaceIndex`의 sentinel로 valid 여부를 함께 표현한다. Surface ID가 필요한 shader 접근 패턴에 맞춘 결정이며, 성능 우위가 측정으로 확인됐다는 뜻은 아니다.

### 3. `GeometryScalar`를 `vec4`로 저장

`vec4`는 정렬과 shader 접근이 간단하지만 현재 두 component만 사용한다. texel마다 두 float 필드를 저장하는 8바이트 구조체를 선택한다. CPU/GPU 자료형의 크기와 각 필드 offset을 검증한다.

### 4. 상태 채널 여섯 개를 `vec4` 두 개에 저장

고정 네 channel이나 여섯 channel을 가정한 `vec4` 묶음은 Registry가 정한 임의 channel 수를 일반적으로 표현하지 못한다. 동적 buffer layout은 GPU Resource 브랜치에서 AoS/SoA, indexing, alignment와 성능을 비교해 결정한다. `SurfaceWater`, `Snow` 등은 별도 고정 enum 없이 Profile에 선언되는 State 사례다.

### 5. Sparse `InputDelta`

영향받은 texel이 매우 적다면 업로드량과 입력 저장공간을 줄일 수 있다. 그러나 texel별 solver에서 sparse 항목을 찾거나 별도 scatter/reduce 단계로 적용해야 하고, 같은 texel을 건드리는 이벤트를 합치는 정책도 필요하다. 현재는 구현이 단순한 dense buffer를 재사용한다. 입력이 전체 grid에서 차지하는 비율과 GPU 시간 측정 후 검토한다.

## Consequences

GPU에서 `NeighborDistanceBuffer`와 `ValidMaskBuffer`를 제거하고, `GeometryScalar` 크기를 texel당 16바이트에서 8바이트로 줄인다. 현재 공유 형상 데이터의 texel당 GPU 저장량은 다음과 같다.

```text
TexelSurfaceIndex  4 B   // invalid면 InvalidSurfaceID
Position          16 B
Normal            16 B
GeometryScalar     8 B   // height + concavity
NeighborIndex     32 B  // uint32 8개
합계              76 B/texel
```

각 instance는 State A/B, TempAlpha, InputDelta 네 버퍼를 가진다. Registry channel 수가 `C`이고 32-bit scalar를 padding 없이 저장한다면 각 buffer는 texel당 `4C` bytes, 합계는 `16C` bytes다. `C = 4`인 demo workload에서만 64 bytes/texel이 된다. 실제 allocation은 GPU layout과 alignment에 따라 달라질 수 있다.

```text
State A       4C B
State B       4C B
TempAlpha     4C B
InputDelta    4C B
합계          16C B/texel
```

512×512 기준으로 공유 형상 데이터는 Mesh당 약 19 MiB다. 상태 버퍼는 instance당 `4C MiB`이며, `C = 4`라면 약 16 MiB로 합계는 약 35 MiB다. 이 추정에는 할당 정렬, Profile table, 동적 적층 형상 버퍼, 렌더링용 복제 데이터가 포함되지 않는다. 설계한 자료형 크기로 계산한 값이며, 실제 GPU 사용량을 측정한 결과는 아니다.

Dense `InputDelta`는 입력이 드문 경우에도 전체 격자 크기를 유지하지만, 매 frame 재할당하지 않고 같은 버퍼를 재사용한다. 이후 성능 측정에서 입력 전달이 병목으로 확인되면 sparse 입력을 다시 검토한다.

## Related

- [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]
- [[05_Development/Notes/0002_Next-State-Calculation|Next State 계산]]
