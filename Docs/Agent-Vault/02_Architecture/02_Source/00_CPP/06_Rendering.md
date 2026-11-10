# Rendering

`FRenderer`는 Scene, Surface State, GPU context를 읽어 프레임의 compute/display command를 기록한다. 보조 클래스는 frame synchronization, material 준비, state texture 변환, height smoothing, texel overlay mesh·side geometry, profiling을 각각 맡는다. 렌더 표시 변환은 Solver State를 다시 쓰지 않는다.

## Renderer와 frame 자원

| 파일 | 주요 코드 | 역할·연결 |
|---|---|---|
| [RenderContext.h](../../../../../Source/Rendering/Core/RenderContext.h) | `FRenderContext` | 현재 frame slot, fence, frame advance API |
| [RenderContext.cpp](../../../../../Source/Rendering/Core/RenderContext.cpp) | wait/reset/advance | CPU frame과 GPU 완료 상태를 동기화 |
| [Renderer.h](../../../../../Source/Rendering/Core/Renderer.h) | `FRenderer`, options, output/resource declarations | public setting API와 내부 render resource 소유 |
| [Renderer.cpp](../../../../../Source/Rendering/Core/Renderer.cpp) | init/destroy/frame entry | render pipeline과 swapchain dependent resources lifecycle |
| [RendererCommands.cpp](../../../../../Source/Rendering/Core/RendererCommands.cpp) | `RecordCommandBuffer` 주변 pass 기록 | state texture/height/overlay compute와 scene draw 순서 기록 |
| [RendererSettings.cpp](../../../../../Source/Rendering/Core/RendererSettings.cpp) | renderer option 적용·cache 재구성 | UI에서 변경한 sampling/smoothing/culling 옵션을 pipeline·resource에 전달 |
| [RenderSettings.h](../../../../../Source/Rendering/Core/RenderSettings.h) | render/debug mode와 sampling settings | renderer와 DebugUI가 공유하는 표시 option 형식 |
| [RendererShared.h](../../../../../Source/Rendering/Core/RendererShared.h) | descriptor/push constant·vertex helper·`TScopeExit` | 여러 rendering translation unit에서 쓰는 공통 pipeline 자료와 작은 helper |
| [RendererMaterials.cpp](../../../../../Source/Rendering/Scene/RendererMaterials.cpp) | material descriptor/pipeline preparation | Scene mesh material과 texture asset을 draw resource로 연결 |
| [FrustumCulling.h](../../../../../Source/Rendering/Scene/FrustumCulling.h) | `FViewFrustum` 및 visibility helper | camera view에 포함되는 object/tile 판별 자료 |

## Surface state 표시 변환

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [RenderStateTexture.h](../../../../../Source/Rendering/Surface/RenderStateTexture.h) | `FRenderStateTexture` | State A/B를 fragment sampling용 texture로 바꾸는 GPU resource/API |
| [RenderStateTexture.cpp](../../../../../Source/Rendering/Surface/RenderStateTexture.cpp) | `RenderStateTexture.comp` dispatch·image barrier | solver buffer 상태를 render texture로 변환 |
| [HeightFieldSmoothing.h](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.h) | `FHeightFieldSmoothing::Record`, output lifecycle | 동적 height geometry 표시용 smoothing API |
| [HeightFieldSmoothing.cpp](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.cpp) | variant pipeline·smoothing dispatch | height field를 화면용으로 smoothing하며 Solver geometry/state를 변경하지 않음 |
| [DemoSurfaceEffects.h](../../../../../Source/Rendering/Surface/DemoSurfaceEffects.h) | demo state color/effect mappings | Surface State 채널과 데모 shader appearance 연결 설정 |

## Overlay geometry·benchmark

| 파일 | 주요 코드 | 역할·데이터 흐름 |
|---|---|---|
| [AccumulationOverlaySides.h](../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.h) | `FAccumulationOverlaySides` API | texel boundary의 overlay side line/segment 생성 resource |
| [AccumulationOverlaySides.cpp](../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.cpp) | boundary upload·draw preparation | GPU shared geometry의 boundary 정보를 overlay draw에 연결 |
| [TexelGeometryPreview.h](../../../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h) | `FTexelGeometryPreview`, `Record` | top geometry·tile activity·draw resources의 생성/업데이트 API |
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

[FRenderer::RecordCommandBuffer](../../../../../Source/Rendering/Core/RendererCommands.cpp) 기준으로:

- `FrameCoordinator.RecordSimulation` → 선택적 `TexelInspector->Record` → ViewMode 평가 → 필요 시 `RenderStateTexture->Record` → 선택적 TexelGeometry/Overlay preparation → RenderPass 시작 순서다.
- `bUseRenderStateTexture = bRenderStateTextureSamplingEnabled || bPrecomputeCoverage`, `bPrecomputeCoverage = DemoEffects.bCoverageSmoothing && bPrecomputeCoverageSmoothingEnabled`. 전환에 따라 `FragmentMode = 0/1/2`가 달라진다.
- Overlay는 Lit view와 `CanRenderLitOverlays()`가 모두 유효해야 한다. instance마다 Shared Geometry/Descriptors/Texel mesh index buffer가 없으면 skip한다. `Mud`, `WaterFilm`, `Lava` channel이 활성일 때 `CombinedSurfaceKey = 0xffffU`로 통합 top surface를 준비한다.
- 화면용 `HeightFieldSmoothing`, coverage smoothing, Render State Texture smoothing은 현재 State buffer를 다시 기록하는 Solver 작업이 아니다. Solver의 geometry feedback과 동일한 개념으로 합치지 않는다.
- query timestamp는 `RendererCommands.cpp`와 [RendererBenchmark.cpp](../../../../../Source/Rendering/Profiling/RendererBenchmark.cpp)에 걸친다. `FrameCoordinator.GetStepSerial`은 Inspector와 연결되므로 readback 결과를 마지막 완료 프레임과 혼동하지 않는다.


## Command recording의 책임 경계 — 2026-11-09

`RecordCommandBuffer`는 `BeginCommandRecording → RecordSimulationAndInspector → PrepareDisplayResources → BeginSceneGraphicsPass → RecordSceneViewports → RecordUIAndEndCommandBuffer` 순서다. `RecordSceneViewports` 안에서는 viewport 설정 → base mesh → accumulation overlay → world reference/gizmo를 호출한다. 각 단계의 기존 조건·query offset·descriptor 선택·barrier/command 인자는 그대로 유지한다.

private `FCommandRecordingState`는 호출 내 frame/query/display/overlay 값을 함께 전달한다. 기존 vector 저장소를 move해 draw 단계까지 유지하며 Renderer의 장기 멤버 자원을 대체하지 않는다. `FViewportRecordingState`는 view mode·행렬/frustum·material offset과 기록 동안 유효한 camera 참조를 전달한다. 타입은 CPU–GPU ABI 자료형이 아니다.

Vulkan 호출 지점 81개 정적 대조와 render-only 5프레임 old/new 캡처 비교를 수행했다. simulation/overlay 앱 경로는 양쪽 버전 모두 기존 GPU storage buffer 한도로 초기화되지 않았으므로 해당 런타임 동등성을 통과로 기록하지 않는다. [검증 기록](../../../05_Experiments/01_Validation-and-Tests.md)을 따른다.

## 5.5 책임 경계 보완

`FBenchmarkCaptureState`는 opt-in `AuthoringScreenshot`·checkpoint serial 목록·마지막 캡처 serial도 소유한다. `ConfigureBenchmarkCapture`의 Scene 참조는 Application 소유 객체를 빌려 초기 입력 observer에 전달한다. `RecordBenchmarkScreenshot`은 지정 step 이미지를 기록하고 `RenderFrame`은 다음 frame 전에 `WriteAuthoringCheckpoint`를 호출해 State/geometry와 함께 저장한다. 기존 일반 benchmark 최종 캡처 경로도 유지한다. 조건과 출력 시점은 Simulation Step Overview의 연구용 캡처 설명을 따른다.

`WriteAuthoringCheckpoint`는 GPU 완료 대기 → 출력 중복 검사 → instance 저장 → 필요 시 이미지 저장·캡처 상태 갱신 → metadata 저장을 조율한다. 같은 translation unit의 익명 namespace 안에 버퍼 출력·instance 저장·Reaction 진단·이미지 변환·metadata 출력 함수를 분리했다. API·소유 상태·buffer 읽기 순서·GPU 대기 위치와 JSON/파일 형식은 유지한다. 파일의 최상위는 실행 흐름과 저장 세부 구현, 하위는 관련 책임으로 구획하며 Region 총 깊이는 2다.

- `Renderer.h::FBenchmarkCaptureState`: stream/output 경로·in-flight submission·capture buffer·frame counters를 값 소유한다. 원래 초기값/생성/마무리/flush·파괴 순서를 보존하고 timestamp pool/프레임 자원은 Renderer에 유지한다.
- [RendererTiming.cpp](../../../../../Source/Rendering/Profiling/RendererTiming.cpp): `CollectCompletedFrameTimings`는 frame fence 이후의 query 회수·통계 갱신·benchmark sample 기록을 담당한다. RenderFrame의 호출 지점과 query index/수식·실패 정책은 동일하다.
- [WorldReferenceGeometry.h](../../../../../Source/Rendering/Scene/WorldReferenceGeometry.h): 내부 grid/축/gizmo record·정점 생성을 material/attachment helper에서 분리했다. 정점 필드/순서·pipeline layout을 유지하며 같은 translation unit의 내부 helper로 사용한다.
- 나머지 swapchain/attachment/descriptor·overlay 자원은 공유 frame/device 수명과 생성/종료 경계가 있어 기존 pass별 소유 클래스와 command helper를 유지한다.

## 상위 진입 함수에서 내려가기

| 상위 함수 | 책임과 주요 하위 단계 |
|---|---|
| `FRenderer::RenderFrame` | 이전 프레임 정리 → 현재 프레임 준비·이미지 획득 → 명령 기록 → 제출·표시 |
| `FRenderer::RecordCommandBuffer` | 시뮬레이션 → 표시 자원 준비 → 그래픽 패스 → Scene Viewport → UI |
| `FRenderer::PrepareDisplayResources` | Instance별 상태 텍스처·Overlay·Geometry 표시 자원 준비 |
| `FRenderer::ReloadSceneResources` | 이전 작업 동기화 → 새 Scene 자원 준비 → 리소스 교체 → 실행 설정 갱신 |

`ReloadSceneResources` 아래의 `BuildSceneReplacement`, `BuildSceneRenderPipelines`, `CommitSceneReplacement`는 **준비 단계와 소유권 이전의 경계**를 드러낸다. `FSceneReplacementResources`는 교체 준비 중의 GPU 자원을 임시 보유한다. 예외·Rollback 경로와 Descriptor 수명은 실제 구현에서 확인해야 한다. GPU 패스·Barrier 순서는 [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Rendering Flow]]를 따른다.

## Renderer 진입 코드와 세부 자원 생성

`FRenderer` 생성자는 `ConfigureDeviceCapabilities` → `InitializeReferenceGizmo` → `CreateMaterialDescriptorResources` → `InitializeSurfaceDisplayResources` → `InitializeFrameTimingResources` → `ConfigureSurfaceResolution`의 흐름이다. 이보다 먼저 실행되는 멤버 초기화 목록은 C++ 선언 순서에 종속된다. 생성 실패 정리는 `ReleaseRawResourcesAfterFailedConstruction`에 모았고, 원시 Vulkan 핸들 정리와 RAII 멤버 정리는 구분한다. 실제 프레임 렌더링의 명령·동기화 순서는 `RenderFrame` → `RecordCommandBuffer`와 해당 단계 함수에서 확인한다.

## Render State Texture 생성 경계

`FRenderStateTexture` 생성자는 `ValidateRenderStateTextureSupport` → `CreateDescriptorAndPipelineResources` → `CreateInstanceRenderTextures`로 읽는다. GPU 지원 가능 형식을 먼저 검사하고, 공통 Descriptor·Compute Pipeline을 구성한 다음 Surface instance별 image array와 descriptor를 채운다. 생성 도중 발생한 예외는 기존과 동일하게 `Destroy()`로 이미 생성한 Vulkan 핸들을 정리한 뒤 전파한다.
