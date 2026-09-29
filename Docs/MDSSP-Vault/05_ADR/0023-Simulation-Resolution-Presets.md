# ADR 0023 — 시뮬레이션 해상도 프리셋

> **한 줄 요약:** Surface simulation 해상도를 Low, Medium, High 프리셋으로 선택한다.

- 분류: **Simulation**
- Status: **Accepted · 구현 완료**
- Date: 2026-09-28
- 관련 문서: [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 캐시]], [[../04_Architecture/0010_UI-Interface|UI Interface]]

## Context

Surface simulation grid가 512×512로 고정되어 기본 Cube Scene에서도 많은 texel invocation과 큰 cache payload를 사용했다. Pass 1 source 재사용과 별도로 해상도를 선택하여 작업량과 공간 정밀도를 조절할 수 있어야 한다.

초기 프리셋 구현은 해상도를 실행 세션의 전역 선택값으로 유지하고 새 Scene에도 이어서 적용했다. 2026-09-29부터 현재 결정은 각 `.Scene`이 해상도를 소유하고 로드·저장 시 복원하는 방식이다.

## Decision

1. Simulation 탭에 Low(128), Medium(256), High(512) 순서의 드롭다운을 둔다. 기본값은 Medium이며 각 Surface에 정사각형 grid를 적용한다.
2. 선택 시 mesh/Profile Distribution과 해상도로 CPU Geometry를 준비한다. [[0026-Resolution-Surface-Cache|ADR 0026]]부터 유효한 `.Surface`를 우선 로드하며, cache miss/stale/corrupt일 때 mapping·Profile map·Normal Map transfer normal·Virtual Meso Geometry를 다시 생성한다. GPU geometry·State/cache·descriptor·Solver·debug pipeline·timing query는 기존처럼 재생성한다.
3. State A/B·InputDelta·pending contact를 초기화한다. UI는 변경 시 State가 초기화됨을 안내한다. 재표본화와 State 보존은 구현하지 않는다.
4. CPU asset cache key에 해상도를 포함한다. 같은 mesh·distribution·해상도 조합은 공유한다. 모든 새 handle을 준비한 뒤 persistent Scene에 적용하고, GPU idle 후 자원을 교체한다. 준비/교체 실패 시 이전 Scene handles·해상도·GPU 자원을 유지한다.
5. 성공 시 사용하지 않는 이전 CPU runtime geometry를 해제한다. 해상도는 `TScene`이 소유하고 `.Scene` 최상위 `simulationResolution`에 정수 128·256·512 중 하나로 저장한다. 필드가 없으면 이전 Scene의 값과 무관하게 256을 사용한다. Loader는 object를 로드하기 전에 값을 검증하고 해당 해상도를 Surface Data 생성에 명시적으로 전달한다. 활성 해상도는 GPU 자원 준비 성공 후 갱신하며, UI 해상도 변경도 현재 Scene의 값에 반영해 다음 저장에 포함한다. INI에는 저장하지 않는다.
6. Solver term toggle·runtime Profile override·transform·카메라·pause/speed는 유지한다. Renderer timing query와 UI 평균은 변경 시 초기화하여 이전 해상도 시간과 섞지 않는다.

## Alternatives Considered

- 512 고정: 현재 공간 정밀도는 유지하지만 텍셀 수를 직접 줄일 수 없다.
- 변경 시 기존 State 재표본화: 연속 진행이 가능하지만 표면 mapping·invalid/seam texel·총량 보존에 대한 별도 계약이 필요하여 미채택이다.
- 표시 선택만 바꾸거나 dispatch 수만 축소: CPU mapping·GPU geometry·접촉·debug lookup 크기가 불일치하므로 자원 전체 재생성을 채택한다.

## Consequences

- Surface 수가 같을 때 Medium은 High의 1/4, Low는 High의 1/16 텍셀을 사용한다. 이는 texel 수 비율이며 FPS 비율이 아니다. 해상도별 이산화와 Virtual Meso Geometry 복원 결과는 달라질 수 있다.
- 6 Surface·1 Registry channel·8슬롯·원소 padding 없는 float32 RawFlux에서 instance당 payload는 Low 3 MiB, Medium 12 MiB, High 48 MiB다. uint32 역방향 슬롯은 공유 Geometry당 각각 0.375/1.5/6 MiB다. allocator overhead와 다른 buffer는 제외한다.
- 해상도 전환은 CPU cache load/검증 또는 miss 전처리와 GPU 생성이 끝날 때까지 동기적으로 처리되어 잠시 멈출 수 있다. 이전 자원은 새 준비가 끝날 때까지 살아 있으므로 전환 중 peak memory는 steady-state보다 크다.
- GPU addressing/`maxStorageBufferRange` 한도 검사는 선택값에도 유지한다. 실패를 숨겨 임의 해상도로 낮추지 않는다.

## Validation

### 초기 프리셋 구현

전체 build와 CTest 5개가 통과했다. 실제 Cube Wetness Scene과 Renderer를 구동한 통합 검사에서 초기 Medium 256, 128→512→256→128→256 전환, 모든 Surface의 CPU/GPU texel 수 일치, 4개 instance·2개 geometry 공유, State read buffer A 초기화, InputDelta clear/소비, 이전 CPU geometry 해제와 Solver toggle 보존을 확인했다. 각 해상도에서 Lit·State Heatmap·Virtual Height Offset 렌더링을 실행했다. 일부 후보 mapping 준비 뒤 sidecar 읽기 실패를 유도하여 이전 handles·해상도·descriptor와 정상 렌더링이 유지되는 rollback을 확인했다. 지원하지 않는 64 해상도는 기존 선택을 유지하며 거부했다. Vulkan validation 오류가 없었다. 드롭다운 직접 클릭/화면 검사는 Computer Use 접근 권한 대기로 수행하지 못했으며, 동일 callback의 Renderer API와 실제 ImGui frame/render 경로를 검증했다.

## Related

- [[0026-Resolution-Surface-Cache|ADR 0026 — 해상도별 Surface 전처리 캐시]]
- [[0022-Pass1-Source-Reuse|ADR 0022 — Pass 1 source 재사용]]
- [[0021-Directional-RawFlux-Cache|ADR 0021 — 방향별 캐시]]
- [[../04_Architecture/0010_UI-Interface|UI Interface]]
- [[../04_Architecture/0008_Surface-GPU-Data-Layout|GPU Data Layout]]
