# Constraints — MDSS Engine

> 시스템이 준수해야 하는 구현·호환성 제약을 기록한다. 플랫폼별 도구체인과 실행 가능 여부는 빌드·검증 기록을 따른다.

| ID | 구분 | 제약 / 근거 | 상태 |
|---|---|---|---|
| CON-001 | 언어 표준 | C++20 (`CMakeLists.txt`의 `CMAKE_CXX_STANDARD`) | CMake 설정 기준 |
| CON-002 | Vulkan GPU | `Source/GPU/Vulkan/`, `Shaders/`, SPIR-V Shader Manifest 사용 | 소스 구조 확인 |
| CON-003 | Host–GPU ABI | Buffer packing, Descriptor Set/Binding, Push Constants, Barrier는 CPU·GLSL·Contracts 간 일치해야 함 | 매 관련 변경 시 확인 |
| CON-004 | 포맷 | `Scripts/format.*`는 clang-format 사용. 루트 및 Shader별 `.clang-format` 참조 | 소스 포맷 정책; Region 마커·컴파일러 문법 보존 |
| CON-005 | 변경 안정성 | GPU 프레임 스케줄링·Solver pass/barrier·A/B state semantics를 리팩터링으로 임의 변경하지 않음 | 개발 규칙 |

## 아직 확인할 것
- CMake 설정과 포맷터 파일을 확인하고, 외부 라이브러리·SDK 및 전체 빌드 가능 여부는 개발 환경에서 검증한다.
- 지원 운영체제, Vulkan SDK·셰이더 컴파일러 및 필요한 GPU 기능.
- 재현 가능한 빌드·테스트·셰이더 컴파일 및 GPU 실측 환경.
