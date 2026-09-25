# ADR 0011 — GPU Resource Initialization, Descriptors, and CPU↔GPU ABI

- 상태: **대부분 Accepted / 초기화 방식은 버퍼별로 보류**
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

초기화·clear의 **구현 방식은 아직 결정하지 않는다.** State A/B, OutgoingFluxScale, InputDelta는 생성 시점, CPU/GPU 접근 패턴, memory property와 동기화 조건이 다를 수 있으므로 buffer별로 정한다.

| Buffer | 의미상 요구 | 구현 방식 |
|---|---|---|
| State A/B | 초기값 0. 이후 ping-pong 단계에서 current/next로 사용 | memory type 및 생성 경로를 정한 뒤 선택 |
| OutgoingFluxScale | 읽히기 전에 이번 단계의 값이 모두 정의되어야 함 | 모든 읽는 슬롯을 덮어쓰는지 확인한 뒤 필요 시 초기화 방식 선택 |
| InputDelta | discrete event 입력 누적 전 0. 첫 solver update에서 한 번 소비한 뒤 다음 누적 전에 clear | CPU upload, GPU clear 등 buffer 접근 경로와 동기화에 맞춰 선택 |

가능한 구현 수단은 host-visible/coherent memory의 CPU upload, transfer command clear, compute clear 등이다. 현재 `TGPUBuffer`의 `Upload`는 host-visible/coherent memory를 지원하지만, 이것만으로 모든 Surface buffer의 memory type이나 초기화 수단을 미리 결정하지 않는다. GPU 사용 중 CPU가 값을 덮어쓰지 않도록 동기화해야 한다.

따라서 브랜치 4에서는 buffer별 memory property와 사용 흐름이 구체화된 뒤 초기화·재사용 방식을 기록한다. 공통 API가 필요하면 서로 다른 backend 방식 위에 동일한 의미를 제공하도록 설계한다.

## 2. 검토안과 결정 — Descriptor 및 ping-pong

Descriptor는 shader binding에 실제 GPU buffer를 연결하는 Vulkan 설정이다. 논리 resource는 Shared Geometry, Profile parameters/Profile index map, Current/Next State, OutgoingFluxScale, InputDelta다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| 매 step descriptor 갱신 | Current/Next가 바뀔 때마다 binding을 수정 | 미선택. 매 step host-side 갱신이 필요하다. |
| AB/BA descriptor set 사전 생성 | A→B용과 B→A용을 각각 만들어 번갈아 사용 | **채택**. 역할 전환이 명확하고 매 step descriptor 수정이 없다. |

Binding 번호, descriptor type, buffer range, count 전달 위치는 실제 shader interface 구현에서 정한다. 이는 layout 전략 선택과 별개의 구체적인 API wiring이다.

## 3. 검토안과 결정 — CPU↔GPU ABI 및 packing

CPU domain struct는 프로그램에서 다루기 편한 논리 자료형이고, GPU ABI는 shader가 읽는 정확한 byte layout이다. 두 쪽의 padding·정렬·필드 위치가 자동으로 같다고 가정할 수 없다.

| 검토안 | 설명 | 결과 |
|---|---|---|
| CPU domain struct를 그대로 upload | 기존 자료형 메모리를 그대로 복사 | 미선택. compiler/library padding이 shader layout과 달라질 수 있다. |
| CPU domain data에서 GPU 전용 packed representation 생성 | upload용 배열/구조체에 필드 순서와 크기를 명시 | **채택**. CPU 모델과 shader ABI를 분리해 통제한다. |

GPU 전용 구조체는 `sizeof`, `alignof`, `offsetof`를 compile-time 검사한다. 각각 구조체 byte 크기, 정렬, 필드 시작 위치를 확인한다. 배열 원소 수·stride·전체 byte size도 별도로 검증한다. Shader layout 선언과 CPU 검증을 함께 유지하고, 알려진 값의 pack 결과 또는 작은 readback으로 일치 여부를 확인한다. Channel scalar 배열은 ADR 0010의 indexing/size 산식을 따른다.

## Alternatives Considered

- Descriptor를 매 step 갱신하는 방식과 AB/BA set을 미리 만드는 방식을 비교해, 갱신을 피할 수 있는 사전 생성 방식을 채택했다.
- CPU domain struct를 그대로 복사하는 방식은 compiler/library padding이 GLSL 배치와 달라질 수 있어 채택하지 않았다. GPU 전용 packed representation으로 변환하고 검증한다.
- CPU upload, transfer clear, compute clear는 buffer memory property와 사용 흐름이 정해지기 전에는 공통 방식으로 고르지 않는다. 각 buffer별 선택은 구현 단계에 남긴다.

## Consequences

- 모든 State-related buffer의 초기값과 재사용 시점이 정의된다.
- Shader binding과 ping-pong 전환 방식이 명시된다.
- CPU domain model의 라이브러리/compiler layout이 GPU ABI에 암묵적으로 노출되지 않는다.
- MVP의 clear 구현과 최종 GPU memory type은 별개로 발전시킬 수 있다.

## Open items for Branch 4

- 각 buffer의 memory property와 초기화·clear 수단
- frame-in-flight에서 CPU upload/clear와 GPU 사용이 겹치지 않게 하는 동기화 경계
- descriptor binding 번호/type/range와 count 전달 방식
- GPU packed record의 구체적 필드 offset 검증 및 readback 경로

## Related

- [[0005-Per-Texel-GPU-Data-Layout|ADR 0005 — Per-Texel GPU Data Layout]]
- [[0010-Dynamic-State-GPU-Buffer-Layout|ADR 0010 — Dynamic State GPU Buffer Layout]]
- [[../03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[../05_Development/Notes/0003-Surface-State-GPU-Resource|Surface State GPU Resource]]
