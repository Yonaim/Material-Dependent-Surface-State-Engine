# Project Brief — Surface State Propagation Engine

> 기존 프로젝트 자료에서 이관한 **프로젝트 정본**. 내용은 업로드한 소스와 이전 문서를 바탕으로 작성했다. 개발 환경 및 동작에 대한 기존 검증 주장은 **이번 환경에서 재검증하지 않았다**.

## 목표 및 범위
- 프로젝트: **MDSSP / MDSS Engine** — 재질·형상 특성에 기반한 표면 상태 전파 및 가시화.
- 영역: Scene/Asset 로드, Surface Mapping·Geometry, State 및 Reaction, GPU Simulation, Vulkan Rendering, Debug UI.
- 실행 핵심: `Source/main.cpp` → `Source/Application/Application.cpp` → `Source/Application/FrameCoordinator.cpp` 및 `Source/SurfaceState/SurfaceStateSystem.cpp` → Rendering.
- 실제 소스 위치는 **이 볼트 기준으로 `../../Source/`, `../../Shaders/`, `../../Tests/`**이며, 프로젝트 루트는 볼트의 상위 두 단계다.

## 개발 프로필
| 항목 | 현재 근거 / 상태 |
|---|---|
| 주요 언어 | C++ (`.cpp`, `.h`), GLSL (`.comp`, `.vert`, `.frag`, `.glsl`), Python, Shell |
| C++ 표준 | **C++20** (2026-11-09: 루트 `CMakeLists.txt`의 `CMAKE_CXX_STANDARD 20` 확인; 전체 빌드는 별도 검증) |
| C++ 네이밍 | **Unreal-Inspired** (2026-11-09 사용자 확정): 일반 클래스·구조체 `F`, 템플릿 클래스 `T`, 추상 인터페이스 `I`, enum `E`, bool `b`+PascalCase. 함수·변수 PascalCase(언어 프로필의 기존 관례 예외 적용). 현재 소스에는 기존 `T` 타입 등이 남아 있으며 전환 미완료. 파일명에 타입 접두어를 자동 적용하지 않음 |
| GPU | Vulkan, GLSL `#version 450` (관련 코드 확인) |
| 소스 포맷터 | `Scripts/format.sh`, `Scripts/format.ps1`는 `clang-format` 사용. 루트 `.clang-format` 확인(2026-11-09); 명명 규칙은 포맷터가 검사하지 않음 |
| 진입점 | `Source/main.cpp`, `Source/Application/Application.cpp` |
| 빌드/테스트 | `Scripts/build.sh`, `Scripts/build.ps1`, `Tests/*.cpp`; 루트 `CMakeLists.txt`의 CTest 등록 확인. 현재 환경의 전체 빌드·테스트 실행 여부와 의존성은 별도 확인 필요 |
| 환경 | GPU/OS/드라이버 요구사항은 별도 검증 필요 |

## 주요 참조 문서
- [[01_Project-Context/01_Glossary|Glossary]] / [[01_Project-Context/02_Requirements|Requirements]] / [[01_Project-Context/03_Constraints|Constraints]]
- [[02_Architecture/00_Flows/00_Main-Flow|Main Flow]] / [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]] / [[02_Architecture/02_Source/00_Source-Map|Source Map]]
- [[04_Development/00_Current-Work|Current Work]] / [[05_Experiments/01_Validation-and-Tests|Validation]]
