# Constraints — MDSS Engine

> 업로드 파일과 기존 문서에서 확인된 항목만 기록한다. 실제 도구체인·GPU 하드웨어 구성은 이 ZIP만으로 확정할 수 없다.

| ID | 구분 | 제약 / 근거 | 상태 |
|---|---|---|---|
| CON-001 | 언어 표준 | C++20. 2026-11-09 루트 `CMakeLists.txt`의 `CMAKE_CXX_STANDARD 20` 확인 | 설정 확인; 빌드 미검증 |
| CON-002 | Vulkan GPU | `Source/GPU/Vulkan/`, `Shaders/`, SPIR-V Shader Manifest 사용 | 소스 구조 확인 |
| CON-003 | Host–GPU ABI | Buffer packing, Descriptor Set/Binding, Push Constants, Barrier는 CPU·GLSL·Contracts 간 일치해야 함 | 매 관련 변경 시 확인 |
| CON-004 | 포맷 | `Scripts/format.*`는 clang-format 사용. 2026-11-09 루트 `.clang-format` 확인 | v5.8 가독성 이관·코드 동등성 검사(2026-11-10); C++ 하위 pragma 들여쓰기는 수동 보존 |
| CON-005 | 변경 안정성 | GPU 프레임 스케줄링·Solver pass/barrier·A/B state semantics를 리팩터링으로 임의 변경하지 않음 | 개발 규칙 |

## 아직 확인할 것
- 루트 `CMakeLists.txt`와 `.clang-format`은 현재 체크아웃에 존재한다. 외부 라이브러리·SDK 설정과 전체 빌드 가능 여부는 별도 검증한다.
- 지원 운영체제, Vulkan SDK·셰이더 컴파일러 및 필요한 GPU 기능.
- 재현 가능한 빌드·테스트·셰이더 컴파일 및 GPU 실측 환경.
