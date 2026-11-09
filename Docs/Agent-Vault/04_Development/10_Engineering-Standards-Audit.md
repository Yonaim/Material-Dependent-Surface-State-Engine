# Engineering Standards 준수 점검 — 2026-11-09

> **후속 정책 변경:** 2026-11-09 Universal-Agent-Vault의 JS/TS 선택 프로필 및 전 언어 Region 표기법을 반영했다. GLSL은 `// #region` 주석을 채택하며 Shader folding 적용을 완료했다. 아래 최초 감사의 주석 전용 섹션·미적용 관찰은 당시 상태다. 현재 수행 순서는 [Current Work](00_Current-Work.md), 최신 검증은 [Validation](../05_Experiments/01_Validation-and-Tests.md)을 따른다.

## 결론과 범위

**현재 소스는 `00_Engineering-Standards` 전체를 준수하지 않는다.** 사용자 확정에 따라 목표 C++ 프로필은 **Unreal-Inspired**다. 기존 구현을 설명하는 문서와 앞으로 지켜야 할 규칙을 구분한다. 이번 작업은 정적 점검 및 프로필 문서 동기화이며 소스 리네이밍·자동 포맷은 수행하지 않았다.

- 범위: 현재 체크아웃의 `Source/`, `Tests/`, `Shaders/`, `Scripts/` 및 루트 `run.sh`, 빌드·포맷 설정. IDE에 표시된 `migration/`은 실제 현재 루트에 없어 검사하지 않았다.
- C++: `.cpp` 102개 + `.h` 93개 = 195개(연구용 C++ 포함). GLSL: `.vert` 9개, `.frag` 10개, `.comp` 21개, `.glsl` 29개. Python 44개, Bash 5개. `ThirdParty/`, Assets/Cache/성능 원자료는 소스 스타일 대상에서 제외했다.
- 단순한 선언·이름·주석·섹션 패턴은 전수 검색했다. 함수의 책임·소유권·성능·GPU 동기화는 일부 대표 경로만 수동 검토했으므로 **모든 의미적 규칙을 전수 인증한 결과가 아니다**.
- PowerShell(`Scripts/*.ps1`, 루트 `run.ps1`)과 CMake는 Bash/C++ 규칙을 그대로 강제하지 않았다. CMake의 빌드·셰이더 목록과 포맷 설정은 확인했지만 PowerShell 실행 검증은 하지 않았다. 프로젝트 자체 `.c` 파일은 이번 범위에서 발견되지 않아 C 프로필은 비해당이다.

## 확인된 미준수

| ID | 근거 규칙 | 관찰 및 대표 위치 | 조치 |
|---|---|---|---|
| AUD-001 | C++ §02 B 일반 클래스·구조체 `F`, 템플릿만 `T` | 일반 클래스·구조체 정의 패턴 188건이 모두 `T`로 전환 대상이다. 템플릿은 별도 검색에서 `TScopeExit` 1건 확인. 예: `Source/AssetManager/Assets/MaterialAsset.h:16`의 `TMaterialAsset` | `FMaterialAsset` 등으로 선언·생성자·소멸자·사용처를 함께 전환. 파일명은 유지 |
| AUD-002 | C++ §02 B enum `E` | enum 정의 16건 중 12건이 `T`. 예: `Source/Logger/Logger.h:15`의 `TLogLevel`, `Source/Rendering/Core/RenderSettings.h:6`의 `TOverlayDisplayMode` | enum과 사용처를 `E`로 전환 |
| AUD-003 | C++ §02 B Boolean `b` | `Source/InputSystem/Raycaster.h:24`의 `Hit`, `Source/Application/BenchmarkOptions.h:15`의 `Enabled` 등 | `bHit`, `bEnabled` 등으로 전환. bool 반환 함수·연산자·외부 API는 bool 변수와 구분 |
| AUD-004 | Doxygen §00·Source Comments §05 한국어 설명 | 한글이 없는 Doxygen 설명 태그 행 39건. 예: `Source/InputSystem/Raycaster.h:37`, `Source/Scene/DemoAnimation.h:84` | 설명 문장을 한국어로 수정하되 식별자·단위·호출 계약은 보존 |
| AUD-005 | Project Brief의 포맷터 및 루트 `.clang-format` | clang-format 20.1.8의 `--dry-run --Werror` 검사에서 195개 중 35개 불일치(실행 오류 없음). Source 26개, Tests 4개, Scripts/Research 5개 | 실제 프로젝트에서 사용할 포맷터 버전을 고정한 뒤 관련 범위를 포맷. 기능 변경과 분리 |
| AUD-006 | Python §01 PEP 8·유용한 타입 힌트 | `Scripts/Performance/analyze_history.py:3,8`의 압축 import·1칸 들여쓰기·여러 문장 한 줄, `analyze_suite.py:48` 등의 한 줄 함수. `Scripts/build_midterm_demos.py:19`의 파일/네트워크 입력 경계에 타입 힌트 없음 | 모듈 스타일을 통일하고 중요한 입력·출력 경계부터 타입 명시 |
| AUD-007 | GLSL §04.3 소스 내 CPU 대응·비자명한 계약 주석 | `Shaders/Rendering/Scene/Gizmo.vert:11` 이후 Vertex Input·Push Constant는 선언되어 있지만 CPU 대응 위치와 축 값의 계약이 소스 주석에 없음. 실제 대응은 `Source/Rendering/Core/RendererShared.h`의 `TGizmoVertex`, `TGizmoPushConstants`, `BuildGizmoPipelineConfig` | 필요한 로컬 계약과 CPU 소스 위치를 짧게 추가. 이 관찰은 ABI 불일치 판정이 아님 |
| AUD-008 | Source Comments §01 구현과 일치하는 주석 | `Source/main.cpp:17`은 반환 코드를 0/1로만 설명하지만 인자 오류 catch는 2 반환 | `@return`에 인자 오류 코드 2를 반영 |
| AUD-009 | Shell §01 스크립트 상단의 목적·도구·입출력·실행 위치·파괴적 효과 | `Scripts/build.sh`의 상단에 실행·인자·출력 및 기존 Build 백업 이동에 관한 개요가 없음. `Scripts/format.sh`도 소스 덮어쓰기 범위를 상단에 설명하지 않음 | 필요한 도구, 실행 예시, 인자, 출력/덮어쓰기·이동 효과를 상단에 짧게 명시 |

집계는 텍스트 패턴에 의한 **정의 발생 건수**이며 서로 다른 완전 수식 타입의 개수나 컴파일러 AST 결과가 아니다. 전방 선언과 enum class는 일반 클래스 집계에서 제외했다. 같은 줄에 template 선언이 있는 `TScopeExit`는 일반 정의 패턴에 포함되지 않아 별도 템플릿 검색으로 확인했다. `TScopeExit`는 템플릿이므로 `T`를 유지한다. 타입 별칭(`using TAssetID`, handle 등)은 현재 프로필에 독립적인 명시 규칙이 없으므로 일괄 위반으로 세지 않았다. GLSL의 기존 `T` 명명도 C++ 프로필 위반 수에 포함하지 않았다.

## 규칙별 확인 범위

| 규칙 영역 | 결과·한계 |
|---|---|
| Code Style: 의도·상위 흐름·추상화·부작용·동작 보존 | 부분 수동 검토. `Source/main.cpp`는 명시적인 파싱/실행 오류 분기를 가진다. `TApplication::MainLoop`는 프레임 조율에 접촉 입력·이벤트 좌표 변환·benchmark 처리를 함께 담아 책임 분리 검토가 필요하다. 함수 길이만으로 규칙 위반을 확정하지 않음 |
| Local Numbering / 섹션 / pragma | 섹션이 있는 C++/GLSL 133개에서 파일 내 번호 연속성 오류 미검출. C++ region은 84개 파일에서 시작/끝 개수 일치. 컴파일러별 pragma 지원 및 중첩 의미까지 인증한 결과는 아님. 모든 파일에 region이 있어야 한다는 규칙은 없음 |
| C++ RAII·수명·예외·const·include·성능 | 전수 검증 미수행. 명명/포맷 결과와 분리해야 함. 이름 전환 때 API 소비자·생성 코드·테스트 및 ABI static_assert를 함께 검사할 것 |
| GLSL 버전·layout·접근·선언 순서 | Manifest 기반 컴파일 및 SPIR-V 검증 통과. 기존 프로젝트 명명 우선이므로 예시 snake_case를 강제하지 않음. 권장 main 배치 순서 자체를 강제 위반으로 판정하지 않음 |
| GLSL CPU–GPU ABI·자원 수명·동기화·성능 | 대표 리소스 선언·CPU 대응 및 계약 문서만 확인. Reflection 대조·Pipeline 생성·GPU 실행·경계/경쟁 검증 미수행. `barrier()` 이전 return은 workgroup 균일성을 확인해야 하므로 패턴만으로 결함으로 단정하지 않음 |
| Source Comments / Doxygen | 언어와 일부 실제 계약 불일치 확인. What 반복·폐기된 코드·모든 공개 API 문서의 충분성은 전수 의미 검토 미수행. `@file`은 선택 사항이므로 누락을 일괄 위반으로 세지 않음 |
| Python 오류·자원·재현성 | AST 구문 통과. 반환 annotation 없는 공개 이름 함수/메서드 후보 123건은 추가 검토 목록이며 전부 위반이라는 뜻은 아님. 런타임 의존성·타입 검사·실제 연구/다운로드 실행 미수행 |
| Shell quoting·실패 처리·멱등성 | Bash 5개 구문 통과, 대표 build/format/run 경로 검토. ShellCheck/shfmt 미설치로 미실행. 구문 통과를 삭제·덮어쓰기 안전성의 증거로 쓰지 않음 |
| 문서 구조 / Markdown / Workflow | Project Brief·Constraints의 관찰과 요구사항 불일치 수정. 전체 링크·번호·문서 중복 전수 감사는 미수행. 과거 작업의 문서 열람/검증 수행 여부는 코드만으로 판정할 수 없음 |
| Commit Style / 문서 템플릿 | 이번 작업에 commit 없음. 템플릿 예시를 현재 소스의 필수 파일 구조로 강제하지 않음 |

## 문서 및 설정 불일치

- Project Brief는 기존 `Existing-Project`를 활성 프로필로 설명했으나 사용자 결정은 `Unreal-Inspired`다. **이번 작업에서 목표 프로필과 전환 미완료 상태를 반영했다.**
- 현재 루트에는 `CMakeLists.txt`와 `.clang-format`이 있으며 Git 추적 파일이다. CMake는 C++20과 테스트 타깃을 설정한다. Project Brief·Constraints의 “미포함” 관찰을 수정했다. 파일 존재만으로 전체 빌드 가능하다고 주장하지 않는다.
- 루트 `AGENTS.md`의 ZIP 누락 문구와 과거 Work Log의 누락 설명은 현재 체크아웃과 구분해 해석해야 한다. 이번 감사에서 루트 지침과 과거 작업 기록은 수정하지 않았다.
- `.clang-format:5`는 삭제된 `Docs/MDSSP-Agent-Vault/01_Project-Policy/02_Code_Style.md`를 가리킨다. 현재 Engineering Standards/Project Brief로 참조 갱신이 필요하다.
- `Docs/Agent-Vault/AGENTS.md`의 마지막 링크와 Code Style §03 일부 링크는 표시 이름과 실제 링크 목적지가 다르다. 실제 목적지는 존재하며 표시 이름 정리가 필요하다.
- Python 버전·환경/의존성 관리, 셸 지원 환경, 포맷터 버전 등의 프로젝트 활성 설정은 아직 충분히 확정되어 있지 않다. 범용 언어 문서의 `{{PLACEHOLDER}}` 자체는 템플릿이므로 오류로 단정하지 않는다.

## 실행한 검증과 미실행 범위

- Python 3.13 AST parse: **44/44 통과**. 스크립트를 실행하거나 다운로드·연구 실험을 수행하지 않음.
- `bash -n`: `Scripts/build.sh`, `Scripts/format.sh`, `Scripts/run.sh`, `Scripts/run_performance_benchmarks.sh`, 루트 `run.sh` **5/5 통과**.
- `clang-format --dry-run --Werror <파일>`: clang-format **20.1.8**, C++ **195개 중 160개 통과 / 35개 서식 불일치**. 원본 파일 변경 없음.
- 셰이더: 실제 `CMake/ShaderManifest.cmake`를 CMake script mode로 읽고 `mdss_add_shader_variant` 실행부를 임시 검증 함수로 대체했다. 일반 엔트리와 `mdss_register_shader_variants()`의 정의를 그대로 열거하여 **75개 엔트리·변형 × `MDSS_GPU_VALIDATION=0/1` = 150개**를 `glslc -I Shaders -I Source ...`로 컴파일하고 각각 `spirv-val`로 검사, **150/150 통과**. Manifest에서 제외한 placeholder `Simulation/Input/SurfaceInput.comp`는 컴파일 대상이 아님. 산출물은 저장소 밖 임시 디렉터리에만 생성했다. 이는 실제 전체 CMake configure/build를 실행한 결과가 아님.
- C++ 전체 configure/build/CTest, clang-tidy, Doxygen 생성, ABI Reflection, GPU 실행·성능 회귀는 **미실행**. 이번 범위는 규칙 감사이며 코드 변경이 없다. 이 결과를 전체 동작·성능 또는 모든 규칙의 완전 준수 증거로 사용하지 않는다.

## 권장 전환 순서

1. `F`/`T`/`I`/`E`/`b` 역할을 기준으로 명명 전환. 외부 타입·별칭·GLSL 명명은 독립적으로 검토하고 무차별 `T → F` 치환 금지.
2. 선언·정의·호출자·테스트·생성 코드와 관련 Architecture 문서를 함께 갱신. ABI 레이아웃·셰이더 문자열·에셋 schema key를 이름 변경에 섞지 않음.
3. Doxygen 한국어·실제 계약·셰이더 로컬 계약·Shell 개요 보완.
4. 포맷터 버전 확정 후 포맷, Python 스타일/타입 정리. 필요한 추상화 리팩터링은 별도 작업으로 수행.
5. 변경 타깃 빌드·계약 테스트·셰이더 검증 후 영향에 따라 GPU 회귀 검증.

## 재검토할 enum 전체 목록

- `Source/Logger/Logger.h:15` — `TLogLevel` → `ELogLevel`
- `Source/Rendering/Core/RenderSettings.h:6` — `TOverlayDisplayMode` → `EOverlayDisplayMode`
- `Source/Rendering/Core/RenderSettings.h:13` — `TOverlayShadingDiagnostic` → `EOverlayShadingDiagnostic`
- `Source/Rendering/Core/RenderSettings.h:27` — `TOverlayOccupancyTileSize` → `EOverlayOccupancyTileSize`
- `Source/Rendering/Core/RenderSettings.h:91` — `TRenderViewMode` → `ERenderViewMode`
- `Source/Rendering/Core/RenderSettings.h:121` — `TSolverTransferWeightView` → `ESolverTransferWeightView`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:54` — `TReactionMode` → `EReactionMode`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:59` — `TReactionRateModel` → `EReactionRateModel`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:65` — `TReactionFunction` → `EReactionFunction`
- `Source/SurfaceState/Solver/SurfaceStateSolver.h:55` — `TSurfaceSolverTerm` → `ESurfaceSolverTerm`
- `Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h:6` — `TSurfaceGPUDescriptorBinding` → `ESurfaceGPUDescriptorBinding`
- `Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h:15` — `TGPUBufferMemoryCategory` → `EGPUBufferMemoryCategory`

## 포맷 불일치 파일 전체 목록

- `Source/SurfaceState/SurfaceStateSystem.h`
- `Source/SurfaceState/SurfaceStateSystem.cpp`
- `Source/Application/EngineConfig.cpp`
- `Source/Application/FrameCoordinator.cpp`
- `Source/Application/PerformanceSettings.h`
- `Source/Scene/DemoAnimation.cpp`
- `Source/AssetManager/Assets/TextureAsset.cpp`
- `Source/Rendering/Core/RendererShared.h`
- `Source/Rendering/Core/Renderer.cpp`
- `Source/Rendering/Profiling/RendererBenchmark.cpp`
- `Source/Rendering/Scene/RendererMaterials.cpp`
- `Source/Rendering/Overlay/AccumulationOverlaySides.cpp`
- `Source/SurfaceState/Mapping/SurfaceMappingBuilder.cpp`
- `Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.cpp`
- `Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp`
- `Source/SurfaceState/GPU/Resources/SharedGeometryResources.h`
- `Source/SurfaceState/GPU/Resources/InstanceResources.h`
- `Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp`
- `Source/DebugUI/Core/DebugUI.cpp`
- `Source/DebugUI/Panels/DebugUIContactProfile.cpp`
- `Source/DebugUI/Panels/DebugUIRender.cpp`
- `Source/DebugUI/Panels/DebugUIViewport.cpp`
- `Source/DebugUI/Panels/DebugUISimulation.cpp`
- `Source/DebugUI/Profiling/DebugUIProfiling.cpp`
- `Source/DebugUI/Editor/DebugUIEditor.cpp`
- `Source/GPU/Vulkan/VulkanInstance.cpp`
- `Tests/SceneResourceTests.cpp`
- `Tests/SurfaceCacheTests.cpp`
- `Tests/SurfaceGPUResourceTests.cpp`
- `Tests/SurfaceDebugRenderingTests.cpp`
- `Scripts/Research/MesoStudy.h`
- `Scripts/Research/ModularBaseline.h`
- `Scripts/Research/ResearchVulkanDevice.h`
- `Scripts/Research/CouplingStudy.cpp`
- `Scripts/Research/AuthoringProbe.cpp`

## 한국어 설명으로 전환할 Doxygen 태그 행

- `Source/InputSystem/InputSystem.h:36`
- `Source/InputSystem/Raycaster.h:37`
- `Source/Application/BenchmarkOptions.h:3`
- `Source/Application/BenchmarkOptions.cpp:3`
- `Source/Scene/DemoAnimation.h:84`
- `Source/Scene/DemoAnimation.h:87`
- `Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h:20`
- `Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h:24`
- `Source/AssetManager/Loaders/SurfaceProfileDistributionLoader.h:33`
- `Source/Rendering/Core/Renderer.h:111`
- `Source/Rendering/Core/Renderer.h:113`
- `Source/Rendering/Core/Renderer.h:116`
- `Source/Rendering/Surface/HeightFieldSmoothing.h:17`
- `Source/Rendering/Surface/HeightFieldSmoothing.h:35`
- `Source/Rendering/Overlay/AccumulationOverlaySides.h:20`
- `Source/Rendering/Overlay/AccumulationOverlaySides.h:57`
- `Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h:24`
- `Source/Rendering/Overlay/Geometry/TexelGeometryPreview.h:53`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:45`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:110`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:120`
- `Source/SurfaceState/Types/SurfaceStateTypes.h:124`
- `Source/SurfaceState/Cache/SurfaceCache.h:35`
- `Source/SurfaceState/Cache/SurfaceCache.h:47`
- `Source/SurfaceState/Cache/SurfaceCache.h:54`
- `Source/SurfaceState/Cache/SurfaceCache.h:58`
- `Source/SurfaceState/Runtime/SurfaceRuntimeData.h:22`
- `Source/SurfaceState/Runtime/SurfaceRuntimeData.h:34`
- `Source/SurfaceState/State/SurfaceInput.h:40`
- `Source/SurfaceState/State/SurfaceInput.h:45`
- `Source/SurfaceState/State/SurfaceInput.h:48`
- `Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h:20`
- `Source/SurfaceState/Geometry/SurfaceGeometryBuilder.h:21`
- `Source/SurfaceState/Geometry/SharedSurfaceGeometryData.h:40`
- `Source/SurfaceState/Geometry/SurfaceTexelMeshBuilder.h:45`
- `Source/SurfaceState/Debug/TexelInspector.h:36`
- `Source/SurfaceState/Debug/TexelInspector.h:60`
- `Source/SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h:75`
- `Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h:47`


## 추가 수동 리뷰 — `00_Code-Style.md` §05 체크리스트

2026-11-09 사용자 후속 요청으로 접두어·서식과 별개인 구조/의미 규칙을 재검토했다. 대상은 `Source/Scene/{Camera,Scene,DemoAnimation}.cpp`, `Camera.h`, `Source/Application/{Application,FrameCoordinator}.cpp`, `Source/main.cpp`, `RendererCommands.cpp`의 기록 흐름, SceneLoader 진입부 및 SurfaceState의 대표 경로다. 전체 저장소의 의미적 전수 리뷰는 아니다. 아래 판정은 검토한 경로에 한정한다.

| 체크리스트 | 판정 | 실제 근거 |
|---|---|---|
| 최상위 함수만으로 전체 흐름 이해 | 미준수 사례 확인 | `Application.cpp:148`의 `MainLoop`, `RendererCommands.cpp:10`의 `RecordCommandBuffer`에서 단계 조율과 입력 조립·좌표 변환·Vulkan 설정이 섞인다. `main.cpp:24`와 `FrameCoordinator.cpp:63`은 비교적 명확한 사례 |
| 분기 우선순위·오류 경로·상태 전이 명시 | 부분 준수, 오류 처리 누락 | `main.cpp:31,45`와 `Scene.cpp:198`의 guard/예외는 명확하다. `Application.cpp:246`의 CPU benchmark 출력은 stream open/write 실패를 확인하지 않는다. `Application.cpp:242`도 종료 대기의 Vulkan 반환값을 무시한다. 후자는 device-lost 시 종료 정책을 정한 뒤 처리해야 함 |
| 주로 한 추상화 수준 유지 | 미준수 사례 확인 | MainLoop의 이벤트 dispatch와 행렬/접촉 payload 계산, LoadDemoAnimation의 파일 I/O와 track/key/event 필드 검증이 같은 본문에 공존 |
| 함수·파일 분리가 의미 있는 책임 표현 | 부분 준수 | Camera, Scene, DemoAnimation 파일 책임은 구분되어 있다. 다만 DemoAnimation 로더의 header/weather/tracks/contactEvents, Renderer의 여러 pass 기록이 응집된 하위 함수로 충분히 나뉘지 않음 |
| 과도한 추상화로 탐색 비용 증가 방지 | 검토 범위에서 큰 문제 미발견 | Camera의 getter나 행렬 함수는 공개 경계/수학적 책임을 가진다. 단지 짧다는 이유로 무의미한 wrapper로 판정하면 안 됨. 회전 계산을 삼각함수 호출마다 분리할 필요도 없음 |
| 주요 계약·동작·성능 보존 | 이번 리뷰만으로 판정 불가 | 구현 변경 없음. 정적 읽기로 기존 동작의 보존을 인증할 수 없다. 이전 셰이더 컴파일 통과도 CPU frame 순서·자원 수명·GPU 동기화·성능 회귀를 증명하지 않음 |

### 우선 수정할 구체적 사례

1. **오류 경로 / Side-Effect Visibility:** `Application.cpp:246`의 임시 `std::ofstream`에 `.cpu.json`을 쓰면서 성공 여부를 확인하지 않는다. 경로/권한/디스크 등의 오류가 나면 CPU benchmark 원자료 누락을 이 코드에서 보고하지 못할 수 있다. 명명한 출력 함수에서 open/write/완료 실패를 확인하고 호출자에게 전달하는 것이 적절하다. 정적 제어 흐름으로 확인한 누락이며 실제 실패 재현 테스트는 미수행이다.
2. **Top-Down / Single Level of Abstraction:** `Application.cpp:148`의 `MainLoop`에 UI 입력 수집, 16개 인자 전달, clock 정책, animation event의 object lookup·local→world 변환·payload 구성, render, GPU wait, benchmark 출력이 함께 있다. `SubmitInteractiveContact`, `ComputeFrameTiming`, `SubmitDemoContacts`, `WriteBenchmarkCpuFrames` 같은 책임으로 분리하면 상위 순서를 읽기 쉬워진다. 함수 이름은 제안이며 아직 구현하지 않았다. UI→입력→시간 계산→animation/contact→render→필요한 GPU wait→측정 순서는 보존해야 한다.
3. **Top-Down / Cohesive Abstraction:** `RendererCommands.cpp:10`의 `RecordCommandBuffer`가 파일 대부분을 차지하며 simulation 호출부터 inspector·render-state/overlay 준비·draw·UI·benchmark까지 직접 기록한다. 문제는 줄 수 자체가 아니라 상위 pass 순서와 descriptor/상수/조건 분기 세부가 같은 수준에 있다는 점이다. pass별 명명된 기록 함수로 나누되 command buffer, barrier, query 위치, A/B state 및 자원 수명을 유지해야 한다.
4. **Top-Down / Cohesive Abstraction:** `DemoAnimation.cpp:136`의 `LoadDemoAnimation`은 308행까지 header/weather/tracks/keys/contactEvents를 한 본문에서 읽고 검증한다. 이미 `Required`, `Float`, `ReadValue` helper는 있지만 이는 저수준 값 처리다. `ReadClipHeader`, `ReadWeatherCue`, `ReadTracks`, `ReadContactEvents` 같은 의미 단위가 빠져 있어 호출 순서가 목차처럼 읽히지 않는다. 중복 target/property 검사, key 순서/첫 시간 0, loop와 contactEvents의 상호 배제는 유지해야 한다.
5. **파일 수준 Top-Down:** `DemoAnimation.cpp:28`부터 세부 helper 정의가 먼저 나오고 공개 로더는 136행, 적용 함수는 318행에 나온다. 일반 선언 순서 제약과 구분하여 helper 전방 선언→상위 load/apply→세부 구현 순으로 정리할 수 있다. 현재 region 번호가 올바르다는 사실만으로 상위 흐름 우선 규칙을 만족하지는 않는다.

### 현재 열린 Scene/Camera 코드의 판단

- `Camera.cpp:43`의 projection 계산은 입력 guard→수학 계산→Vulkan 좌표계 보정으로 한 책임에 응집되어 있고, Y 보정 주석도 Why를 설명한다. `SetRotationDegrees:80`의 거리 보존·pitch 제한·방향 계산과 `GetRotationDegrees:117`의 역계산도 그 자체로 의미 있는 수학적 책임이다. 연산별 helper 추출은 권하지 않는다.
- `Scene.cpp:195`의 `AdvanceDemoAnimation`은 유효성/재생 guard→시간 갱신(loop/end)→track 적용→해당 구간 event 반환의 순서가 직접 보인다. 작은 함수여서 추가 분리는 필수가 아니다. 단, 시간 0 event 포함 조건은 비자명하므로 계약/이유 설명을 보완할 수 있다.
- `Scene.cpp:176`의 `AddInitialContact`와 181행의 getter는 `Demo Animation State` 섹션 사이에 있어 `Initial State and Contacts` 섹션으로 옮기는 편이 책임 탐색에 맞다. 이는 동작 결함보다 탐색 구조의 개선 사항이다.
- `Camera.h`의 getter/setter를 없애거나 한 줄짜리 helper를 늘리는 방향은 이 규칙의 목표가 아니다.

이번 후속 리뷰는 소스 읽기와 문서 보강만 수행했다. 소스 변경·새 빌드/테스트·벤치마크 실행 없음. API·ABI·실행 흐름이 바뀌지 않아 Architecture 본문 변경은 필요하지 않다.


## GLSL 후속 리뷰 — 구조·리소스 계약·동기화

2026-11-09 사용자 요청으로 `02_GLSL.md` §01–07과 Code Style 기준을 셰이더에 적용했다. **앞선 150개 컴파일/SPIR-V 검사와 일부 계약 주석 검토만으로 GLSL 규칙 전체를 점검했다고 볼 수 없다.** 이번에는 `Shaders/` 구조 패턴을 전수 검색하고 Debug vertex/fragment/shared helper, Solver Pass2/Reaction, Overlay Occupancy/SideBoundary 및 대응 CPU pipeline/draw 경로를 수동 검토했다. 전체 셰이더의 모든 메모리 접근·Host barrier·성능을 전수 인증한 작업은 아니다.

### 확인된 구조·문서 미준수 및 개선 사항

| ID | 규칙 | 관찰 | 판단·권장 조치 |
|---|---|---|---|
| GLSL-001 | §01 main에 주요 단계, Code Style 단일 추상화 수준 | `Shaders/Debug/SurfaceDebug.vert:51`에서 좌표/normal/tangent 변환, Range·UV→texel 조회, Meso normal 읽기, 모드별 높이 변위가 모두 전개됨. 67–68행과 85–86행은 UV→texel 계산 중복 | 의미 단위의 `TryResolveSurfaceTexel`, Meso normal/offset 계산으로 분리하여 main에 순서를 드러낼 대상. bounds fallback·변위 전 FragWorldPosition 계산은 유지해야 함 |
| GLSL-002 | §04.3 CPU 대응·인덱싱·좌표계의 로컬 계약 필수 | `SurfaceDebug.vert:12,16,33,58`에 CPU vertex/material/push 타입 위치와 `gl_InstanceIndex`의 Surface ID 계약 설명이 없음 | CPU `RendererShared.h`의 TVertex layout·TMaterialUniform·TStaticMeshPushConstants, `RendererCommands.cpp:534`의 firstInstance=Section.Surface, shared descriptor의 set/texel 인덱싱 연결을 짧게 명시. 일반적인 “CPU와 같아야 함”만으로 대체하지 않음 |
| GLSL-003 | §01·Code Style §02.1 의도 드러내기 | `SurfaceDebug.vert:76`의 `RenderMode == 14u`는 모드 의미를 숫자로 표현. fragment에서는 `RENDER_MODE_MESO_OFFSET` 명명 상수를 사용 | 모드의 의미와 CPU enum 대응을 이름/주석으로 표현. 매직 넘버를 바꾸면서 CPU/GLSL 숫자 계약을 변경하지 말 것 |
| GLSL-004 | §01·Code Style 응집된 책임 | `SurfaceDebug.frag:105`부터 derivative 계산·범위/fallback·모드 선택·각 진단 색 계산을 큰 main에 직접 전개. `SurfaceSolverPass2.comp:24`, `SurfaceReaction.comp:73`, `OverlaySideBoundary.comp:285`도 상위 실행과 여러 계산 단계를 혼합 | 모드별 평가 및 응집된 계산 단위 분리 검토. derivative는 현재처럼 fragment별 조기 반환 이전의 uniform 분기에서 평가하는 제약 보존. barrier 자체와 참여 조건은 상위 흐름에 남길 것 |
| GLSL-005 | §01 권장 선언 순서 | helper 정의가 main보다 먼저 나오는 직접 소스 패턴 17개(그중 `.glsl` main 포함 파일 1개). 예: OverlaySideBoundary는 285행에서 main 시작 | 이는 전부 컴파일 오류나 강제 위반이라는 뜻이 아님. 권장 구조인 리소스→prototype→main→상세 구현으로 정리할 후보. include 내부의 모든 helper 정의를 전부 뒤로 옮기라는 요구는 아님 |
| GLSL-006 | §01 명확하고 일관된 서식 | `SurfaceDebugData.glsl:18`의 여러 멤버 한 줄, 31행 이후 한 줄 guard/return, `SurfaceReaction.comp:171`의 여러 상태 변경 한 줄 등 | 기존 4칸/여러 줄 스타일과 일관되게 정리. GLSL에 C++ 이름 접두어·snake_case 예시·C++ 포맷터 설정을 무조건 강제하지 않음 |

`SurfaceDebug.vert`의 InPosition/Normal/Tangent는 mesh-local 입력이고 normal은 inverse-transpose로 변환한다. shader 내 `FragWorldPosition`은 변위 전 위치로 유지한다는 이유가 이미 주석에 있다. 이 주석은 유지 가치가 있다. 14u 변위 분기의 존재만으로 실제 pipeline에서 활성화되는 빈도나 기능 결함을 단정하지 않았다.

### 경계·동기화 검토에서 구분할 사항

- **로컬 bounds/전제조건 보완 후보:** `SurfaceDebugData.glsl:33`은 `MesoNormals[Texel]`, 62행은 `Normals[Texel]`을 읽지만 해당 함수의 앞선 검사는 이 두 buffer 길이를 직접 확인하지 않는다. `DebugFinalNormal:100`도 fallback을 읽은 뒤 다른 유효성 검사를 한다. buffer들이 동일 texel count로 제공된다는 Host 계약을 입증/주석화하거나, 진단 helper가 짧은 buffer도 받아야 한다면 guard를 보완해야 한다. **실제 OOB 발생을 확인했다는 판정은 아니다.**
- **Barrier 통과를 유지하는 사례:** Solver Pass2는 `TexelIndex < LocalTexelCount`로 연산만 제한하고 main의 두 barrier(31,185행)는 전체 invocation이 통과하는 구조다. loop의 continue를 main의 return으로 바꾸면 안 된다. Reaction도 78,177행 barrier 사이에서 texel 연산만 조건화한다. 두 경로의 모든 호출 helper/Host 동기화까지 검증한 것은 아니다.
- **조기 반환이 있어도 곧바로 오류는 아님:** `OverlayOccupancyScan.comp:234`의 barrier 뒤 `TileValid`(shared)와 `Push.TileSize`를 보고 235행에서 반환한다. 이 경로는 workgroup 공통 조건으로 판단할 근거가 있다. `OverlaySideBoundary.comp:290`의 전역 flag 반환도 같은 주소의 읽기이며 해당 flag의 Host 생산/가시성 보장까지 별도 검토해야 한다. 패턴 검색만으로 divergent barrier 결함으로 기록하지 않았다.
- 서로 다른 pass/dispatch 사이의 barrier, descriptor 수명, shader 내부 memoryBarrier의 정확성, atomics 출력 capacity, 모든 경계값은 **추가 검증 필요**다. 이번 컴파일 결과만으로 안전을 인증하지 않는다.

### 확인된 일치 및 정상 사항

- stage 소스 40개 중 실제 entry 39개는 comment 제거 후 첫 지시문이 `#version 450`다. 나머지 `Simulation/Input/SurfaceInput.comp`는 Manifest에 명시된 placeholder라 미준수로 세지 않았다. include용 `.glsl`에는 독립 `#version`을 강제하지 않았다.
- GLSL의 `#pragma region`/endregion 지시문은 전수 검색에서 발견되지 않았다.
- `SurfaceDebug.vert` 출력과 `SurfaceDebug.frag` 입력은 location 0–6의 타입·location 4 `flat`이 텍스트 대조에서 일치한다.
- CPU `RendererShared.h`의 `BuildStaticMeshPipelineConfig`는 location 0–3을 TVertex Position/Normal/UV/Tangent 및 대응 VkFormat으로 구성하고, Debug config는 이 layout을 사용한다. push range는 vertex+fragment stage를 지정한다. `RendererCommands.cpp:534`의 draw firstInstance는 debug 경로에서 Section.Surface를 사용한다.
- CPU TMaterialUniform의 앞쪽 멤버 순서는 Debug MaterialParameters와 대응하지만 **모든 member offset/stride에 대한 reflection 대조를 수행한 것은 아니다**. CPU 블록에 후속 appearance 멤버가 더 있다는 사실만으로 ABI 불일치라고 판정하지 않았다.
- fragment의 fwidth/dFdx/dFdy를 조기 반환 전에 평가하는 이유와 WorldAreaPerUV의 면적 상쇄 주석은 비자명한 제약·Why를 설명하는 좋은 사례다.

### 이번 후속 작업의 검증 및 문서 영향

파일 패턴 전수 검색 + 위 경로 수동 대조만 수행. 이전 작업의 Manifest 75개 엔트리·변형×validation 0/1=150개 glslc/spirv-val 통과와 구분한다. **이번 작업에서 새 컴파일·Reflection·Pipeline/GPU 실행·성능 측정은 수행하지 않았다.** GLSL/CPU 코드는 변경하지 않았으며 API·ABI·계산 의미·흐름의 변경이 없어 Architecture 계약 본문 변경은 필요하지 않다. 규칙과 Debug/Shared Source 및 CPU–GPU Interface 문서를 읽고 감사 결과만 보강했다.
