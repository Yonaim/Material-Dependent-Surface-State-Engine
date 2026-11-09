# Source ↔ Vault 교차 검증 (2026-10-26)

> 검증 입력: `MDSSP-Agent-Vault_Reorganized.zip` + `Shaders.zip` 안의 `Source/`(C++ 180개: `.cpp` 90 + `.h` 90)·`Shaders/`(65개). **컴파일/실행/테스트가 아닌 실제 구현 정적 대조**다. 과거 보고서의 목표와 현재 코드가 다르면 현재 소스 구현을 우선 기록했다.

## 코드에서 직접 확인한 연결 경계

| 실행 경계 | 소스 근거 | 문서에 반영한 주의점 |
|---|---|---|
| 프로그램 → Scene 초기화 | [Application.cpp](../../../../Source/Application/Application.cpp) / [SceneLoader.cpp](../../../../Source/Scene/SceneLoader.cpp) | `surfaceProfileMap` 여부에 따라 simulation runtime handle이 없는 mesh도 가능 |
| CPU Surface 전처리 | [SurfaceDataManager.cpp](../../../../Source/SurfaceState/Runtime/SurfaceDataManager.cpp) / [SurfaceCache.h](../../../../Source/SurfaceState/Cache/SurfaceCache.h) | `.Surface` cached geometry와 GPU State buffer를 혼동하지 않는다 |
| Frame/clock | [FrameCoordinator.cpp](../../../../Source/Application/FrameCoordinator.cpp) / [SimulationClock.h](../../../../Source/SurfaceState/Solver/SimulationClock.h) | 1/60 tick, 8 step 한도, pause/manual step, pending time drop |
| Contact | [SurfaceContactApplication.cpp](../../../../Source/SurfaceState/Contact/SurfaceContactApplication.cpp) | hit-triangle 대응 UV texel 탐색·경고·InputDelta upload·sparse input metadata |
| GPU resource/ABI | [DescriptorResources.cpp](../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp) / [SurfaceGPUAbi.h](../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h) | AB/BA set 2개, 23 binding, profile 48B, solver push constant 128B |
| Solver record | [SurfaceStepRecording.cpp](../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp) / [SurfaceStateSolver.cpp](../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp) | Current rebuild→sparse reset→Pass1→barrier→Pass2→Next derived→A/B advance |
| Shader State math | [SurfaceSolverPass1.comp](../../../../Shaders/Simulation/Solver/SurfaceSolverPass1.comp) / [SurfaceSolverPass2.comp](../../../../Shaders/Simulation/Solver/SurfaceSolverPass2.comp) | flux scale와 InputDelta clear, cached vs recomputed incoming, stale guard |
| Geometry update | [SurfaceAccumulationHeight.comp](../../../../Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp) / [SurfaceDynamicGeometryUpdate.comp](../../../../Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp) | Current full prebuild와 Next 후행 derive 구분 |
| Renderer 소비자 | [RendererCommands.cpp](../../../../Source/Rendering/Core/RendererCommands.cpp) / [RenderStateTexture.cpp](../../../../Source/Rendering/Surface/RenderStateTexture.cpp) | Lit/view/overlay 옵션에 따른 compute conditional dispatch, display-only smoothing |

## 대조 중 보완한 누락·오해 위험

1. **`SurfaceCache.h/.cpp` 빠짐:** 신규 [[02_Architecture/03_Source/Src_Cpp/03_SurfaceState/04_Cache-and-Lifecycle|Cache 문서]] 작성. `PreprocessVersion = 4`, `FormatVersion = 4`, SRProfile response parameters/Registry 채널은 cache fingerprint에서 제외.
2. **GPU AB/BA의 정확한 의미:** `DescriptorResources.cpp`에서 CurrentState/NextState만 서로 다른 State A/B를 참조하며 나머지 shared/profile/instance buffer는 공통 reference를 사용한다.
3. **Contact의 예외 처리:** UV가 같은 텍셀이라도 hit triangle이 틀리면 대체 texel을 제한적으로 찾고, 실패하면 경고 후 거절한다. CPU InputGroup/Channel sparse metadata 경로 추가.
4. **Compute Barrier 위치:** Pass1↔Pass2와 Pass2↔geometry, sparse metadata↔indirect dispatch 및 graphics consumption의 access stage 구분 보완.
5. **Geometry Feedback 옵션:** Current full prebuild와 Next sparse 후행 update는 별개의 경로다. State-driven geometry OFF에서는 render-only height 준비가 가능하다.
6. **Pipeline variants:** Pass1/2 각각 `5 × 2 + 1 = 11`개 pipeline을 C++ 생성자가 만든다. `SurfaceInput.comp`의 실제 pipeline/dispatch는 확인되지 않았다.
7. **Source coverage:** 기존 Source 문서의 filename 목록에서 유일하게 빠진 C++ 2개가 Cache 구현임을 확인하고 추가했다.

## 검증하지 못한 것

- `Shaders.zip`에 **`CMake/ShaderManifest.cmake`, `Tests/`, 실행 빌드 설정, `Performance-Data/`가 없다.** 원래 문서가 이들을 가리키더라도 실제 파일 존재·SPIR-V 변종 컴파일·테스트 결과·벤치마크 숫자는 이번 입력으로 검증할 수 없다.
- 이번 결과물은 Vault 문서 ZIP이다. `Source/`와 `Shaders/`의 코드 파일 245개를 Vault 내부로 복제하지 않았다. 경로 링크는 기존 프로젝트에서 Vault가 `Docs/MDSSP-Agent-Vault` 아래에 있고 `Source/`·`Shaders/`가 프로젝트 루트에 있을 때 동작하도록 **기존 상대경로를 유지**했다. Vault ZIP만 별도 위치에서 열면 코드 링크는 열리지 않는다.
- 소스는 문법 수준 구조와 문자열/함수 참조로 **정적 확인**했다. Vulkan SDK/toolchain 환경이 없어 실제 build, GPU validation, runtime 결과를 성공했다고 주장하지 않는다.

## 문서 수정 지침

- 실행 순서는 [[02_Architecture/01_Flows/00_Index|Flows]], State/geometry 수식과 불변조건은 [[02_Architecture/02_Contracts/00_State-and-Profile|Contracts]], 실제 경로와 ownership은 [[02_Architecture/03_Source/00_Index|Source]].
- 매개변수/버퍼 layout 변경 시 C++ ABI, Descriptor, GLSL shared header, Pass1/2, Renderer 소비자를 **동일 commit에서** 대조한다.
- `.Surface` 캐시 invalidation과 GPU profile response parameter 갱신을 구분한다.
