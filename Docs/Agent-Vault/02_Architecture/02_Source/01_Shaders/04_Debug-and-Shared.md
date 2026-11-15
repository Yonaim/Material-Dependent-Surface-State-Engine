# Debug and Shared Shader Data

이 파일들은 debug rendering과 여러 pass 사이의 GPU 선언 공유를 담당한다. Shared 구조체·binding은 CPU C++ ABI와 함께 바뀌어야 한다. 해당 C++ 선언은 [GPU Resources](../00_CPP/01_GPU/00_Index.md), contract는 [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]]을 본다.

## Debug shaders

| 파일 | 주요 코드 | 입력→출력/연결 |
|---|---|---|
| [SurfaceDebug.vert](../../../../../Shaders/Debug/SurfaceDebug.vert) | vertex `main`, material debug mode | 원본 mesh vertex와 debug option→normal/UV/surface/world-position varyings |
| [SurfaceDebug.frag](../../../../../Shaders/Debug/SurfaceDebug.frag) | flux/area/transfer/state debug bindings, fragment `main` | State·Profile·geometry·solver debug buffer를 읽어 view mode별 heatmap/normal output |
| [SurfaceDebugData.glsl](../../../../../Shaders/Debug/SurfaceDebugData.glsl) | `TDebugAccumulation`, `DebugFinalNormal` | capacity, saturation, area, accumulation geometry 파생량을 모아 debug fragment와 texel inspector가 공유 |
| [TexelInspector.comp](../../../../../Shaders/Debug/TexelInspector.comp) | `main`, `TSnapshot` output buffer | 선택한 texel/channel의 state, profile, 면적, accumulation 계산값을 여섯 vec4 snapshot으로 기록 |
| [Views/SurfaceDebugColors.glsl](../../../../../Shaders/Debug/Views/SurfaceDebugColors.glsl) | `GridLines`, `TexelGridColor`, `TexelAreaColor`, heat/weight colors | debug fragment의 texel grid와 diagnostic scalar color palette 제공 |

## Cross-pass ABI includes

| 파일 | 주요 선언 | 포함 범위·주의 사항 |
|---|---|---|
| [SurfaceTypes.glsl](../../../../../Shaders/Shared/SurfaceState/SurfaceTypes.glsl) | `TSurfaceGPUProfileParameters`, geometry scalar/vector records | 가장 작은 shared record 정의. C++ `SurfaceGPUAbi.h`의 field 순서·size·alignment와 일치해야 함 |
| [SurfaceStateData.glsl](../../../../../Shaders/Shared/SurfaceState/SurfaceStateData.glsl) | neighbor/geometry/profile/state/scratch buffers와 accessors | 공통 descriptor binding, ABI record array, profile/load helpers. include하는 shader는 compile-time feature와 필요한 binding을 함께 확인 |

## 변경 추적

- Debug data의 계산 의미는 [[02_Architecture/01_Contracts/04_Solver|Solver]]와 [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]]을 따른다.
- CPU mirror는 `Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h` 및 `Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h`다.
- `CMake/ShaderManifest.cmake`에는 진입점만 등록한다. 이 페이지에 적힌 `.glsl` include는 독립 SPIR-V entry가 아니다.

## 공통 구조와 readback 연결 검증

- [SurfaceGPUAbi.h](../../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h)의 layout asserts와 [SurfaceTypes.glsl](../../../../../Shaders/Shared/SurfaceState/SurfaceTypes.glsl)의 GLSL struct/flags를 같은 revision에서 비교한다.
- [DescriptorResources.cpp](../../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp)는 두 descriptor set을 가진다. debug/renderer에서 read할 때도 `IsCurrentStateAB`에 맞는 set을 써야 한다.
- [TexelInspector.cpp](../../../../../Source/SurfaceState/Debug/TexelInspector.cpp) ↔ [TexelInspector.comp](../../../../../Shaders/Debug/TexelInspector.comp): Inspector 스냅샷은 GPU readback 경로다. CPU 표시용 metadata만 검사하면 Solver 계산 검증이 되지 않는다.

## 진입점과 세부 구현

- `SurfaceDebug.vert`: 원본 위치·basis 출력을 유지하고 `WriteMesoNormal` → `ApplyMesoOffset`으로 조회/변위를 구분한다. 면적 진단용 world position은 변위 전 위치다.
- `SurfaceDebug.frag`: `ComputeDebugDerivatives`를 모든 fragment별 guard보다 먼저 호출한다. surface/geometry guard와 mode 우선순위는 `main`에 남기고 `Render*` helper가 각 진단 출력·fallback을 담당한다.
- 인터페이스/prototype → main → Detailed Implementation 순서이며 layout과 CPU 바인딩은 유지한다.

## Debug 진입점의 단계 분리

- `SurfaceDebug.frag`는 fragment derivative를 early return 전에 계산하고, `ResolveDebugSurfaceRange` → `RenderUVViewsIfSelected` → `RenderMappedSurfaceView`로 나누어 view mode에 따른 실제 작업은 하위 함수에서 확인한다.
- `TexelInspector.comp`는 `CaptureAccumulationDiagnostics`와 `CaptureReactionDiagnostics`로 별도 스냅샷 슬롯을 채운다. 슬롯 배치와 CPU readback 계약을 변경하지 않는다.
- Region 내부의 설명은 시작 마커 바로 아래 한 줄로 유지하고, 하위 Region의 마커와 본문은 부모보다 들여쓴다.
