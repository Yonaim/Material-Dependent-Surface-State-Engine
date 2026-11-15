# MDSS Engine Agent Vault — 범용 템플릿에서 생성된 프로젝트 사본

> 기본 운영 규칙과 00_Engineering-Standards는 `Universal-Agent-Vault.zip` 기반이다. `01_Project-Context`는 사용자가 제공한 실제 컨텍스트로 교체했다. `02_Architecture/03_Decisions.md`는 프로젝트별 단일 결정 요약이며, 연구 노트는 개인 볼트에서 관리한다.

# Universal Agent Vault — 프로젝트 초기화 체크리스트

이 디렉터리는 **공통 템플릿의 사본**이다. 공통 템플릿을 따로 보관하고, 새 프로젝트마다 복사해 각각 독립적으로 수정한다. 자동 초기화 스크립트는 필수로 두지 않는다.

## 0. 설치

- [ ] 이 템플릿을 `<repository>/Docs/Agent-Vault/` 등 프로젝트별 한 위치에 복사한다. 문서의 상대 경로를 유지한다.
- [ ] [`.vscode/settings.json`](.vscode/settings.json)은 볼트 자체를 VS Code 루트로 열 때 적용된다. **볼트를 `Docs/<Vault>/`에 넣은 프로젝트 루트를 여는 경우** Setup 시 이 설정의 `explorer.sortOrder`와 `files.exclude`를 프로젝트 루트의 `.vscode/settings.json`에도 병합한다. 이때 프로젝트 최상단 파일은 유지하고 볼트 폴더의 최상단에서는 `00_Home.md`, `01_Document-Map.md`만 보이게 한다. 기존 파일·설정을 덮어쓰지 않는다.
- [ ] **저장소 루트의 `AGENTS.md`**에서 이 볼트의 `AGENTS.md`를 가리키도록 연결한다. 일부 에이전트는 하위 폴더의 `AGENTS.md`를 자동 발견하지 못한다. 도구별 우선순위와 위치를 확인한다.

예시 — 저장소 루트의 짧은 `AGENTS.md` (실제 경로에 맞게 조정):

```md
# Project Agent Instructions
Before every code task, read `Docs/Agent-Vault/AGENTS.md` and
`Docs/Agent-Vault/01_Document-Map.md`, then follow their workflow.
```

## 필수 Setup — 소스 포맷팅 및 빌드 연결

- [ ] 이 볼트의 [`Assets/Formatting/`](Assets/Formatting/)에서 `.clang-format`을 **프로젝트 루트**에, `indent_regions.py`와 `format_before_build.py`를 프로젝트 **`Tools/`**에 복사한다.
- [ ] 파일이 이미 있으면 자동 덮어쓰지 않고 설정·변경 사항을 확인한 뒤 병합한다.
- [ ] **모든 빌드 진입점의 컴파일 이전에** `python Tools/format_before_build.py`가 실행되도록 CMake/MSBuild/Make 등의 실제 빌드 시스템에 연결한다. 이는 `clang-format → indent_regions.py` 순서를 보장해야 한다. 선택 템플릿은 [`Assets/Build`](Assets/Build/README.md)를 참조한다.
- [ ] 저장 시 Run On Save가 이미 동작하더라도 **빌드 전 단계는 필수**이다. 소스 디렉터리·도구 실행 가능 여부·반복 실행 안정성을 검증한다.
- [ ] 새 디렉터리는 `Tools`, `Source`, `Shaders`, `Docs`처럼 대문자로 시작한다. 도구가 강제한 이름은 예외로 한다.

세부 실행 명령과 CMake 예시는 [`Pre-Build Formatting`](00_Engineering-Standards/03_Workflow/03_Pre-Build-Formatting.md)에서 관리한다.

## Setup 시작 — 선택·질문·빌드 템플릿

- [ ] [`Project Setup`](00_Engineering-Standards/03_Workflow/04_Project-Setup.md) 순서대로 저장소를 **먼저 조사**한다.
- [ ] [`Setup Questionnaire`](00_Engineering-Standards/03_Workflow/06_Setup-Questionnaire.md)에서 아직 결정하지 못한 **필수 항목만** 질문한다. 전체 문항을 한꺼번에 묻지 않는다.
- [ ] [`Build-System Selection`](00_Engineering-Standards/03_Workflow/05_Build-System-Selection.md)으로 기존 빌드 유지 / Make / CMake / UnrealBuildTool 등을 결정한다.
- [ ] 필요할 때만 [`Assets/Build`](Assets/Build/README.md)에서 해당 원본을 선택하고, 실제 프로젝트 경로·타깃·표준으로 구체화한다. 기존 설정은 무단 교체하지 않는다.
- [ ] CI/IDE/CLI/Unreal Editor 등 빌드 진입점의 포맷 실행 여부를 확인한다. 별도 경로가 확인되지 않으면 미검증으로 남긴다.

## 1. 프로젝트 정의 — 네 문서 필수

- [ ] [`Project Brief`](01_Project-Context/00_Project-Brief.md): 실제 목표, 범위, 저장소, 엔트리 포인트, 빌드·테스트 방법을 기입한다.
- [ ] [`Glossary`](01_Project-Context/01_Glossary.md): 핵심 용어·약어를 정의한다. 모르는 개념은 `미정`으로 둔다.
- [ ] [`Requirements`](01_Project-Context/02_Requirements.md): 요구 기능, 비기능 기준, 수용 조건, 우선순위를 기록한다.
- [ ] [`Constraints`](01_Project-Context/03_Constraints.md): 플랫폼, 호환성, 표준, 라이브러리, 성능, 배포 제한 등을 기록한다.
- [ ] [`Home`](00_Home.md)의 프로젝트 정보를 실제 자료와 대조하여 채운다.

## 2. 활성 개발 프로필

- [ ] [`C++`](00_Engineering-Standards/01_Languages/01_CPP.md), [`C`](00_Engineering-Standards/01_Languages/00_C.md), [`Python`](00_Engineering-Standards/01_Languages/03_Python.md), [`Shell`](00_Engineering-Standards/01_Languages/04_Shell.md) 중 사용할 언어를 선택한다. C++이 주력인 경우에도 다른 언어 규칙을 무조건 읽지 않는다.
- [ ] C++ 표준(예: 98/17/20/23), 컴파일러, 빌드 시스템, 서식 규칙, 명명 프로필(Unreal / Unreal-Inspired / T-PascalCase / Existing)을 실제 프로젝트에 맞게 정한다.
- [ ] C 언어를 사용한다면 C 표준·컴파일러·함수 접두어·오류 처리 방식을 확정한다.
- [ ] [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md)와 [`Doxygen`](00_Engineering-Standards/02_Documentation/00_Doxygen.md)의 적용 범위를 확정한다.
- [ ] [`Commit Style`](00_Engineering-Standards/03_Workflow/02_Commit-Style.md)을 프로젝트에 맞게 채택하거나 덮어쓴다.

## 3. 구조와 문서 탐색

- [ ] [`Main Flow`](02_Architecture/00_Flows/00_Main-Flow.md)에 실제 또는 **제안된** 최상위 실행 흐름을 상태와 함께 기입한다.
- [ ] [`Contract Register`](02_Architecture/01_Contracts/00_Contract-Register.md)에 실제 계약과 그 출처를 연결한다.
- [ ] [`Source Map`](02_Architecture/02_Source/00_Source-Map.md)에 **중요 모듈만** 등록한다. 모든 파일에 문서 하나씩 생성하지 않는다.
- [ ] [`Document Map`](01_Document-Map.md)의 프로젝트 전용 키워드/파일 경로/문서 연결을 채운다. 링크가 존재하는지 확인한다.
- [ ] 서로 다른 문서에서 같은 정의를 중복 유지하지 않는지 확인한다. 불필요한 `00_Overview.md`는 만들지 않는다.

## 4. 첫 변경으로 작업 흐름 검증

- [ ] 작업 시작 시 `AGENTS.md` + `01_Document-Map.md` + 관련 문서를 읽는다.
- [ ] 코드를 수정하고 실제 실행한 검증만 기록한다.
- [ ] 영향 문서를 검토·갱신하고 [`Work Log`](04_Development/01_Work-Log.md)에 **작업 단위 1건**을 남긴다.
- [ ] 설계상 중요한 결정이면 [`ADR`](02_Architecture/03_Decisions.md)에 기록한다.
- [ ] 마지막으로 Markdown 링크, 소스 경로, 활성 언어 프로필, VS Code 설정을 확인한다.

## 범용 템플릿과 프로젝트 사본

- **공통 템플릿에 보관:** 재사용되는 규칙, 문서 스켈레톤, 공통 예시 및 워크플로.
- **프로젝트 사본에만 보관:** 실제 코드 경로, 프로젝트 고유 지식, 실험 수치, 검증 결과, ADR, Work Log.
- 프로젝트에서 검증한 개선이 일반적으로 유용하면 **선별하여** 공통 템플릿에도 반영한다. 프로젝트 고유 사실을 자동 병합하지 않는다.
- 문서가 아직 미작성인 상태에서 관련 작업을 진행해야 한다면, 읽지 않은 내용을 추측하지 말고 필요 문서를 실제 근거로 채운다.
