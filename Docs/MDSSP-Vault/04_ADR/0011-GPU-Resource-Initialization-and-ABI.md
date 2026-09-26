# ADR 0011 — GPU Resource Initialization, Descriptors, and CPU↔GPU ABI

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련: [[0005-Per-Texel-GPU-Data-Layout|ADR 0005]], [[0010-Dynamic-State-GPU-Buffer-Layout|ADR 0010]], [[../03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]], [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]

## Context

GPU buffer는 데이터 layout뿐 아니라 생성 시 초기값, CPU 자료형과 GLSL의 byte-level 일치, shader resource 연결도 명확해야 한다. 이 계약이 불분명하면 초기화되지 않은 State를 읽거나, CPU와 shader가 같은 구조체를 서로 다른 byte offset으로 해석할 수 있다.

## Decision scope

이 ADR은 Solver의 수식이나 dispatch 방식은 정하지 않는다. 브랜치 4에서 다음 세 가지 resource 계약을 결정한다.

1. State A/B, OutgoingFluxScale, InputDelta의 초기화와 InputDelta 재사용
2. Shader descriptor와 ping-pong 연결
3. CPU domain data를 GPU ABI로 pack하고 검증하는 방식

## 1. Buffer initialization and reuse

초기화의 **의미상 계약**은 결정한다. 셰이더가 읽는 값은 그 전에 정의되어 있어야 한다. State A/B의 시작값은 0이고, OutgoingFluxScale은 읽히기 전에 해당 단계에서 유효한 값이 쓰여 있어야 한다. InputDelta는 접촉 discrete event(발생 시점에 한 번 기록되는 접촉 사건)의 양을 누적하고, 이벤트 뒤 처음 실행되는 solver update에서 한 번 반영한 뒤 다음 입력 누적 전에 비운다. 지속 입력은 이 버퍼 의미에 포함하지 않는다. InputDelta buffer는 매 frame 새로 할당하지 않고 재사용한다.

실제 Buffer별 접근 특성을 반영해 MVP 초기화 경로를 구현한다. 현재 GPU resource는 `TGPUBuffer`가 지원하는 host-visible/coherent memory를 사용한다. CPU packed Geometry/Profile은 생성 시 한 번 upload하고, instance State A/B, OutgoingFluxScale, InputDelta는 생성 시 0을 upload한다. 이는 현재 MVP의 구현 선택이며 device-local/staging 최종 정책은 아니다.

| Buffer | 초기화와 현재 구현 | 이후 사용 |
|---|---|---|
| State A/B | 생성 시 0을 CPU upload | Solver 단계에서 ping-pong으로 읽고 씀 |
| OutgoingFluxScale | 생성 시 0을 CPU upload | 각 solver update에서 읽기 전에 유효한 단계 값을 기록해야 함 |
| InputDelta | 생성 시 0을 CPU upload | 입력/solver 구현에서 event upload, 소비 후 clear 및 GPU 동기화를 추가해야 함 |

`TGPUBuffer::Download`는 host-visible/coherent buffer를 readback해 packed ABI와 생성 초기값을 검사한다. InputDelta의 매 update 소비 후 clear 및 frame-in-flight 안전 규칙은 compute/input 경로가 구현될 때 확정한다. 추후 device-local 경로에서는 staging, transfer clear 또는 compute clear로 바꿀 수 있다.

## 2. 검토안과 결정 — Descriptor 및 ping-pong

Descriptor는 shader binding에 실제 GPU buffer를 연결하는 Vulkan 설정이다. 논리 resource는 Shared Geometry, Profile parameters/Profile index map, Current/Next State, OutgoingFluxScale, InputDelta다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| 매 step descriptor 갱신 | Current/Next가 바뀔 때마다 binding을 수정 | 미선택. 매 step host-side 갱신이 필요하다. |
| AB/BA descriptor set 사전 생성 | A→B용과 B→A용을 각각 만들어 번갈아 사용 | **채택**. 역할 전환이 명확하고 매 step descriptor 수정이 없다. |

실제 descriptor binding/type/range 계약은 [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]의 12개 storage-buffer binding 표를 따른다. 각 binding은 descriptor count 1과 해당 buffer 전체 range를 사용한다.

## 3. 검토안과 결정 — CPU↔GPU ABI 및 packing

CPU domain struct는 프로그램에서 다루기 편한 논리 자료형이고, GPU ABI는 shader가 읽는 정확한 byte layout이다. 두 쪽의 padding·정렬·필드 위치가 자동으로 같다고 가정할 수 없다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| CPU domain struct를 그대로 upload | 기존 자료형 메모리를 그대로 복사 | 미선택. compiler/library padding이 shader layout과 달라질 수 있다. |
| CPU domain data에서 GPU 전용 packed representation 생성 | upload용 배열/구조체에 필드 순서와 크기를 명시 | **채택**. CPU 모델과 shader ABI를 분리해 통제한다. |

GPU 전용 구조체는 `sizeof`, `alignof`, `offsetof`를 compile-time 검사한다. 각각 구조체 byte 크기, 정렬, 필드 시작 위치를 확인한다. 배열 원소 수·stride·전체 byte size도 별도로 검증한다. Vulkan GPU resource test가 알려진 Geometry/Profile 값과 State 초기 0값을 readback해 확인한다. Channel scalar 배열은 ADR 0010의 indexing/size 산식을 따른다.

## Alternatives Considered

- Descriptor를 매 step 갱신하는 방식과 AB/BA set을 미리 만드는 방식을 비교해, 갱신을 피할 수 있는 사전 생성 방식을 채택했다.
- CPU domain struct를 그대로 복사하는 방식은 compiler/library padding이 GLSL 배치와 달라질 수 있어 채택하지 않았다. GPU 전용 packed representation으로 변환하고 검증한다.
- CPU upload, transfer clear, compute clear는 buffer memory property와 사용 흐름이 정해지기 전에는 공통 방식으로 고르지 않는다. 각 buffer별 선택은 구현 단계에 남긴다.

## Consequences

- 모든 State-related buffer의 초기값과 재사용 시점이 정의된다.
- Shader binding과 ping-pong 전환 방식이 명시된다.
- CPU domain model의 라이브러리/compiler layout이 GPU ABI에 암묵적으로 노출되지 않는다.
- MVP의 clear 구현과 최종 GPU memory type은 별개로 발전시킬 수 있다.

## 후속 작업 (Branch 4 범위 밖)

- InputDelta event upload/consumption/clear와 frame-in-flight 동기화 (입력/solver 구현에서 처리)
- device-local/staging memory 경로와 allocation suballocation 최적화

Descriptor의 구현 계약은 Branch 4 개발 노트에 기록한다. 현재 구현은 12개의 storage-buffer binding을 사용하며 binding별 원소 형식과 Current/Next 연결은 [[../05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]에 정리한다. CPU ABI 구조체의 크기, 정렬, 주요 offset은 compile-time assertion으로 검증한다.

## Related

- [[0005-Per-Texel-GPU-Data-Layout|ADR 0005 — Per-Texel GPU Data Layout]]
- [[0010-Dynamic-State-GPU-Buffer-Layout|ADR 0010 — Dynamic State GPU Buffer Layout]]
- [[../03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[../05_Development/Notes/0003-Surface-State-GPU-Resource|Surface State GPU Resource]]
