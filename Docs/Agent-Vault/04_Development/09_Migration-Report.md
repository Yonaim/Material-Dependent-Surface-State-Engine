# Migration Report — Universal Agent Vault

## Scope
- 기존 MDSSP 전용 에이전트 볼트의 Architecture·Development·Experiments·Research 문서를 Universal Agent Vault 규칙 아래로 재배치했다.
- 공통 Standards·Obsidian 테마·최소 플러그인 설정은 v5.3에서 가져왔다.
- 구 문서의 내용은 프로젝트 지식으로 보존했으며, **과거 결정을 새 ADR로 자동 승인하지 않았다**.
- 프로젝트 루트에는 경량 [`AGENTS.md`](../../../AGENTS.md)를 추가해 문서 진입점을 명시했다.
- 해당 업로드에는 `CMakeLists.txt`·`.clang-format`이 없어 전체 빌드 및 포맷터 검증은 수행하지 못했다.

## Code Formatting
- C/C++ 기존 region 표기에 번호와 3줄 섹션 주석을 붙였다.
- `FrameCoordinator.cpp`는 Frame Preparation → Simulation Recording → Clock & State Lifecycle 구획을 새로 만들었다.
- GLSL은 실제 선언의 위치를 변경하지 않고 Interface/`main()` 앞에 섹션 주석만 추가했다.
- **함수 본문, 리소스 Binding, Dispatch, 메모리 동기화, 빌드 설정은 변경하지 않았다.**

## Recommended follow-up
1. 전체 소스가 포함된 `CMakeLists.txt`와 `.clang-format`을 복구한 후 build/CTest/Shader Compile 검증.
2. `TApplication` 생성자와 `TRenderer`의 긴 함수는 실제 상태 의존성 및 성능을 확인한 뒤 별도의 행동 보존 리팩터링으로 분해.
3. 사양·계약 문서는 이관된 역사적 기록이며 재검증되지 않았다. 새 변경을 시작할 때 실제 소스·테스트와 대조.

## 공급되지 않은 근거 자료
- 과거 성능 문서가 참조하는 저장소 루트 `Performance-Data/`의 원시 데이터(문서상 약 19.5 GB)는 업로드 ZIP에 없다. 관련 링크는 원래의 예상 경로로 유지하되 **현재 검증 가능한 결과라고 주장하지 않는다**.
- 원래 프로젝트가 가진 Git 이력·루트 `.clang-format`·`CMakeLists.txt`는 ZIP에 들어 있지 않으므로 이번 수정본에 임의 생성하지 않았다.
- **이번 변경은 단계적 코드 가독성 정리**이며 복잡한 생성자·렌더링/솔버 메서드의 의미 단위 분해는 개별 검증을 동반하는 후속 단계로 남겨 두었다.

## 이번 이관의 자동 검증
- 입력 자료 파일 381개 존재 확인, **실제 변경된 코드 파일 133개** (C/C++ 84개, GLSL 49개).
- `#pragma region` 174개에 대한 균형 확인 및 코드를 구성하는 원래 비주석 줄의 순서·내용 대조: 문제 0건.
- Python AST 44개, Shell `bash -n` 4개, Obsidian wiki link 및 Markdown local link 검증.
- **기존 연구 기록/성능자료에 대한 비해결 링크 178건**은 업로드에 해당 원자료·원래 PC 절대 경로가 없어 보존만 하였다. 링크가 곧 실측 검증 가능성을 의미하지 않는다.
- C++ 전체 빌드, CTest, glslangValidator·SPIR-V·GPU 런타임 검증: **미실행** (루트 CMake 구성과 빌드 환경 미제공).

## 기존 형식 결함의 보정
- `DebugUIRender.cpp`와 `DebugUIViewport.cpp`는 기존에 `#pragma region`과 `#pragma endregion`이 서로 다른 `.cpp` 파일에 걸쳐 있었다. 각각의 파일에서 독립적으로 닫히도록 표시 위치만 보정했다.
- `Scripts/*.sh`는 CRLF를 LF로 정규화하여 Linux Bash에서도 구문을 판독할 수 있도록 했다. 쉘 명령어 자체는 변경하지 않았다.

## 이번 이관의 자동 검증
- 입력 자료 파일 381개 존재 확인, **실제 변경된 코드 파일 137개** (C/C++ 84개, GLSL 49개).
- `#pragma region` 174개에 대한 균형 확인 및 코드를 구성하는 원래 비주석 줄의 순서·내용 대조: 문제 0건.
- Python AST 44개, Shell `bash -n` 4개, Obsidian wiki link 및 Markdown local link 검증.
- **기존 연구 기록/성능자료에 대한 비해결 링크 178건**은 업로드에 해당 원자료·원래 PC 절대 경로가 없어 보존만 하였다. 링크가 곧 실측 검증 가능성을 의미하지 않는다.
- C++ 전체 빌드, CTest, glslangValidator·SPIR-V·GPU 런타임 검증: **미실행** (루트 CMake 구성과 빌드 환경 미제공).
