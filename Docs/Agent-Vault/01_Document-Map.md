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
| `.vert`, `.frag`, `.comp`, `.glsl`, GPU 셰이더·GLSL | [`GLSL`](00_Engineering-Standards/01_Languages/02_GLSL.md), 해당 [`Source Map`](02_Architecture/02_Source/00_Source-Map.md), GPU 경계가 있다면 [`Contract Register`](02_Architecture/01_Contracts/00_Contract-Register.md) | 셰이더 내부 리소스 선언·주석 / CPU 코드 / CPU–GPU 계약 / Flow / Test |
| Descriptor Set/Binding, `layout(std430)`, UBO/SSBO, Push Constant, Vertex Input, Render Target | [`GLSL`](00_Engineering-Standards/01_Languages/02_GLSL.md), [`Contract Register`](02_Architecture/01_Contracts/00_Contract-Register.md), 관련 Host 코드와 셰이더 | 리소스 인터페이스·데이터 레이아웃·CPU–GPU 연결 / 관련 테스트 / Work Log |
| Workgroup, `gl_GlobalInvocationID`, Dispatch, `barrier()`, GPU pass synchronization | [`GLSL`](00_Engineering-Standards/01_Languages/02_GLSL.md), 관련 Flow/Contract, [`Verification`](00_Engineering-Standards/03_Workflow/01_Verification-and-Review.md) | 셰이더 내부 동기화 / Host Barrier / Flow / Contract / 검증 기록 |
| `.h` 공용 헤더 | 실제 포함하는 C/C++ 소스 및 활성 언어 프로필, 관련 Contract | 공개 API / Doxygen / Contracts |
| 공백 줄, 지나치게 밀집한 코드, 한 줄 if/return, 시각적 그룹화, 기능별 Region | [`Code Style`](00_Engineering-Standards/00_Core/00_Code-Style.md), [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md), 규모가 크면 [`Section & Responsibility Refactoring`](00_Engineering-Standards/00_Core/03_Section-and-Responsibility-Refactoring.md), 활성 언어 프로필 | 해당 소스 가독성 / 검증 / Work Log |
| 코드 주석, 인라인 설명, 섹션 구분, TODO/FIXME | [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md), 활성 언어 규칙 | 해당 코드 주석 / Contracts / ADR(필요 시) |
| `.py`, Python, 패키지, 타입 힌트 | [`Python`](00_Engineering-Standards/01_Languages/03_Python.md), 관련 Source | Source / Flows / 검증 결과 |
| `.sh`, `.bash`, bash, POSIX sh, 자동화·배포 | [`Shell`](00_Engineering-Standards/01_Languages/04_Shell.md), 관련 Flow | Source / Flow / 검증 결과 |
| `.js`, `.jsx`, `.ts`, `.tsx`, JavaScript, TypeScript | [`JavaScript/TypeScript`](00_Engineering-Standards/01_Languages/05_JavaScript-TypeScript.md), 관련 Source | Source / Contracts / 검증 결과 |
| 큰 클래스, 멤버 변수 과다, God Object, 섹션 재배치, 상태 구조체 추출, 책임 분리 | [`Section & Responsibility Refactoring`](00_Engineering-Standards/00_Core/03_Section-and-Responsibility-Refactoring.md), [`Code Style`](00_Engineering-Standards/00_Core/00_Code-Style.md), 활성 언어 규칙, 관련 Source / Contracts | 섹션·소스 구조 / API·상태 계약 / 테스트 / Work Log / 필요 시 ADR |
| `#region`, `#endregion`, `#pragma region`, 중첩 Region, 셰이더 접기, VS Code Custom Folding | [`Local Numbering`](00_Engineering-Standards/00_Core/01_Local-Numbering-and-Navigation.md), [`Source Comments`](00_Engineering-Standards/02_Documentation/01_Source-Comments.md), 활성 언어 문서 | 코드 섹션 마커 / 에디터 설정 / Work Log |
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

| 키워드 / 동의어 | 소스 경로 | 작업 전 문서 | 작업 후 동기화 후보 |
|---|---|---|---|
| 시작, Scene, assets, initialization | `Source/main.cpp`, `Source/Application/`, `Source/Scene/`, `Source/AssetManager/` | [[02_Architecture/00_Flows/00_Initialization/00_Scene-Load|Scene Load]], [[02_Architecture/02_Source/00_CPP/01_Application-and-Frame|Application]] | Flow, Source Map, Work Log |
| frame, clock, step scheduling | `Source/Application/FrameCoordinator.cpp`, `Source/SurfaceState/Solver/` | [[02_Architecture/00_Flows/01_Frame/00_Frame-and-Step-Scheduling|Frame Scheduling]], [[02_Architecture/02_Source/00_CPP/05_Solver|Solver]] | Flow, Source, Validation |
| state, profile, reaction | `Source/SurfaceState/`, `Shaders/Simulation/` | [[02_Architecture/01_Contracts/01_State-and-Profile|State]], [[02_Architecture/01_Contracts/06_Reactions|Reaction]] | Contract, Glossary, Tests |
| solver, flux, gather, barrier, A/B | `Source/SurfaceState/Solver/`, `Shaders/Simulation/Solver/` | [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Step]], [[02_Architecture/01_Contracts/04_Solver|Solver Contract]] | Contract, Flows, Validation |
| rendering, overlay, debug | `Source/Rendering/`, `Shaders/Rendering/`, `Shaders/Debug/` | [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Render]], [[02_Architecture/02_Source/01_Shaders/00_Index|Shaders]] | Source, Flows, Tests |
| descriptor, push constant, std430, GPU ABI | `Source/SurfaceState/GPU/`, `Source/GPU/`, `Shaders/` | [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]], [[02_Architecture/01_Contracts/07_CPU-GPU-Interface|CPU–GPU Interface]] | CPU/GLSL, Contract, Verification |
| benchmarks, performance, sparse, fp16 | `Config/Performance/`, `Scripts/`, `Tests/` | [[05_Experiments/02_Performance-Measurements|Performance]], [[03_Decisions/01_Current-Decision-Summary|Decisions]] | Performance, Work Log, ADR when warranted |

## 경로가 불명확할 때

- [`00_Home.md`](00_Home.md) → [`Source Map`](02_Architecture/02_Source/00_Source-Map.md) → 실제 코드의 호출/참조 관계를 확인한다.
- 작업이 특정 디렉터리의 상세 문서를 많이 요구할 때만 해당 위치에 **`00_Index.md`**를 추가한다. 인덱스는 제목·책임·링크만 제공하고 본문을 복제하지 않는다.
- 문서가 없거나 링크가 깨졌으면 소스와 디렉터리를 확인한 후 필요한 문서/맵을 수정한다. 존재하지 않는 파일을 읽었다고 보고하지 않는다.
- 프로젝트별 키워드 증가, 주요 경로 이동, 새 모듈 추가 시 이 맵을 갱신한다.
