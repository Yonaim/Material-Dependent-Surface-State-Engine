# Source — C++ · Shader Entry Map

이 위치는 계산 정의가 아니라 **실제 코드 위치와 변경 영향**을 찾는 지도다. [[02_Architecture/00_Flows/00_Main-Flow|Flows]]에서 실행 시점을, [[02_Architecture/01_Contracts/01_State-and-Profile|Contracts]]에서 의미와 ABI를 확정한 뒤 소스 파일을 따라간다.

## 시작 지점

- [[02_Architecture/02_Source/00_CPP/00_Index|Src_Cpp — C++ 소유권과 진입점]]
- [[02_Architecture/02_Source/01_Shaders/00_Index|Src_Shaders — GPU entry / include]]

## CPU ↔ GPU 연결 경계

| C++ 기록·자원 책임 | Shader 단계 | 검사할 연결 조건 |
|---|---|---|
| `SurfaceStepRecording.cpp`, `SurfaceStateSolver.cpp` | `SurfaceSolverPass1.comp` → `SurfaceSolverPass2.comp` → 선택적 `SurfaceReaction.comp` | Current/Next A/B, RawFlux·Reaction scratch, barrier, push constant |
| `SurfaceContactApplication.cpp` | Pass 2의 `InputDelta` 소비 | 처음 실행되는 step, Contact 중복 반영 여부 |
| `SurfaceGeometryUpdate.cpp`, Solver 기록 | `SurfaceAccumulationHeight.comp` 및 Dynamic Geometry/Weight | State 기반 형상 ON/OFF, Current 재구축과 Next 갱신 |
| `RenderStateTexture.cpp` | `RenderStateTexture.comp` | state channel lookup, render 전용 texture binding |
| `TexelGeometryPreview.cpp`, `AccumulationOverlaySides.cpp` | Overlay Geometry/Coverage compute | dirty tile·top/side indirect command, render-only smoothing |
| `RendererCommands.cpp` | Scene/Surface/Overlay fragment·vertex | compute→graphics barrier, 현재 State descriptor, draw 순서 |
| `SurfaceGPUAbi.h`, packing/descriptor | `SurfaceTypes.glsl`, `SurfaceStateData.glsl` | record stride/alignment, binding, CPU↔GLSL layout |

## 증상별 첫 탐색

| 증상 | 먼저 읽을 Source / Flow |
|---|---|
| Contact가 입력되지 않음 | [[02_Architecture/02_Source/00_CPP/00_SurfaceState/03_Contact-and-State|Contact Source]] → [[02_Architecture/00_Flows/01_Frame/01_Contact-to-InputDelta|Contact Flow]] |
| Flux 이동/Decay가 이상함 | [[02_Architecture/02_Source/01_Shaders/00_Simulation/01_Solver-Passes|Shader Passes]] → [[02_Architecture/02_Source/01_Shaders/00_Simulation/02_Transport-and-Scheduling|Transport]] |
| Geometry Feedback이 한 step 늦거나 다름 | [[02_Architecture/00_Flows/02_Simulation/04_Accumulation-and-Geometry-Feedback|Feedback Flow]] → [[02_Architecture/02_Source/01_Shaders/00_Simulation/03_Dynamic-Geometry|Dynamic Geometry]] |
| Source C++ / Shader의 struct·binding mismatch | [[02_Architecture/02_Source/00_CPP/01_GPU/02_ABI-and-Packing|ABI/Packing]] → [[02_Architecture/02_Source/01_Shaders/04_Debug-and-Shared|Shared Shader]] |
| State는 맞는데 화면이 다름 | [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Render Flow]] → [[02_Architecture/02_Source/01_Shaders/01_Overlay/00_Index|Overlay]] |
| Instance 교체/재설정 뒤 문제 | [[02_Architecture/02_Source/00_CPP/01_GPU/03_Resources-and-Descriptors|Resource Lifecycle]] → [[02_Architecture/02_Source/00_CPP/01_Application-and-Frame|Frame]] |

## Source 기록 규칙

각 소스 페이지에는 **책임 / 진입점·주요 함수 / 입력·출력 / 소유·수명 / CPU↔Shader 연결 / barrier와 variants / 관련 검증 / 변경 영향**을 필요 범위에서 기록한다. 확인하지 않은 함수 호출이나 구현 결과를 추측해 채우지 않는다. 테스트·성능 결과는 각각 [[05_Experiments/01_Validation-and-Tests|Validation]]과 [[05_Experiments/02_Performance-Measurements|Performance]]에서 관리한다.

## C++/Shader 실제 파일 대조

- [[02_Architecture/02_Source/01_Code-Crosscheck|소스 교차검증 결과와 확인 범위]] — 실제 코드 호출 경계·구현상 주의·미제공 파일을 구분한다.
- [[02_Architecture/02_Source/00_CPP/00_SurfaceState/04_Cache-and-Lifecycle|누락되었던 CPU Surface Cache]] — `.Surface` fingerprint, version, rebuild fallback.

이 Source 지도는 코드 위치와 연결을 설명한다. 빌드·GPU 실행·렌더·테스트 결과는 [[05_Experiments/01_Validation-and-Tests|Validation]]에서 확인한다.

## Universal v5.8 가독성 이관 (2026-11-10)

상위 Source 경로·진입점·소유권과 CPU–GPU 연결은 유지했다. 공백/조건 서식 및 선택적인 하위 Region만 정리했고, C++ MSVC Region 라벨 인용을 보완했다. Contracts·Flows 검토 결과 API/ABI·데이터·실행 순서의 새 정의가 없어 본문을 재작성하지 않았다. 파일별 적용 범위와 검증은 [v5.8 적용표](../../04_Development/13_Vault-5.8-Readability-Coverage.md)를 따른다.
