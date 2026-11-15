# Mandatory Pre-Build Formatting — Setup & Execution

> **프로젝트 최초 Setup 시 설치하고, 이후 모든 빌드 전에 실행한다.** 저장 시 포맷팅(Run On Save)은 편의 기능이며 빌드 전 포맷팅을 대체하지 않는다.

## 00. 원본과 배치 경로

볼트는 도구의 재사용 원본만 보관한다. Setup 에이전트는 해당 프로젝트에서 사용할 실제 실행 파일을 프로젝트 루트에 준비한다.

| 볼트 안의 원본 | 프로젝트에 배치할 위치 | 책임 |
|---|---|---|
| [`Assets/Formatting/.clang-format`](../../Assets/Formatting/.clang-format) | `<Project>/.clang-format` | C/C++ 포맷 규칙 |
| [`Assets/Formatting/indent_regions.py`](../../Assets/Formatting/indent_regions.py) | `<Project>/Tools/indent_regions.py` | Region·내부 코드의 시각적 들여쓰기 |
| [`Assets/Formatting/format_before_build.py`](../../Assets/Formatting/format_before_build.py) | `<Project>/Tools/format_before_build.py` | 두 포맷터의 순서·대상 및 실패 처리 |

- 프로젝트 폴더명은 `Tools`, `Source`, `Include`, `Shaders`, `Docs`처럼 **대문자로 시작**한다. 도구가 고정한 `.git`, `.vscode`, `.obsidian`과 외부 규격·기존 빌드 경로는 예외다.
- 기존 `.clang-format`이나 `Tools` 파일이 있다면 **덮어쓰지 말고 비교·병합**한다. 프로젝트 의도와 충돌하면 변경 근거·합의가 필요하다.
- Python 3과 clang-format 실행 경로를 실제 환경에서 확인하고, clang-format 버전이 설정을 지원하는지 시험한다.
- 포맷 대상은 해당 프로젝트가 소유하는 소스에 한정한다. `ThirdParty`, 생성 파일, 빌드 산출물, 볼트 문서는 임의로 포맷하지 않는다.

## 01. 매 빌드 전 실행 순서

```text
Build Request
   └─ Pre-Build Formatting
        ├─ clang-format (C/C++ 파일)
        ├─ indent_regions.py (같은 C/C++ 파일)
        └─ [선택] indent_regions.py (GLSL Region)
   └─ Compile / Link / Test
```

프로젝트 루트에서 실행:

```powershell
python Tools/format_before_build.py --source Source --source Include
```

- 기본 스캔 경로: `Source`, `Include`, `Src` 중 **실제로 존재하는 경로**. 프로젝트가 다른 경로를 사용하면 `--source`를 수정한다.
- GLSL 파일은 clang-format 지원 대상으로 가정하지 않는다. 셰이더 Region 후처리가 필요한 프로젝트에서만 `--include-shaders`를 추가하고, **반복 실행에 따른 들여쓰기 누적·원본 의미 변경을 개별 검증**한다.
- 형식 검사는 `--check`로 실행할 수 있지만 이는 **자동 포맷을 수행하는 빌드 전 절차를 대신하지 않는다**.
- 포맷 단계가 실패하면 빌드를 중단하고 원인을 보고한다. 오류를 무시한 채 정상 빌드했다고 기록하지 않는다.
- `IndentPPDirectives: Leave`만으로 Region 본문 들여쓰기가 정규화되는 것은 아니다. 실행 순서가 중요하며 **연속 두 번 실행해 diff가 발생하지 않는지** 프로젝트 실제 코드로 검증한다.

## 02. 실제 빌드 시스템에 연결 (Setup 에이전트 필수)

프로젝트의 **모든 일반 빌드 진입 경로**에서 컴파일 전에 이 명령을 실행하도록 통합한다. 템플릿에는 특정 빌드 시스템이나 대상 이름을 강제하지 않는다.

CMake 예시 (실제 타깃명·소스 디렉터리로 교체):

```cmake
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(FormatProjectBeforeBuild
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/format_before_build.py"
            --root "${CMAKE_SOURCE_DIR}" --source Source --source Include
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Format C/C++ and Region indentation before compilation"
    VERBATIM
)

# 실제로 빌드하는 관련 타깃에 연결한다. 필요한 다른 타깃도 동일하게 연결.
add_dependencies(YourActualTarget FormatProjectBeforeBuild)
```

`add_custom_target`은 일반 빌드에서 항상 갱신되는 타깃이다. **실제 실행 타깃에 `add_dependencies` 연결이 빠지면 매 빌드 보장이 성립하지 않는다.** Make/MSBuild/기타 체계에서도 동등한 Pre-Build 단계로 연결한다. 포맷 단계가 빌드 명령을 다시 호출하는 순환 구조는 만들지 않는다.

VS Code `Run On Save`는 선택적 보조 도구일 뿐이다. 빌드 메뉴, CLI, CI 등에서 독립적으로 빌드할 때도 실행되어야 한다.

## 빌드별 설치 템플릿

빌드 시스템은 [`Build-System Selection`](05_Build-System-Selection.md)으로 결정하고, [`Assets/Build`](../../Assets/Build/README.md)에서 **해당하는 한 종류**의 템플릿만 사용한다. Make의 재귀 빌드, CMake의 `add_dependencies`, Unreal의 타깃별 `PreBuildSteps`는 서로 다른 방식이며, 어떤 경우에도 **컴파일이 시작되기 전에 포맷 완료가 확인**되어야 한다. 최초 Setup과 필요한 질문은 [`Project Setup`](04_Project-Setup.md) 및 [`Setup Questionnaire`](06_Setup-Questionnaire.md)를 따른다.

## 03. 설치 및 검증 체크리스트

- [ ] Assets 원본 3개가 실제 프로젝트 경로에 배치되고 내용·이름이 정확하다.
- [ ] C/C++ 파일 경로를 검사했고 외부·생성 소스는 제외했다.
- [ ] `.clang-format`이 설치된 clang-format 버전에서 오류 없이 로드된다.
- [ ] 한 파일에서 `clang-format → indent_regions.py` 순서를 확인했다.
- [ ] 같은 파일을 두 번 연속 포맷해 diff가 발생하지 않음을 확인했다.
- [ ] CMake/MSBuild/기타 **실제 빌드 진입점**에 Pre-Build 단계가 연결됐다.
- [ ] 빌드 명령을 실행해 포맷 단계가 **컴파일 전에 실제 실행되는 것**을 확인했다.
- [ ] 포맷 오류 시 빌드가 실패하며 실행한 검증 범위를 기록했다.

검증 결과는 프로젝트의 Validation/Work Log에 기록한다. **실행하지 않은 빌드·테스트를 통과했다고 쓰지 않는다.**
