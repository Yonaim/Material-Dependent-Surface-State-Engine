# Document Routing Map

> **모든 코드 작업 전 반드시 읽는 경량 탐색 인덱스.** 문서 본문을 여기에 복제하지 않는다. 전체 읽기 순서와 실제 파일 정렬 순서는 서로 독립적이다.

## 선택 절차

1. **작업 유형**: 기능 / 버그 / 리팩터링 / API / 테스트 / 성능 / 빌드 / 문서 등
2. **소스 경로와 확장자**: 실제 수정할 파일 및 직접 영향받는 호출자·소비자
3. **키워드**: 요청에 등장하는 개념, [`01_Project-Context/01_Glossary.md`](01_Project-Context/01_Glossary.md)의 동의어·약어
4. 아래 표에서 읽을 문서를 선택한다. **필수 공통 + 활성 언어 + 직접 관련 영역**만 읽고 부족할 때 확장한다.
5. 작업 후에는 갱신 후보를 점검하고 [`04_Development/01_Work-Log.md`](04_Development/01_Work-Log.md)에 기록한다.

## 공통 라우팅

| 신호 (작업 / 키워드 / 경로) | 코드 변경 전 읽기 | 변경 후 동기화 후보 |
|---|---|---|
| **모든 코드 변경** | [`Code Style`](00_Engineering-Standards/00_Core/00_Code-Style.md), [`Document Workflow`](00_Engineering-Standards/03_Workflow/00_Document-Workflow.md), 활성 언어 규칙 | 해당 Architecture·Context·Experiments, **Work Log 필수** |
| `.cpp`, `.cc`, `.cxx`, `.hpp`, C++, RAII, 템플릿 | [`C++`](00_Engineering-Standards/01_Languages/01_CPP.md), 관련 [`Source Map`](02_Architecture/02_Source/00_Source-Map.md) | Source / Contracts / Flows / Doxygen 주석 |
| `.c`, C17/C11, C API, 포인터·버퍼 계약 | [`C`](00_Engineering-Standards/01_Languages/00_C.md), 관련 [`Source Map`](02_Architecture/02_Source/00_Source-Map.md) | Source / Contracts / C API 주석 / Tests |
| `.h` 공용 헤더 | 실제 포함하는 C/C++ 소스 및 활성 언어 프로필, 관련 Contract | 공개 API / Doxygen / Contracts |
| 코드 주석, 인라인 설명, 섹션 구분, TODO/FIXME | [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md), 활성 언어 규칙 | 해당 코드 주석 / Contracts / ADR(필요 시) |
| `.py`, Python, 패키지, 타입 힌트 | [`Python`](00_Engineering-Standards/01_Languages/03_Python.md), 관련 Source | Source / Flows / 검증 결과 |
| `.sh`, `.bash`, bash, POSIX sh, 자동화·배포 | [`Shell`](00_Engineering-Standards/01_Languages/04_Shell.md), 관련 Flow | Source / Flow / 검증 결과 |
| API, 인터페이스, ABI, 스키마, 소유권, 불변식 | [`Contract Register`](02_Architecture/01_Contracts/00_Contract-Register.md), 관련 Source, C/C++ 공개 API면 [`Doxygen`](00_Engineering-Standards/02_Documentation/00_Doxygen.md) | Contract / Source / 테스트 / Doxygen |
| 시작·종료·라이프사이클·프레임·파이프라인 | [`Main Flow`](02_Architecture/00_Flows/00_Main-Flow.md), 관련 Source | Flows / Source |
| 새 기능, 범위 변경, 기대 동작 | [`Requirements`](01_Project-Context/02_Requirements.md), [`Constraints`](01_Project-Context/03_Constraints.md), 관련 Flow | Requirements / Contracts / Tests |
| 프로젝트 고유 용어, 데이터 의미, 약어 | [`Glossary`](01_Project-Context/01_Glossary.md), 관련 Contract | Glossary / Contract / Map |
| 버그, crash, regression, 오류 | 관련 Source, [`Validation & Tests`](05_Experiments/01_Validation-and-Tests.md), 관련 Contract | 테스트 / Source / Contract / Work Log |
| 성능, benchmark, memory, latency | [`Performance Measurements`](05_Experiments/02_Performance-Measurements.md), Constraints, 관련 Source | 성능 근거 / ADR(필요 시) / Work Log |
| 새 모듈·기능 단위 문서 구조, Flows/Contracts/Source 그룹화 | [`Architecture Organization`](00_Engineering-Standards/00_Core/02_Architecture-Organization.md), [`Source Map`](02_Architecture/02_Source/00_Source-Map.md) | 관련 하위 문서 / Map / 지역 Index(필요 시) |
| 구조 선택, 의존성 방향, 트레이드오프 | 관련 Flow/Contract/Source, [`ADR Index`](03_Decisions/00_ADR-Index.md) | ADR / Architecture / Work Log |
| Markdown, 문서 이동, 탐색 맵 | [`Markdown`](00_Engineering-Standards/02_Documentation/02_Markdown.md), 관련 문서 | 링크 / Map / 실제 변경 문서 |
| 검증 계획, 변경 영향, 코드 리뷰 | [`Verification and Review`](00_Engineering-Standards/03_Workflow/01_Verification-and-Review.md), 관련 Requirements/Contracts | 검증 결과 / Work Log |
| 커밋 작성 | [`Commit Style`](00_Engineering-Standards/03_Workflow/02_Commit-Style.md) | 해당 기록 (커밋 실행 여부와 별개) |

## 프로젝트별 라우팅 — MDSS Engine

| 키워드 / 동의어 | 소스 경로 패턴 | 우선 읽을 문서 | 변경 후 검토할 문서 |
|---|---|---|---|
| `Surface State`, Capacity, SRProfile, Profile v5 | `Source/*Surface*`, `Shaders/*` | [[02_Architecture/01_Contracts/01_State-and-Profile|State & Profile]], [[02_Architecture/01_Contracts/02_Assets-and-Profiles|Assets & Profiles]] | 같은 Contract / Source Map / Regression |
| `Flux`, Outgoing, Gather, timestep | `Source/*Solver*`, `Shaders/*solver*` | [[02_Architecture/01_Contracts/04_Solver|Solver]], [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Simulation]] | Solver Contract / Validation / Perf |
| `Reaction`, consumes, produces, inflowFraction | `Shaders/*reaction*`, reaction 관련 C++ | [[02_Architecture/01_Contracts/06_Reactions|Reaction]], [[02_Architecture/01_Contracts/07_CPU-GPU-Interface|CPU–GPU]] | Reaction Contract / Integration Tests |
| `Geometry`, `UV Seam`, `Meso`, `Dirty`, `Overlay` | geometry/overlay C++ 및 GLSL | [[02_Architecture/01_Contracts/03_Geometry|Geometry]], [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Rendering]] | Geometry / Source / Perf |
| `Vulkan`, `Descriptor`, `Shader ABI` | GPU Resource / Pipeline / GLSL | [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]], [[02_Architecture/02_Source/01_Shaders/00_Index|Shader Map]] | Layout / Source / GPU Regression |
| 빌드, CMake, preset, shader manifest | `CMakeLists.txt`, `CMake/*`, `CMakePresets.json`, `Scripts/build.*` | [[01_Project-Context/03_Constraints|Constraints]], [[00_Engineering-Standards/03_Workflow/03_Pre-Build-Formatting|Pre-Build Formatting]], [[00_Engineering-Standards/03_Workflow/05_Build-System-Selection|Build-System Selection]] | Current Work의 포맷 훅 상태 / Validation |
| Python 성능·연구 분석, PowerShell/Bash 실행 | `Scripts/**/*.py`, `Scripts/**/*.ps1`, `Scripts/**/*.sh`, `Tools/*.py` | 해당 언어 규칙, [[02_Architecture/02_Source/00_Source-Map|Source Map]], [[05_Experiments/01_Validation-and-Tests|Validation]] | 관련 실험 기록 / Work Log |

> 소스 경로 패턴은 탐색 힌트이며 실제 저장소의 파일 존재를 확인한 뒤 따른다.

## 경로가 불명확할 때

- [`00_Home.md`](00_Home.md) → [`Source Map`](02_Architecture/02_Source/00_Source-Map.md) → 실제 코드의 호출/참조 관계를 확인한다.
- 작업이 특정 디렉터리의 상세 문서를 많이 요구할 때만 해당 위치에 **`00_Index.md`**를 추가한다. 인덱스는 제목·책임·링크만 제공하고 본문을 복제하지 않는다.
- 문서가 없거나 링크가 깨졌으면 소스와 디렉터리를 확인한 후 필요한 문서/맵을 수정한다. 존재하지 않는 파일을 읽었다고 보고하지 않는다.
- 프로젝트별 키워드 증가, 주요 경로 이동, 새 모듈 추가 시 이 맵을 갱신한다.

## 빌드·포맷팅 라우팅

프로젝트 Setup 또는 빌드/포맷팅 변경 전에는 [`Pre-Build Formatting`](00_Engineering-Standards/03_Workflow/03_Pre-Build-Formatting.md)을 읽고 `.clang-format`, `Tools/` 배치 및 실제 빌드 선행 실행을 확인한다.

## 프로젝트 Setup·빌드 체계

신규·기존 프로젝트 초기화에는 [`Project Setup`](00_Engineering-Standards/03_Workflow/04_Project-Setup.md), [`Setup Questionnaire`](00_Engineering-Standards/03_Workflow/06_Setup-Questionnaire.md), [`Build-System Selection`](00_Engineering-Standards/03_Workflow/05_Build-System-Selection.md)과 [`Build Assets`](Assets/Build/README.md)를 사용한다. 저장소를 먼저 조사하고 **필수 미정 질문만** 한다. 기존 빌드 시스템은 무단 교체하지 않는다.
