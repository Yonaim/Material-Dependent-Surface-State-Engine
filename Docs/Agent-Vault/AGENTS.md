# Universal Agent Vault — Agent Entry

> 모든 **코드 작업**의 필수 진입 규칙. 프로젝트별 설정은 [`00_Home.md`](00_Home.md), 관련 문서의 위치는 [`01_Document-Map.md`](01_Document-Map.md)를 따른다.

## 실행 절차 — Read → Inspect → Change → Validate → Sync → Log

1. **매 코드 작업 시작 시** 이 `AGENTS.md`와 [`01_Document-Map.md`](01_Document-Map.md)를 읽는다. 이미 동일 작업에서 읽었다면 같은 내용을 불필요하게 반복 로드하지 않아도 된다.
2. **코드 수정 전에** 작업 유형·키워드·예정 소스 경로를 기준으로 관련 문서를 선택하고, [`00_Engineering-Standards/00_Core/00_Code-Style.md`](00_Engineering-Standards/00_Core/00_Code-Style.md), 활성 언어 규칙, 관련 Context/Flows/Contracts/Source 및 필요 테스트 문서의 해당 부분을 읽는다. **전체 볼트를 일괄 읽지 않는다.**
3. 현재 소스·설정·테스트와 문서를 대조한다. 불일치가 있으면 **실제 구현에 관한 관찰**과 **지켜야 할 요구사항·계약**을 구분하고, 근거 없이 어느 한쪽을 정답으로 가정하지 않는다.
4. 의도 중심 추상화와 위에서 아래로 읽히는 실행 흐름을 유지하면서 작업한다. 기능 변경과 리팩터링을 구분하고 기존 동작·제약을 존중한다.
5. **빌드 전** 프로젝트의 포맷 파이프라인을 먼저 실행하고 실패를 해결한다. 다음 검증을 수행한다.
6. 가능한 범위에서 검증한다. **실행하지 않은 빌드·테스트·벤치마크의 성공을 주장하지 않는다.** 미실행이면 이유를 적는다.
7. 작업 종료 전에 영향받는 문서(요구사항, 용어, 계약, 흐름, 소스 맵, 테스트, ADR 포함)를 **검토하고 필요한 부분만 갱신**한다. 문서를 바꿀 필요가 없어도 영향 검토는 생략할 수 없다.
8. **모든 코드 변경 작업마다** [`04_Development/01_Work-Log.md`](04_Development/01_Work-Log.md)에 작업 단위로 짧은 기록을 남긴다. 문서 수정 없음이면 그 근거를 기록한다.
9. 종료 보고에는 `읽은 문서 / 코드 변경 / 문서 동기화 / 검증 결과·미실행 범위`를 간단히 정리한다.

## Setup 및 빌드 전 포맷팅 (필수)

- **새 프로젝트/기존 프로젝트를 먼저 구분**하고 [`Project Setup`](00_Engineering-Standards/03_Workflow/04_Project-Setup.md) 절차를 따른다. 저장소 조사 후 [`Setup Questionnaire`](00_Engineering-Standards/03_Workflow/06_Setup-Questionnaire.md)에서 아직 필요한 질문만 한다.
- [`Build-System Selection`](00_Engineering-Standards/03_Workflow/05_Build-System-Selection.md)의 결정 기준으로 기존 빌드를 유지하거나 Make·CMake·UnrealBuildTool을 선택한다. 설치 원본은 [`Build Assets`](Assets/Build/README.md)에 있으며 기존 파일을 무단으로 교체하지 않는다.
- 초기 Setup을 맡은 에이전트는 볼트 [`Assets/Formatting`](Assets/Formatting)의 원본을 확인하고 프로젝트 루트 `.clang-format`, `Tools/indent_regions.py`, `Tools/format_before_build.py`를 배치한다. 프로젝트 기존 설정을 무단 덮어쓰지 않는다.
- 실제 빌드 시스템의 **모든 관련 타깃에 포맷 선행 조건**을 연결한다. C/C++ 파일은 매 빌드마다 `clang-format → indent_regions.py` 순서로 포맷해야 한다. 포맷 실패 시 빌드를 진행하지 않는다.
- 저장 시 Run On Save는 보조 기능이며, 독립적인 빌드에서도 포맷 단계가 실행되어야 한다.
- 방법·예외·검증 절차는 [`Pre-Build Formatting`](00_Engineering-Standards/03_Workflow/03_Pre-Build-Formatting.md)을 정본으로 따른다.
- 새 프로젝트 디렉터리는 **대문자로 시작하는 이름**(`Tools`, `Source`, `Shaders`, `Docs`)을 사용한다. `.git`, `.vscode` 및 외부 도구가 강제하는 경로는 예외다.

## 정보의 책임

- **Engineering Standards:** 개발 방식의 규칙. **Project Context:** 프로젝트 목표·용어·요구사항·제약.
- **Architecture:** 현재 시스템의 흐름·계약·핵심 소스 위치. **Development:** 작업 현황·짧은 작업 기록.
- **Experiments:** 실제 검증·측정의 근거. **Decisions:** 왜 그 설계를 선택했는지에 관한 ADR.
- 중복 정의를 만들지 않는다. 라우팅 맵은 정보를 복제하지 않고 **위치만 안내**한다.

## 적용 범위

- 파일/폴더 번호는 **각 디렉터리에서 `00, 01, 02…`으로 다시 시작**하고, 같은 디렉터리의 파일과 폴더 번호는 독립적이다.
- 번호는 탐색 힌트이지 호출 깊이·빌드 순서가 아니다. 빌드·패키지·언어 규칙을 우선한다.
- 저장소 루트의 에이전트 설정이 이 파일을 실제로 참조하도록 연결되어 있어야 한다. 설정 방법은 [`README.md`](README.md)에 있다.

상세 운영 규칙: [`00_Engineering-Standards/03_Workflow/00_Document-Workflow.md`](00_Engineering-Standards/03_Workflow/00_Document-Workflow.md).
