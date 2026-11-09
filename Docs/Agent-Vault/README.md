# Universal Agent Vault — Setup

이 디렉터리는 **공통 템플릿의 사본**이다. 공통 템플릿을 따로 보관하고, 새 프로젝트마다 복사해 각각 독립적으로 수정한다. 자동 초기화 스크립트는 필수로 두지 않는다.

## 0. 설치

- [ ] 이 템플릿을 `<repository>/docs/Agent-Vault/` 등 프로젝트별 한 위치에 복사한다. 문서의 상대 경로를 유지한다.
- [ ] `.vscode/settings.json`은 볼트 자체를 VS Code 루트로 열 때 적용된다. **저장소 루트를 여는 경우** 필요한 설정을 저장소 루트의 `.vscode/settings.json`에도 병합한다. 기존 설정을 덮어쓰지 않는다.
- [ ] **저장소 루트의 `AGENTS.md`**에서 이 볼트의 `AGENTS.md`를 가리키도록 연결한다. 일부 에이전트는 하위 폴더의 `AGENTS.md`를 자동 발견하지 못한다. 도구별 우선순위와 위치를 확인한다.

예시 — 저장소 루트의 짧은 `AGENTS.md` (실제 경로에 맞게 조정):

```md
# Project Agent Instructions
Before every code task, read `docs/Agent-Vault/AGENTS.md` and
`docs/Agent-Vault/01_Document-Map.md`, then follow their workflow.
```

## Obsidian 외형, 선택적 플러그인 및 파일 우선 정렬

견본 `.agents.zip`에서 Blue Topaz 테마·세부 스타일·CSS 스니펫을 선별 이식했다. 견본의 AI/Git/PDF 관련 플러그인, 개인 작업 환경과 기록은 포함하지 않는다.

### 포함된 외형

- **테마:** Blue Topaz (`.obsidian/themes/Blue Topaz/`)
- **상세 스타일:** `.obsidian/plugins/obsidian-style-settings/data.json` — 테마 폴더 색·아이콘·본문 행간 등의 값. 견본의 PDF++ 전용 키만 제외.
- **CSS 스니펫:** `.obsidian/snippets/reading-rhythm.css` (`.obsidian/appearance.json`에서 활성화)
- **Markdown 링크:** 상대 경로 및 링크 이름 변경 시 자동 갱신
- **VS Code:** `.vscode/settings.json`의 `explorer.sortOrder: filesFirst`
- **Obsidian 파일 우선:** 루트 `sortspec.md` + Custom File Explorer sorting 플러그인 (`custom-sort`)

### Community Install Manager 부트스트랩

견본의 **Community Install Manager** (`automatic-installation-of-plugins`, 견본 버전 1.2.2)를 포함하며, 초기에 Style Settings와 함께 활성 플러그인으로 등록한다. 이 매니저의 설치·활성화 자동화 설정은 견본의 값으로 유지한다.

`community-plugins-list.json`에서 관리하는 플러그인은 **6개**다.

1. Community Install Manager (자동 설치 관리자)
2. Style Settings (테마 세부 설정)
3. Custom File Explorer sorting (파일 우선 정렬)
4. Smart Fold (긴 문서 접기)
5. Editor Width Slider (편집 너비, 데스크톱 전용)
6. Table Width (표 너비, 데스크톱 전용)

위 목록 이외의 플러그인은 설치 대상으로 지정하지 않는다. **Obsidian Git, PDF++, Copilot 등은 제외**한다.

`community-plugins-settings.json`에는 Blue Topaz 스타일 값과 프로젝트에 종속되지 않는 편집 너비 설정만 넣었다. 견본의 다른 프로젝트 테이블 컬럼 기록, AI 플러그인 데이터, API 키·계정 정보는 복사하지 않았다.

### 처음 열 때

1. 이 볼트를 **Obsidian에서 신뢰하는 로컬 폴더**로 열고 `설정 → 커뮤니티 플러그인`에서 커뮤니티 플러그인 사용을 허용한다. 설치는 사용자의 환경과 권한에 따라 확인이 필요할 수 있다.
2. `.obsidian/community-plugins-list.json`에 적힌 플러그인 ID 목록을 검토한다. 매니저가 자동 설치를 수행할 수 있으므로 타인이 제공한 목록은 검토 없이 사용하지 않는다.
3. Community Install Manager가 실행되어 필요한 플러그인을 설치했는지 확인한다. 설치되지 않았다면 매니저 설정에서 **Preview plugin setup import → Import plugin setup from JSON**을 실행한다.
4. 테마가 Blue Topaz로 표시되고, Style Settings에서 폴더 색·아이콘·행간 설정이 적용됐는지 확인한다.
5. Custom File Explorer sorting의 리본 버튼 또는 명령 팔레트에서 **사용자 정의 정렬 활성화**를 실행한다. 탐색기 정렬은 이름 오름차순으로 설정한다. `sortspec.md`가 파일 그룹을 먼저 정렬하도록 지정한다.
6. 다른 플랫폼에서는 데스크톱 전용 플러그인이 건너뛰어질 수 있으며, 특정 Obsidian 버전에서 설치·기능이 다를 수 있다.

**보안/이식성:** 플러그인 실행 코드는 신뢰할 수 있는 볼트에서만 활성화한다. 자동 다운로드는 네트워크 연결과 Obsidian의 설치 권한을 필요로 하며, 이 ZIP 자체가 모든 플러그인을 포함하는 것은 아니다. 견본의 Git 이력·개인 Workspace·캐시·외부 플러그인 설정은 포함하지 않았다.

## VS Code 섹션 접기

- GLSL·Shell에서 Region 접기를 쓰려면 **Custom Folding**(`maptz.regionfolder`)을 설치한다.
- 마커는 C/C++ `#pragma region`, GLSL·JavaScript·TypeScript `// #region`, Python·Shell `#region`을 사용한다. 번호가 있는 3줄 섹션 주석도 함께 유지한다.
- 자세한 표기와 예시는 [섹션·Region 표준](00_Engineering-Standards/00_Core/01_Local-Numbering-and-Navigation.md)을 참고한다. 접히지 않으면 VS Code의 언어 모드와 확장 설정을 확인한다.
- MDSS 저장소는 루트 `.vscode/settings.json`과 볼트 `.vscode/settings.json`에 `[glsl]`·`[shellscript]`의 Custom Folding 정규식 및 shader 확장자→`glsl` 연결을 설정한다. 현재 작업 환경에는 `maptz.regionfolder`와 `slevesque.shader`가 설치되어 있다. 다른 환경에서는 확장 설치·활성화를 별도로 확인한다.
- 이번 동기화의 기준 원본은 저장소 루트 `Universal-Agent-Vault/`다. 범용 규칙만 병합하고 MDSS의 Context·Architecture·프로필·실험 기록은 유지한다. JS/TS는 규칙을 제공하되 실제 프로젝트 소스가 도입될 때만 활성화한다.


### MDSS Shader 자동 접기 확인

- 프로젝트 설정에서 모든 shader(작은 include/placeholder 포함)에 comment Region을 적용한다. 현재 69개 파일·116개 영역이다.
- `foldOnOpen.targets: ["AllMarkerRegions"]`는 인식된 Region을 접는 명령 설정이다. GLSL의 마커 인식은 Custom Folding 설정/공급자가 담당한다.
- root/vault 설정의 `maptz.regionfolder.[glsl]`은 **CRLF를 허용하는 정규식**과 `defaultFoldStartRegex`를 사용한다. Maptz의 `collapseDefaultRegionsOnOpen`도 활성화했다. 이전 `(.*)$` 형식은 확장의 split("\n")에서 남은 CR을 처리하지 못했다.
- GLSL 설정을 새로 추가한 실행 중 창은 **Developer: Reload Window** 후 shader를 다시 연다. 필요하면 **Fold All Regions**를 실행한다. 파일의 언어 모드는 GLSL이어야 한다.
- [원인 재현·검증 기록](../../Build/Validation/20261109-215915-shader-folding-fix/summary.md). 실제 확장 파서에서 인식과 default Region 지정은 검증했지만, UI 연결 실패로 접힌 화면 자체는 미관찰이다.

## 1. 프로젝트 정의 — 네 문서 필수

- [ ] [`Project Brief`](01_Project-Context/00_Project-Brief.md): 실제 목표, 범위, 저장소, 엔트리 포인트, 빌드·테스트 방법을 기입한다.
- [ ] [`Glossary`](01_Project-Context/01_Glossary.md): 핵심 용어·약어를 정의한다. 모르는 개념은 `미정`으로 둔다.
- [ ] [`Requirements`](01_Project-Context/02_Requirements.md): 요구 기능, 비기능 기준, 수용 조건, 우선순위를 기록한다.
- [ ] [`Constraints`](01_Project-Context/03_Constraints.md): 플랫폼, 호환성, 표준, 라이브러리, 성능, 배포 제한 등을 기록한다.
- [ ] [`Home`](00_Home.md)의 프로젝트 정보를 실제 자료와 대조하여 채운다.

## 2. 활성 개발 프로필

- [ ] [`C++`](00_Engineering-Standards/01_Languages/01_CPP.md), [`C`](00_Engineering-Standards/01_Languages/00_C.md), [`GLSL`](00_Engineering-Standards/01_Languages/02_GLSL.md), [`Python`](00_Engineering-Standards/01_Languages/03_Python.md), [`Shell`](00_Engineering-Standards/01_Languages/04_Shell.md), [`JavaScript/TypeScript`](00_Engineering-Standards/01_Languages/05_JavaScript-TypeScript.md) 중 실제 사용하는 언어 프로필만 선택한다. C++이 주력인 경우에도 다른 언어 규칙을 무조건 읽지 않는다.
- [ ] C++ 표준(예: 98/17/20/23), 컴파일러, 빌드 시스템, 서식 규칙, 명명 프로필(Unreal / Unreal-Inspired / Existing)을 실제 프로젝트에 맞게 정한다.
- [ ] C 언어를 사용한다면 C 표준·컴파일러·함수 접두어·오류 처리 방식을 확정한다.
- [ ] GLSL을 사용한다면 Stage/확장자, GLSL 버전, Host API(Vulkan/OpenGL 등), 컴파일·검증 도구를 정하고, 필요한 CPU–GPU 계약을 등록한다. 셰이더 내부 `layout`/주석과 Host 코드를 함께 확인한다.
- [ ] [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md)와 [`Doxygen`](00_Engineering-Standards/02_Documentation/00_Doxygen.md)의 적용 범위를 확정한다. C/C++ Doxygen 설명 문장은 한국어로 작성한다.
- [ ] [`Document Workflow`](00_Engineering-Standards/03_Workflow/00_Document-Workflow.md)를 확인하고 코드 변경 전후의 문서 열람·동기화 절차를 따른다.
- [ ] [`Verification and Review`](00_Engineering-Standards/03_Workflow/01_Verification-and-Review.md)에 따라 검증 범위와 리뷰 기준을 정한다.
- [ ] [`Commit Style`](00_Engineering-Standards/03_Workflow/02_Commit-Style.md)을 프로젝트에 맞게 채택하거나 덮어쓴다.

## 3. 구조와 문서 탐색

- [ ] [`Main Flow`](02_Architecture/00_Flows/00_Main-Flow.md)에 실제 또는 **제안된** 최상위 실행 흐름을 상태와 함께 기입한다.
- [ ] [`Contract Register`](02_Architecture/01_Contracts/00_Contract-Register.md)에 실제 계약과 그 출처를 연결한다. 셰이더가 있다면 Binding·Push Constant·Buffer Layout·패스 간 동기화의 Host–GPU 관계도 등록한다.
- [ ] [`Source Map`](02_Architecture/02_Source/00_Source-Map.md)에 **중요 모듈만** 등록한다. 모든 파일에 문서 하나씩 생성하지 않는다.
- [ ] [`Document Map`](01_Document-Map.md)의 프로젝트 전용 키워드/파일 경로/문서 연결을 채운다. 링크가 존재하는지 확인한다.
- [ ] [`Architecture Organization`](00_Engineering-Standards/00_Core/02_Architecture-Organization.md)에 따라 Flows·Contracts·Source를 필요한 기능/모듈별 디렉터리로 그룹화한다.
- [ ] Home은 탐색 허브만 담당하고 프로젝트 설정은 Project Brief에서만 관리한다.
- [ ] 서로 다른 문서에서 같은 정의를 중복 유지하지 않는지 확인한다. 불필요한 `00_Overview.md`는 만들지 않는다.

## 4. 첫 변경으로 작업 흐름 검증

- [ ] 작업 시작 시 `AGENTS.md` + `01_Document-Map.md` + 관련 문서를 읽는다.
- [ ] 코드를 수정하고 실제 실행한 검증만 기록한다.
- [ ] 영향 문서를 검토·갱신하고 [`Work Log`](04_Development/01_Work-Log.md)에 **작업 단위 1건**을 남긴다.
- [ ] 설계상 중요한 결정이면 [`ADR`](03_Decisions/00_ADR-Index.md)에 기록한다.
- [ ] 마지막으로 Markdown 링크, 소스 경로, 활성 언어 프로필, VS Code 설정을 확인한다.

## 범용 템플릿과 프로젝트 사본

- **공통 템플릿에 보관:** 재사용되는 규칙, 문서 스켈레톤, 공통 예시 및 워크플로.
- **프로젝트 사본에만 보관:** 실제 코드 경로, 프로젝트 고유 지식, 실험 수치, 검증 결과, ADR, Work Log.
- 프로젝트에서 검증한 개선이 일반적으로 유용하면 **선별하여** 공통 템플릿에도 반영한다. 프로젝트 고유 사실을 자동 병합하지 않는다.
- 문서가 아직 미작성인 상태에서 관련 작업을 진행해야 한다면, 읽지 않은 내용을 추측하지 말고 필요 문서를 실제 근거로 채운다.

## Universal 규칙 동기화 — 5.5

2026-11-09 사용자 지정 5.5 갱신을 로컬 Universal-Agent-Vault의 `93dbbed`(큰 클래스·파일 2단계 규칙)에서 반영했다. [공통 지침](00_Engineering-Standards/00_Core/03_Section-and-Responsibility-Refactoring.md)은 1단계 섹션 정리와 2단계 책임 추출을 독립된 작업·검증으로 다룬다. 예시의 T 접두어는 범용 예시이며 이 프로젝트의 실제 C++ 코드는 Project Brief의 Unreal-Inspired(F/E/b)를 따른다. 기존 프로젝트 라우팅·계약·실험 기록과 shader folding 설정을 보존한다.
