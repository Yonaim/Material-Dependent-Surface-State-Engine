# Source ↔ Architecture Crosscheck

C++와 Shader의 **실제 의존성 및 CPU–GPU 계약을 확인하기 위한 탐색표**다. 흐름은 [[02_Architecture/00_Flows/00_Main-Flow|Flows]], 수식·불변식은 [[02_Architecture/01_Contracts/00_Contract-Register|Contracts]], 파일 책임은 [[02_Architecture/02_Source/00_Source-Map|Source Map]]이 정본이다.

## 연결 경계별 확인 지점

| 실행 경계 | 확인할 코드 | 주요 질문 |
|---|---|---|
| 앱·Scene 초기화 | `Application.cpp`, `SceneLoader.cpp` | Mesh, Surface Profile, Runtime handle이 언제 준비되는가? |
| Surface Mapping·Cache | `SurfaceDataManager.cpp`, `SurfaceCache.cpp` | Runtime key와 `.Surface` 캐시의 유효성 검사·재구축 조건은? |
| Contact 입력 | `SurfaceContactApplication.cpp`, `SurfaceStateSystem.cpp` | Contact가 어떤 Texel의 `InputDelta`로 변환되고 언제 소비되는가? |
| GPU Resource·ABI | `DescriptorResources.cpp`, `SurfaceGPUAbi.h`, `Shaders/Shared/` | Buffer Layout, Binding 및 Push Constant가 CPU·GLSL에서 일치하는가? |
| Solver 명령 순서 | `SurfaceStepRecording.cpp`, `SurfaceStateSolver.cpp` | Current/Next A/B, Pass, Reaction, Barrier의 순서는? |
| Simulation Shader | `SurfaceSolverPass1.comp`, `SurfaceSolverPass2.comp`, `SurfaceReaction.comp` | Outgoing·Incoming·State의 소비·생성 규칙은? |
| Geometry Feedback | Geometry Update C++·GLSL | 상태 기반 높이·법선이 언제 갱신되고 다음 Step에서 읽히는가? |
| Render 소비자 | `RendererCommands.cpp`, `RenderStateTexture.cpp` | 현 State의 표시용 가공과 Graphics Barrier는 올바른가? |

## 변경 시 확인 원칙

1. CPU 구조체와 GLSL 레이아웃 변경은 Descriptor, Packing, Shader Binding 및 모든 소비자를 함께 확인한다.
2. Solver의 GPU 동기화는 함수 경계만 보고 판단하지 않는다. 실제 Vulkan Barrier와 Shader `barrier()` 참여 조건을 확인한다.
3. `.Surface` 캐시의 정적 Geometry와 Instance별 동적 GPU State를 구별한다.
4. 빌드·테스트·GPU 실행 여부는 [[05_Experiments/01_Validation-and-Tests|Validation]]의 근거로만 판단한다.
