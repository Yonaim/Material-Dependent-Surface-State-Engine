# Overlay — Geometry / Coverage

Top/Side Vertex 및 Indirect Command를 만들고 dirty tile·coverage를 갱신하는 entry와 include를 기록한다.

**연결 문서**
- [[02_Architecture/01_Flows/04_Rendering/02_Texel-Overlay-Preparation|Render Preparation]]
- [[02_Architecture/03_Source/Src_Shaders/03_Overlay/02_Overlay-Shading|Overlay Shading]]

## Geometry compute와 vertex input

| 파일 | 주요 코드 | 입력→출력/다음 단계 |
|---|---|---|
| [TexelGeometry.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp) | `main`, debug view/height geometry | static texel mesh+accumulation height→computed top geometry vertex buffer |
| [TexelGeometryBaseline.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryBaseline.comp) | `main`, geometry cache write | CPU baseline vertex의 static geometry를 GPU cache와 computed vertex layout에 초기화 |
| [TexelGeometryHeight.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryHeight.comp) | `main`, cache update | dynamic accumulation height 변화가 있는 texel의 cached height/normal을 갱신 |
| [TexelGeometry.vert](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelGeometry.vert) | manifest vertex entry | `TexelMeshVertex.glsl` 공통 vertex 구현을 독립 entry로 빌드 |
| [TexelGeometryData.glsl](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelGeometryData.glsl) | `TTexelGeometryVertex` | computed geometry storage buffer의 position/normal·height field 표현 |
| [TexelMeshVertex.glsl](../../../../../../Shaders/Rendering/Overlay/Geometry/TexelMeshVertex.glsl) | vertex `main`, vertex/sample buffers | computed vertex와 per-vertex texel sample을 읽어 overlay top vertex를 raster 위치로 변환 |
| [OverlayDrawReset.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/OverlayDrawReset.comp) | indirect command structs/reset | frame에서 재사용할 top/side indirect draw command count를 초기화 |
| [OverlayTopCommand.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/OverlayTopCommand.comp) | `main`, top/side command merge | side/top draw command 결과를 top draw command buffer로 결합 |
| [OverlaySideBoundary.comp](../../../../../../Shaders/Rendering/Overlay/Geometry/OverlaySideBoundary.comp) | boundary texel/edge expansion | computed top geometry에서 외곽 edge를 찾아 overlay side vertex/segment를 생성 |
| [OverlayTop.vert](../../../../../../Shaders/Rendering/Overlay/Geometry/OverlayTop.vert) | `TexelMeshVertex.glsl` include | accumulation overlay의 top mesh raster vertex entry |
| [OverlaySide.vert](../../../../../../Shaders/Rendering/Overlay/Geometry/OverlaySide.vert) | `TSideSegment` vertex input | side segment의 base/top edge와 normal로 옆면을 rasterize |

## Coverage·scheduling

| 파일 | 주요 코드 | 입력→출력/다음 단계 |
|---|---|---|
| [OverlayStateSampling.glsl](../../../../../../Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl) | profile/state/area bindings, `OverlayStateSaturation` | State/profile을 overlay pass가 읽는 공통 binding과 coverage saturation helper |
| [OverlayOccupancyScan.comp](../../../../../../Shaders/Rendering/Overlay/Coverage/OverlayOccupancyScan.comp) | `ResolveCandidateTile`, `CoverageSignal`, `main` | State/height/cache 변화 검사→height/normal/coverage dirty tile 목록과 indirect dispatch count |
| [OverlaySideCoverage.comp](../../../../../../Shaders/Rendering/Overlay/Coverage/OverlaySideCoverage.comp) | `CoverageListBase`, `main` | surface State와 boundary geometry→side coverage buffer 및 draw command |
| [OverlayCoverageSmoothing.comp](../../../../../../Shaders/Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp) | `LoadCoverageSaturation`, `main` | neighbor/chart-aware coverage smoothing 결과와 관련 top/side command data |


## Dispatch와 variant 확인

`TexelGeometry.comp`, `TexelGeometryBaseline.comp`, `TexelGeometryHeight.comp`는 각각 다른 용도다. C++의 어떤 variant를 dispatch하는지는 `TexelGeometryPreview.cpp`와 `RendererCommands.cpp`에서 확인한다. 실제 manifest는 `CMake/ShaderManifest.cmake`를 확인한다.

## GPU Overlay preparation 확인

- [TTexelGeometryPreview::Record](../../../../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp): view mode 또는 combined overlay에 따라 baseline/height geometry 및 tile activity를 준비한다.
- [TAccumulationOverlaySides::Record](../../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.cpp): coverage 및 boundary 정보로 top/side draw용 자료를 만든다.
- 호출은 [TRenderer::RecordCommandBuffer](../../../../../../Source/Rendering/Core/RendererCommands.cpp)의 Lit 조건, `CanRenderLitOverlays`, Shared mesh/descriptor availability에 종속된다. `CombinedSurfaceKey = 0xffffU`는 Registry State channel ID가 아니라 derived geometry 식별용 예약 값이다.
- Overlay coverage/height smoothing은 **표시 전용 결과**다. Solver의 `DynamicGeometry`와 구별하고 visual artifact만 관찰된 경우 Render branch부터 추적한다.
