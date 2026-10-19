# Decision 0025 — 방향별 RawFlux 캐시 제거

> **한 줄 요약:** RawFlux는 Pass 1에서 합산하고 Pass 2에서 재계산하며, 방향별 캐시와 비교 UI를 제거한다.

- 분류: **Simulation**
- Status: **Historical implementation decision; A/B cache variants restored by Decision 0029**
- Date: 2026-10-05

## Context

초기안은 Pass 1에서 texel·channel·neighbor direction별 `RawFlux`를 저장하고 Pass 2에서 역방향 인덱스로 읽는 방식이었다. 이 배열은 `texelCount × channelCount × 8`개의 float32를 요구하며 Solver cache 비교를 위해 OFF 경로와 전용 UI·pipeline variant도 유지했다.

`RawOutgoing` 합계는 Pass 1에서 계속 보존할 수 있다. Pass 2는 방향별 scratch 없이 source State와 TransferWeight로 같은 raw flux를 재계산할 수 있다. 재평가 경로에서 비용이 큰 Dynamic Geometry normal은 이미 `DynamicGeometry`에 저장하므로 Solver가 저장된 위치·normal을 읽도록 한다.

## Decision

1. 방향별 `RawFlux` GPU buffer, descriptor, allocation size 계산과 관련 uint32 index 검사 코드를 제거한다.
2. Pass 1은 방향별 flux를 합산해 `RawOutgoing`에 저장하고, 방향별 값을 기록하지 않는다.
3. Pass 2는 source alpha 검사 뒤 역방향 neighbor slot, TransferWeight, source Profile/State, target 값과 source geometry로 flux를 재계산한다. `OutgoingFluxScale`과 `RawOutgoing`은 계속 사용한다.
4. ON/OFF specialization constant·pipeline variant·Solver flag·Renderer API·Debug UI를 제거한다.
5. `ReverseNeighborDirectionIndices`는 Pass 2의 재계산 경로에서 역방향 slot을 찾는 데 쓰므로 shared Geometry resource로 유지한다.
6. accumulation geometry가 활성화된 경우 `prepareSourceGeometry()`는 `DynamicGeometry`가 저장한 position과 local normal을 읽는다. 정적 경로는 기존 position과 선택된 normal을 사용한다.

## Alternatives Considered

- **방향별 캐시 유지 및 ON/OFF 비교:** 기존 구현과 Debug UI에서 사용하던 방식이다. 다채널·고해상도에서 방향별 배열 payload가 커지고 양 경로를 계속 유지해야 하므로 채택하지 않는다.
- **역방향 direction index를 제거하고 매번 탐색:** Pass 2 계산 시 연결된 source의 neighbor 배열을 다시 훑어야 하므로 현재 direction index buffer를 유지한다.

## Consequences

- 방향별 RawFlux payload와 해당 descriptor 하나가 사라진다. 기존 High 예시(6 Surface × 512×512 × 1 channel, float32, padding 없음)에서 인스턴스별 48 MiB가 제거된다. 역방향 direction index의 6 MiB shared Geometry payload는 유지된다. allocator overhead는 제외한다.
- Pass 2에서 incoming 후보마다 RawFlux·GeometryDrive 계산이 필요하므로 실제 GPU 시간은 State 분포와 GPU에 따라 달라진다. 별도 GPU timestamp 측정으로 확인한다.
- Dynamic Geometry 경로는 8-neighbor least-squares normal을 Solver가 다시 구하지 않고 geometry update 결과를 재사용한다.
- Decision 0005의 비활성 RawFlux write-elision은 방향별 cache가 없어져 적용 대상이 사라진다. 기존 측정과 검증 기록은 구현 당시의 역사 기록으로 남긴다.

## Related

- [[03_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]
- [[04_Development/0003_Solver-Performance|Solver Performance]]
- [[05_Decisions/0005_Inactive-RawFlux-Write-Elision|Decision 0005 — superseded]]
