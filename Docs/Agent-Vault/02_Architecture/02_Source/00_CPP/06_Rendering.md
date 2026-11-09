# Rendering

`TRenderer`는 Scene, Surface State, GPU context를 읽어 프레임의 compute/display command를 기록한다. 보조 클래스는 frame synchronization, material 준비, state texture 변환, height smoothing, texel overlay mesh·side geometry, profiling을 각각 맡는다. 렌더 표시 변환은 Solver State를 다시 쓰지 않는다.

## Renderer와 frame 자원

| 파일 | 주요 코드 | 역할·연결 |
|---|---|---|
| [RenderContext.h](../../../../../Source/Rendering/Core/RenderContext.h) | `TRenderContext` | 현재 frame slot, fence, frame advance API |
| [RenderContext.cpp](../../../../../Source/Rendering/Core/RenderContext.cpp) | wait/reset/advance | CPU frame과 GPU 완료 상태를 동기화 |
| [Renderer.h](../../../../../Source/Rendering/Core/Renderer.h) | `TRenderer`, options, output/resource declarations | public setting API와 내부 render resource 소유 |
| [Renderer.cpp](../../../../../Source/Rendering/Core/Renderer.cpp) | init/destroy/frame entry | render pipeline과 swapchain dependent resources lifecycle |
| [RendererCommands.cpp](../../../../../Source/Rendering/Core/RendererCommands.cpp) | `RecordCommandBuffer` 주변 pass 기록 | state texture/height/overlay compute와 scene draw 순서 기록 |
| [RendererSettings.cpp](../../../../../Source/Rendering/Core/RendererSettings.cpp) | renderer option 적용·cache 재구성 | UI에서 변경한 sampling/smoothing/culling 옵션을 pipeline·resource에 전달 |
| [RenderSettings.h](../../../../../Source/Rendering/Core/RenderSettings.h) | render/debug mode와 sampling settings | renderer와 DebugUI가 공유하는 표시 option 형식 |
| [RendererShared.h](../../../../../Source/Rendering/Core/RendererShared.h) | descriptor/push constant·vertex helper·`TScopeExit` | 여러 rendering translation unit에서 쓰는 공통 pipeline 자료와 작은 helper |
| [RendererMaterials.cpp](../../../../../Source/Rendering/Scene/RendererMaterials.cpp) | material descriptor/pipeline preparation | Scene mesh material과 texture asset을 draw resource로 연결 |
| [FrustumCulling.h](../../../../../Source/Rendering/Scene/FrustumCulling.h) | `TViewFrustum` 및 visibility helper | camera view에 포함되는 object/tile 판별 자료 |

## Surface state 표시 변환

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [RenderStateTexture.h](../../../../../Source/Rendering/Surface/RenderStateTexture.h) | `TRenderStateTexture` | State A/B를 fragment sampling용 texture로 바꾸는 GPU resource/API |
| [RenderStateTexture.cpp](../../../../../Source/Rendering/Surface/RenderStateTexture.cpp) | `RenderStateTexture.comp` dispatch·image barrier | solver buffer 상태를 render texture로 변환 |
| [HeightFieldSmoothing.h](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.h) | `THeightFieldSmoothing::Record`, output lifecycle | 동적 height geometry 표시용 smoothing API |
| [HeightFieldSmoothing.cpp](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.cpp) | variant pipeline·smoothing dispatch | height field를 화면용으로 smoothing하며 Solver geometry/state를 변경하지 않음 |
| [DemoSurfaceEffects.h](../../../../../Source/Rendering/Surface/DemoSurfaceEffects.h) | demo state color/effect mappings | Surface State 채널과 데모 shader appearance 연결 설정 |

## Overlay geometry·benchmark

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [AccumulationOverlaySides.h](../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.h) | `TAccumulationOverlaySides` API | texel boundary의 overlay side line/segment 생성 resource |
| [AccumulationOverlaySides.cpp](../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.cpp) | boundary upload·draw preparation | GPU shared geometry의 boundary 정보를 overlay draw에 연결 |
| [TexelGeometryPreview.h](../../../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h) | `TTexelGeometryPreview`, `Record` | top geometry·tile activity·draw resources의 생성/업데이트 API |
| [TexelGeometryPreview.cpp](../../../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp) | baseline/height geometry dispatch, smoothing·tile draw prep | CPU render mesh를 GPU texel geometry로 변환하고 overlay top command 입력 준비 |
| [RendererBenchmark.cpp](../../../../../Source/Rendering/Profiling/RendererBenchmark.cpp) | frame query/sample serialization | renderer GPU timestamp와 benchmark frame data를 결과 파일로 기록 |

셰이더별 producer/consumer 관계는 [[02_Architecture/02_Source/01_Shaders/02_Surface-Rendering|Surface Rendering]], [[02_Architecture/02_Source/01_Shaders/01_Overlay/00_Index|Overlay Shaders]]에서 파일 단위로 본다. 측정 절차와 해석은 [[05_Experiments/02_Performance-Measurements|Performance Measurements]]에 둔다.

## `RecordCommandBuffer`에서 찾아야 할 경계

[`RendererCommands.cpp`](../../../../../Source/Rendering/Core/RendererCommands.cpp)는 `FrameCoordinator.RecordSimulation`을 먼저 기록한 다음 Inspector, State texture, texel geometry와 overlay compute, graphics pass를 기록한다. view mode를 검사해 geometry debug와 Lit overlay가 실제 필요한 때만 해당 pass를 준비한다. 각 instance의 `IsCurrentStateAB`를 다시 읽어 solver 이후의 descriptor를 선택한다.

| 코드 묶음 | 읽는 값 | 만드는 값·다음 사용 |
|---|---|---|
| `RenderStateTexture->Record` | Registry의 demo channel, current State | Lit fragment가 sample할 State texture와 선택적 smoothed texture |
| `TexelGeometryPreview->Record` | 공유 texel mesh, current height, view mode | debug/overlay용 computed vertex·geometry cache |
| `HeightFieldSmoothing->Record` | 표시용 geometry, State별 높이 | 선택적 화면용 smoothed geometry |
| `OverlaySides->Record` | geometry, boundary, State coverage | 공통 top/side index·segment와 indirect draw command |
| base/overlay draw | material, 현재 A/B State, texture, geometry sets | 화면 image와 debug UI 합성 |

Lit overlay가 활성인 경우 renderer는 Mud/WaterFilm/Lava channel을 하나의 `CombinedSurfaceKey`(`0xffff`)로 묶어 top surface를 기록한다. 원본 mesh는 base surface로 유지하고 top은 `vkCmdDrawIndexedIndirect`, side는 `vkCmdDrawIndirect`를 사용한다. `OverlayActive`가 false이거나 공유 mesh/index/descriptor가 없는 instance는 준비를 건너뛴다. geometry debug view는 Lit overlay와 다른 view mode 경로이므로 `TexelGeometryPreview::Record`의 channel·total height 인수를 확인한다.

`RenderStateTexture.cpp`는 buffer State를 RGBA16F image로 바꾸고 필요할 때 chart-aware smoothing을 실행한다. `TexelGeometryPreview.cpp`는 baseline/height cache와 tile 기반 indirect dispatch를 관리한다. `RendererBenchmark.cpp`의 timestamp는 이 stage 경계를 측정하지만 표시에 사용한 옵션·실제 step 수와 함께 해석해야 한다. 화면용 smoothing, color ramp, `LitHeightDisplayScale`은 Solver의 State나 동적 transfer weight를 갱신하지 않는다.

## RendererCommands의 코드상 조건과 State 소비

[TRenderer::RecordCommandBuffer](../../../../../Source/Rendering/Core/RendererCommands.cpp) 기준으로:

- `FrameCoordinator.RecordSimulation` → 선택적 `TexelInspector->Record` → ViewMode 평가 → 필요 시 `RenderStateTexture->Record` → 선택적 TexelGeometry/Overlay preparation → RenderPass 시작 순서다.
- `bUseRenderStateTexture = bRenderStateTextureSamplingEnabled || bPrecomputeCoverage`, `bPrecomputeCoverage = DemoEffects.bCoverageSmoothing && bPrecomputeCoverageSmoothingEnabled`. 전환에 따라 `FragmentMode = 0/1/2`가 달라진다.
- Overlay는 Lit view와 `CanRenderLitOverlays()`가 모두 유효해야 한다. instance마다 Shared Geometry/Descriptors/Texel mesh index buffer가 없으면 skip한다. `Mud`, `WaterFilm`, `Lava` channel이 활성일 때 `CombinedSurfaceKey = 0xffffU`로 통합 top surface를 준비한다.
- 화면용 `HeightFieldSmoothing`, coverage smoothing, Render State Texture smoothing은 현재 State buffer를 다시 기록하는 Solver 작업이 아니다. Solver의 geometry feedback과 동일한 개념으로 합치지 않는다.
- query timestamp는 `RendererCommands.cpp`와 [RendererBenchmark.cpp](../../../../../Source/Rendering/Profiling/RendererBenchmark.cpp)에 걸친다. `FrameCoordinator.GetStepSerial`은 Inspector와 연결되므로 readback 결과를 마지막 완료 프레임과 혼동하지 않는다.
