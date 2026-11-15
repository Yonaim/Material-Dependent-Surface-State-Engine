# Project Setup — Agent Workflow

> 이 문서는 **새 프로젝트 시작과 기존 프로젝트에 볼트를 도입할 때의 공통 절차**다. 프로젝트 고유 사실과 설치 이력은 `Project Brief`에 섞지 않는다.

## 00. Existing First — 조사 후 질문

1. 먼저 저장소·파일 구조를 조사한다. `Makefile`, `CMakeLists.txt`, `CMakePresets.json`, `.uproject`, `Source/*.Target.cs`, `*.Build.cs`, `.clang-format`, `Tools/`, `.vscode/settings.json`, CI 및 실제 빌드 명령을 확인한다.
2. 프로젝트 목적·언어 표준·컴파일러·플랫폼·외부 의존성·대상 실행 파일을 **확인된 사실 / 추론 / 미정**으로 나눈다. 기존 코드를 읽지 않고 새 빌드 시스템을 강제하지 않는다.
3. [Setup Questionnaire](06_Setup-Questionnaire.md)의 질문 후보 중 **현재 자료에서 답을 얻을 수 없고 다음 단계에 꼭 필요한 것만** 짧게 질문한다. 기본값과 근거가 있는 선택은 제안하고 사용자 의도와 충돌하지 않게 한다.
4. [Build-System Selection](05_Build-System-Selection.md)의 기준으로 `Existing / Make / CMake / UnrealBuildTool / Other` 중 경로를 선택한다. 엔진·플랫폼 강제 규칙이 가장 우선이다.
5. [Formatting Assets](../../Assets/Formatting/format_before_build.py)와 선택한 [Build Assets](../../Assets/Build/README.md)를 조사한다. 원본 템플릿의 내용을 **대상 프로젝트에 맞게 편집**하여 배치한다. 템플릿 원본을 덮어쓰지 않는다.
6. 실제 빌드 진입점마다 **`clang-format → indent_regions.py → compile`** 순서를 연결하고 대상 소스·외부 파일 제외·실패 중단을 검증한다. 빌드 도구가 이를 지원하지 않는 경우 우회 경로를 명시하고 해결책을 제안한다.
7. 기본 문서 네 개 `Project Brief`, `Glossary`, `Requirements`, `Constraints`와 홈·Document Map을 **프로젝트 사실만으로** 채운다. Architecture의 최초 진입 흐름·핵심 Source Map은 실제 코드 경로를 따라 작성한다.
8. 실제 수행한 명령과 검증만 Validation/Work Log에 기록한다. 설정값·빌드 시스템 변경이 설계 결정이면 ADR을 남긴다.

9. 볼트의 [VS Code 설정](../../.vscode/settings.json)을 프로젝트 루트의 `.vscode/settings.json`에 복사해 적용한다. 파일이나 설정이 이미 있으면 덮어쓰지 말고 기존 항목과 병합한다. 프로젝트 루트는 그대로 두고, `Docs` 바로 아래에 둔 볼트 폴더에서만 루트의 `.gitignore`, `AGENTS.md`, `CHANGELOG.md`, `README.md`를 Explorer에서 숨긴다. 볼트의 `00_Home.md`와 `01_Document-Map.md`는 보이게 유지한다. 생성 산출물·임시 파일과 볼트 전용 Obsidian 설정·문서 템플릿도 숨긴다.

## 10. 경로 정책과 덮어쓰기 금지

| 원본 | 설치 위치 | 변경 규칙 |
|---|---|---|
| [`.vscode/settings.json`](../../.vscode/settings.json) | 프로젝트 루트 `.vscode/settings.json` | 없으면 복사하고, 있으면 기존 설정을 보존하며 필요한 항목만 병합 |
| `Assets/Formatting/.clang-format` | 루트 `.clang-format` | 기존 것이 있으면 비교·병합 |
| `Assets/Formatting/indent_regions.py` | `Tools/indent_regions.py` | 기존 변경과 충돌 확인 |
| `Assets/Formatting/format_before_build.py` | `Tools/format_before_build.py` | 소스 디렉터리·필터 확인 |
| `Assets/Build/Make/Makefile.template` | 루트 `Makefile` | **신규 Make 프로젝트에서만** 사용 |
| `Assets/Build/CMake/*template` | 루트 CMake 설정 | **신규 CMake 프로젝트에서만** 사용 |
| `Assets/Build/Unreal/Build-Integration.md` | 기존 `*.Target.cs` 일부 수정 | 완전 교체 금지; 필요한 타깃에 병합 |

새 디렉터리는 `Source`, `Include`, `Docs`, `Tools`, `Shaders`, `Build`처럼 대문자로 시작한다. `.git`, `.vscode`, `.obsidian`, Unreal의 `Config`, `Content`, `Plugins`, `Intermediate`, 플랫폼별 고정명 등은 외부 요구사항을 우선한다. `Scripts/`는 별도로 만들지 않고 자체 도구는 `Tools/`를 사용한다.

## 20. 선택 후 확인할 종료 조건

- [ ] 새 프로젝트인가 기존 프로젝트인가 명확하다. 기존 설정을 무단 교체하지 않았다.
- [ ] 언어·표준·컴파일러·플랫폼과 빌드 타깃을 확인했다.
- [ ] Make/CMake/UnrealBuildTool 중 필요한 하나만 선택했거나 기존 빌드 체계를 유지했다.
- [ ] 원본 `.clang-format`과 `Tools` 스크립트의 설치·실행 경로가 정확하다.
- [ ] 빌드 전 포맷 단계가 **실제 타깃의 컴파일 전에 실행**된다. 실패 시 빌드가 중단된다.
- [ ] IDE/CLI/CI와 별도 Unreal Editor 경로에서 후크가 우회되지 않는지 확인했다. 미확인 경로는 명시했다.
- [ ] 정본 문서에는 프로젝트 목적과 요구·구조만 있고 템플릿 적용 이력은 없다.
- [ ] 실제 수행한 검증을 기록했다. 불필요한 질문으로 Setup을 지연시키지 않았다.
