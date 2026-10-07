# Decision 0029 — GPU A/B 비교용 컴파일 시점 셰이더 variant

> **한 줄 요약:** 성능 비교 경로를 셰이더 내부의 런타임 분기로 합치지 않고, 유효한 조합만 별도 SPIR-V variant로 생성해 A/B pipeline에서 선택한다.

- 분류: **Architecture**
- Status: **Accepted (variant 생성 및 pipeline 연결 구현 완료, 성능·화질 A/B 검증 대기)**
- Date: 2026-10-07
- Related: [[04_Development/0003_Solver-Performance|Solver Performance]], [[03_Architecture/0008_Rendering|Surface State Rendering]], [[03_Architecture/0009_UI-Interface|UI Interface]], [[05_Decisions/0025_RawFlux-Cache-Removal|Decision 0025 — RawFlux 캐시 이력]]

## Context

현재 GPU A/B 옵션은 Solver의 Raw Flux 저장 유무·저장 배치·FP16 정밀도와 TransferWeight 정밀도, 그리고 렌더링의 smoothing·State texture sampling 경로를 런타임 값으로 선택한다. 일부 선택은 Pass 1/2의 이웃 루프와 데이터 접근 helper 안에서 반복된다. 비교할 두 구현을 동일 장면과 초기 상태에서 실행하면서 shader hot path에는 비교용 분기와 양쪽 구현 코드가 함께 남는다.

Raw Flux 캐시와 그 비교 경로는 Decision 0025에서 제거됐으나, 현재 구현에서는 GPU 성능 측정을 위해 cache/layout/precision 비교 선택이 다시 존재한다. 이 결정은 그 비교 경로들을 유효한 compile-time variant로 관리하는 현재 방침을 정한다. 결과가 측정되기 전에는 어느 알고리즘을 최종 production 경로로 채택할지 정하지 않는다.

## Decision

1. GPU A/B 실험의 알고리즘 선택은 compile-time macro로 고정한 별도 SPIR-V 파일로 생성한다. 측정 중 활성 draw/dispatch 경로에서는 해당 variant의 `VkPipeline`을 선택한다. 비교 알고리즘을 셰이더 hot loop의 런타임 `if`로 합치지 않는다.
2. 유효하고 중복되지 않는 variant는 build configuration 하나당 총 43개다. 이는 아래 대상 파일의 산출물 수이며, 프로그램 전체 셰이더 파일 수나 전체 runtime configuration의 단순 곱이 아니다.
3. Solver variant는 다음과 같이 생성한다.
   - `SurfaceSolverPass1.comp`: Raw Flux cache OFF 1종, cache ON의 layout 2종 × precision 2종, 그리고 FP16 Weights ON/OFF를 조합해 10종.
   - `SurfaceSolverPass2.comp`: 위와 동일하게 10종.
   - `SurfaceDynamicWeightsUpdate.comp`: FP16 Weights ON/OFF 2종.
4. Rendering variant는 다음과 같이 생성한다.
   - `HeightFieldSmoothing.comp`: 기존 경로와 shared-memory sparse halo 경로 2종.
   - `OverlayCoverageSmoothing.comp`: 기존 경로와 shared-memory sparse tile halo 경로 2종.
   - `RenderStateTextureSmoothing.comp`: direct 5×5 1종, horizontal pass 기존/halo 2종, vertical pass 기존/halo 2종으로 5종.
   - `SurfaceLit.frag`, `BaseSurfaceLit.frag`, `TexelSurfaceLit.frag`, `OverlayCombined.frag`: Coverage smoothing의 precompute가 꺼진 상태의 Texture/SSBO sampling 2종과 precompute 상태의 Texture sampling 1종씩, 각 3종.
5. Precompute Coverage Smoothing이 활성화되면 Render State Texture sampling을 강제한다. 이때 `Render State Texture Sampling` A/B 선택은 UI에서 비활성화하고, 툴팁에 비교할 수 없는 이유를 표시한다.
6. Debug와 Release는 기존 `MDSS_GPU_VALIDATION` 값에 따라 서로 다른 결과물을 사용한다. 따라서 43개는 각 build configuration에 생성하며, Debug와 Release 양쪽을 모두 빌드한 출력 파일 수는 86개다.
7. 이 결정은 최종 성능 승자를 정하지 않는다. 각 A/B 쌍은 같은 GPU·Scene·해상도·State·입력·warm-up·측정 구간으로 비교하고, GPU timestamp와 전체 frame time 및 시각/수치 정확성을 함께 확인한다.

## Alternatives Considered

- **한 shader에 런타임 `if`로 두 구현 유지:** 현재 push constant와 Solver flag로 여러 경로를 선택하는 구현이 근거다. 이 방식은 한 pipeline으로 runtime 설정을 바꿀 수 있지만, A/B 구현을 같은 shader에 보유하며 반복 경로에서 조건을 평가한다.
- **한 번에 한 variant만 빌드해 교체:** variant 저장 공간과 pipeline 수를 줄이는 방법이다. 기존 benchmark 기록에서 변경 전·후 pipeline을 같은 실행에서 준비해 비교한 전례가 있으므로, 동시에 측정할 variant는 build 산출물로 미리 제공하는 쪽을 채택한다.
- **모든 조합의 Cartesian product를 생성:** 옵션 간 비활성/강제 조합도 파일로 만든다. Raw Flux OFF에서 layout·precision은 효과가 없고, precompute smoothing ON에서 Texture/SSBO 선택은 강제되므로 중복 variant를 생성하지 않는다.

## Consequences

- 대상 10개 shader entrypoint를 각각의 variant 산출물로 대체해 build configuration마다 대상 SPIR-V 43개를 생성한다. Debug 빌드의 43개 실측 합계는 1,759,924 byte(약 1.68 MiB)다. 이 수치는 디스크 파일 크기이며 Vulkan driver의 `VkPipeline` 메모리 사용량은 포함하지 않는다.
- 각 측정은 같은 process에서 비교 pipeline을 미리 생성하고 초기화 비용을 warm-up 밖에서 제외할 수 있다. 실제 pipeline cache/driver 메모리 소비량은 장치·드라이버에서 별도 확인한다.
- Raw Flux cache는 Decision 0025 이후 A/B 후보 경로로 다시 존재한다. 성능 측정 결과로 최종 선택을 내린 뒤 해당 후보와 UI를 유지할지 제거할지 후속 결정한다.
- shared-memory halo variant는 sparse/tile 경로에 적용한다. full update fallback과 tile boundary의 데이터 유효성·chart/Profile 경계 규칙을 보존한다.
- State texture precompute와 separable smoothing은 표시 결과가 달라질 수 있다. 특히 경계 normalization 차이는 이미지 비교로 검토한다.

## Implementation and Validation

- CMake가 Debug/Release 출력 경로에 43개 variant를 생성하고, Solver/Rendering pipeline은 현재 A/B 설정에 따라 대응 파일을 선택한다. Shared halo A/B 토글은 초기값 OFF다.
- `TexelSurfaceLit.frag`의 3개 variant도 산출물 수에 포함된다. 현재 활성 draw 경로는 `BaseSurfaceLit.frag`를 사용하므로 이 3개에는 활성 `VkPipeline`이 없다.
- Height halo는 sparse 8×8 microtile마다 10×10 입력을, Overlay Coverage halo는 sparse tile마다 2 texel 테두리를, Render State Texture separable pass는 축별 2 texel 테두리를 shared memory에 올린다. full update와 direct 5×5는 기존 경로를 사용한다.
- 2026-10-07: Debug/Release `MDSS_Shaders`에서 각각 대상 43개를 생성했다. 출력 전체는 Debug 78개, Release 71개 SPIR-V다. Debug `MDSS` 전체 빌드와 BrickCube.Scene 기본값 2 frame 실행, 세 shared halo를 일시적으로 모두 ON으로 둔 5 frame 실행을 통과했다. 최종 토글 초기값은 OFF다. Shared halo ON/OFF의 수치·시각 동등성과 GPU 성능 비교는 아직 수행하지 않았다.

## Related

- [[05_Decisions/0025_RawFlux-Cache-Removal|Decision 0025 — RawFlux 캐시 초기 제거 결정]]
- [[04_Development/0003_Solver-Performance|Solver Performance]]
- [[03_Architecture/0008_Rendering|Surface State Rendering]]
- [[03_Architecture/0009_UI-Interface|UI Interface]]
