# Work Log — 코드 변경 및 문서 동기화

> **모든 코드 변경 작업에서 작업 단위 최소 1건 기록 필수.** 상세 설계는 Architecture/ADR, 실제 테스트 결과는 Experiments에 기록한다.

| 날짜 | 작업 / 코드 범위 | 읽은 주요 문서 | 갱신 문서 또는 영향 없음 근거 | 검증 상태 |
|---|---|---|---|---|
| `YYYY-MM-DD` | `{{TASK_AND_CODE_PATHS}}` | `{{READ_DOCS}}` | `{{UPDATED_DOCS_OR_NO_IMPACT_REASON}}` | `{{COMMAND_AND_RESULT_OR_NOT_RUN}}` |

## 작성 원칙

- 소규모 작업은 한 줄이면 충분하다. 여러 파일의 동일 작업은 하나로 묶을 수 있다.
- 문서 갱신이 불필요해도 **왜 영향이 없는지** 간결히 기록한다.
- `미실행`, `미검증`, `실패`, `통과`를 구분한다. 실행하지 않은 명령을 성공 처리하지 않는다.
- 길어지면 월별 로그로 분리하고 여기에는 짧은 링크 인덱스를 남긴다.


## 2026-11-09 — 범용 볼트 이관 및 소스 구획 정리
- **읽은 문서:** 기존 프로젝트 Policy·Glossary·Architecture(Flows/Contracts/Source)·Development·Experiments 및 Universal Agent Vault 규칙·언어·GLSL·주석 문서.
- **코드 변경:** 기존 `#pragma region`에 번호·섹션 구분선 추가, 주요 엔트리 포인트의 구획 정리, GLSL 코드의 접기용 섹션 주석 추가. **알고리즘·상수·API·호출 순서·GPU 동기화는 의도적으로 변경하지 않음.**
- **문서 동기화:** 새 볼트의 Context/Architecture/Decisions/Development/Experiments/Research로 기존 문서 보존·재배치. 새 Document Map·Project Brief·Requirements·Constraints·Home 작성.
- **검증:** ZIP 무결성, 이전/이후 코드 토큰 대조(공백·주석·`#pragma region`을 제외), 파일 목록·링크 탐색, 셸/Python 문법 검사를 수행할 실행함 (C/C++/GLSL 원본 비주석 코드 줄 동일, 44개 Python AST 검사, 4개 Shell 문법 검사, Region 균형 및 문서 링크). 실제 C++ 빌드/셰이더 컴파일/런타임·GPU 테스트는 프로젝트 루트 `CMakeLists.txt` 등 누락으로 **미실행**.


## 2026-11-09 — Engineering Standards 준수 감사 및 네이밍 프로필 확정
- **읽은 문서:** AGENTS·Document Map, Code Style·Local Numbering·Architecture Organization, C++·GLSL·Python·Shell, Source Comments·Doxygen·Markdown, Workflow·Verification·Commit Style, Project Brief·Constraints, Source Map·CPU–GPU Interface 및 기존 Work Log.
- **코드 변경:** 없음. 사용자 결정에 따라 목표 프로필을 Unreal-Inspired로 문서화하고 현재 소스의 미준수 항목을 감사했다. 소스 리네이밍·자동 포맷 미수행.
- **문서 동기화:** Project Brief의 활성 네이밍·빌드/포맷 설정, Constraints의 실제 파일 존재 여부 갱신. [준수 감사 보고서](10_Engineering-Standards-Audit.md)에 근거·전수 패턴 검사와 수동 검토 한계·전환 순서 기록. API·ABI·실행 흐름이 바뀌지 않아 Architecture 변경 없음.
- **검증:** Python AST 44/44, Bash 구문 5/5 통과. clang-format 20.1.8: C++ 195개 중 35개 불일치. 실제 Manifest의 셰이더 75개 엔트리·변형을 검증 매크로 0/1로 컴파일하고 spirv-val 검사: 150/150 통과. C++ 빌드·CTest·GPU 실행·성능·ABI Reflection은 미실행. 명명/주석 집계는 정적 패턴 검사이며 완전한 컴파일러 AST/의미 검증이 아님.


## 2026-11-09 — Code Style 구조·의미 체크리스트 후속 리뷰
- **읽은 문서:** AGENTS·Document Map·Code Style, Scene/Input/Debug UI 및 Application/Frame Source 문서. 기존 규칙 감사 결과와 실제 Scene/Camera/DemoAnimation/Application/Renderer 흐름 대조.
- **코드 변경:** 없음. 접두어와 별개로 상위 흐름·추상화 수준·책임·오류 경로를 리뷰했다.
- **문서 동기화:** [규칙 감사 보고서](10_Engineering-Standards-Audit.md)에 §05의 6개 체크리스트 판정, 구체적 미준수와 준수 사례, 보존할 순서/계약 및 검토 범위를 추가. API/ABI/동작 변경이 없어 Architecture 변경 없음.
- **검증:** 정적 소스 수동 리뷰만 수행. 이번 후속 작업에서 빌드·테스트·GPU 실행·성능 검증 미실행. 기존 셰이더 검사 결과와 구분한다.


## 2026-11-09 — GLSL 규칙 후속 리뷰
- **읽은 문서:** AGENTS·Document Map·GLSL, CPU–GPU Interface 및 Debug/Shared Shader Source 문서. 기존 Code Style 감사 기준과 함께 Debug/Solver/Overlay 및 CPU pipeline/draw 경로 대조.
- **코드 변경:** 없음. Shader 구조 패턴 전수 검색, 주요 entry/helper의 책임·로컬 계약·경계/barrier 조건을 수동 리뷰했다.
- **문서 동기화:** [감사 보고서](10_Engineering-Standards-Audit.md)에 GLSL 규칙별 근거, SurfaceDebug.vert/frag 개선 대상, bounds 전제조건 확인 과제, 정상 사례 및 검토 한계 추가. API/ABI/실행 변경이 없어 Architecture 변경 없음.
- **검증:** 이번 작업은 정적 리뷰만 수행. 새 shader 컴파일·Reflection·Pipeline 생성·GPU 실행·성능 측정 미실행. 이전 150개 compile/spirv-val 결과와 구조·안전성 리뷰를 구분함.


## 2026-11-09 — 변경 전 최소 빌드·테스트 기준선 확보
- **읽은 문서:** AGENTS·Document Map·Verification and Review, Validation and Tests의 테스트/fixture·실행 기준, 기존 build script 및 CMake 테스트 설정.
- **코드 변경:** 없음. 스냅샷 808개·SHA-256·HEAD·미커밋 patch 저장 후 별도 Debug build/CTest 및 Manifest shader 검증 실행. 보조 runner/로그는 결과 디렉터리에만 저장.
- **문서 동기화:** Validation and Tests 상단에 이번 환경의 실제 결과·한계·[증거 위치](../../../Build/Validation/20261109-210554-pre-refactor/summary.md) 추가. API/ABI/실행 흐름 변경이 없어 Architecture 변경 없음.
- **검증:** configure/전체 build 성공. CTest 14개: 7 통과·7 실패·0 skip(종료 코드 8). 셰이더 150개 glslc/spirv-val 통과. 실행 중 입력 해시 변경 0·ZIP CRC 정상·기존 Build cache 유지. 실패는 assertion 5개/Scene 입력 누락 1개/GPU 한도 1개로 구분하고 실제 failed 상태 보존. 성능/대규모 시각·물리 동등성은 미실행.


## 2026-11-09 — Universal 규칙 동기화·전 언어 folding 표기·Shader 접기 반영
- **읽은 문서:** 양쪽 AGENTS·Document Map, Universal Code Style·Local Numbering·GLSL·Python·Shell·JS/TS·Source Comments·README, 현재 Workflow·Verification·Shader Source Index·Current Work. 설치된 Custom Folding README/schema 및 Shader 확장 language ID도 확인.
- **코드/설정 변경:** Shader 50개에 84개 comment region, 작은 19개 유지. 기존 Source/Tests 수정 없음. root/vault VS Code에 glsl/shellscript folding 정규식과 shader 언어 연결. 원본 Universal 볼트 수정 없음.
- **문서 동기화:** 공통 규칙 7개 및 AGENTS/README/공통 라우팅 병합, Shader Source Index·Current Work의 후속 순서 갱신. 프로젝트 Context·네이밍·기존 실험 보존. 계산/API/ABI/호출/barrier가 동일해 Contract/Flow 본문 변경 없음.
- **검증:** 69개 주석 제외 코드 동일·84개 region 균형/번호·기존 개행 유지. shader 150개 glslc/spirv-val 통과 및 직전 기준선 SPIR-V와 모두 바이트 동일. 전체 C++/CTest/GPU 실행은 주석 전용 변경이라 반복하지 않음. UI folding 화면은 미확인. [상세 증거](../../../Build/Validation/20261109-211718-vault-region-folding/summary.md).


## 2026-11-09 — 현재 상태 분할 커밋 및 다음 작업 확정
- **읽은 문서:** 루트/볼트 AGENTS·Document Map·Commit Style와 기존 감사/기준선/Current Work 기록.
- **작업:** refactor/engineering-standards에서 기존 미커밋 상태를 기능/연구/실행 도구/공통 소스 정리/GLSL folding/볼트 이관으로 분리해 보존했다. 기능 변경과 섹션·개행 변경이 같은 파일에 있던 부분은 index에서 분리하고 작업 폴더 내용은 유지했다.
- **커밋 단위:**
  - `405a5cc Feat(Rendering): Burn·Scorched 시각화와 반응 비교 씬 생성기 추가`
  - `6a979d3 Chore(Research): 결합·모듈 기준선과 저작 검증 도구 보존`
  - `d16e1a2 Build(Scripts): 기존 빌드 산출물 실행 진입점 추가`
  - `49390b0 Style(Code): 기존 섹션 번호와 주석·소스 개행 정리`
  - `a73ac04 Style(Shaders): 책임별 주석 Region folding 적용`
  - `Docs(AgentVault): 공통 규칙 이관과 감사·기준선 기록 보존` — 이 작업 기록을 포함하는 마지막 커밋.
- **문서 동기화:** 현재 Agent-Vault, JS/TS 및 언어별 folding 규칙, Source/Contracts/Flows, 감사·검증·연구 기록을 기존 볼트 삭제와 함께 이관. 독립 Git 저장소인 로컬 Universal-Agent-Vault는 .gitignore로 제외하고 실제 반영된 규칙은 본체 볼트에 보존했다. 루트 .vscode 및 Build 검증 원자료는 기존 ignore 정책에 따라 로컬 유지하며, 공유 가능한 볼트 .vscode 설정은 이관 문서에 포함한다.
- **검증:** 분할 전 작업 파일 531개 존재/해시 검사 일치. 공통 정리 staged 297개는 C++/GLSL의 주석·Region·공백을 제외한 코드 동일, 다른 설정/fixture/스크립트는 CRLF/LF 정규화 후 동일. 연구 Python 27개 AST·계획 JSON 확인. 기존 전체 Debug build 성공, CTest 7 통과/7 실패/skip 0, shader 150개 검사·기준선 바이트 동일 결과를 보존. 이번 분할 작업에서 전체 빌드/CTest/실험을 재실행하지 않았다.
- **남은 제한:** 비교 Scene 생성기는 보존했으나 생성된 ReactionTransitionComparison.Scene·Profile·Mesh는 현재 checkout에 없어 실행 전 생성이 필요하다. 기존 assertion 실패·누락 입력·GPU 한도 문제는 수정하지 않았다.
- **다음 구현:** 기존 3번 C++ 구조 정리 — LoadDemoAnimation → MainLoop → RecordCommandBuffer. 이번 작업에서 해당 리팩터링이나 push는 수행하지 않는다.


## 2026-11-09 — C++ 상위 흐름 정리 (기존 3번)
- **읽은 문서:** AGENTS·Document Map·Code Style·C++·Local Numbering·Source Comments·Workflow·Verification·Constraints, Application/Scene/Rendering Source, Render Flow·CPU–GPU Interface 및 기존 기준선 기록.
- **코드 변경:** LoadDemoAnimation을 파일/설정/weather/tracks/contact 단계로, MainLoop를 입력·시간·demo contact·GPU wait·마무리 단계로, RecordCommandBuffer를 simulation/inspection·display preparation·graphics/viewport·UI/end 단계로 추출. Renderer private frame/view 상태는 자원을 장기 소유하지 않으며 기존 vector를 move. 원래 값 계산·검증·예외·Vulkan call/query/barrier 순서 유지. 오류 정책·타입명 일괄 변경·GLSL 구조 변경은 후속 범위.
- **문서 동기화:** Application/Scene/Rendering Source·Render Flow·Current Work·Validation 갱신. Contract/ABI 데이터와 수용 기준은 변경되지 않아 계약 본문 변경 없음. 감사의 원래 관찰은 역사 기록으로 유지하고 Current Work에서 완료 상태 안내.
- **검증:** Debug 전체 build 성공. CTest 7 통과/7 실패/skip 0이며 테스트별 상태/이유가 기준선과 동일. 파서 35개 old/new 비교 동일, Vulkan call 지점 81개 정적 순서/인자 동일, render-only old/new 5프레임 PPM/settings/validation 동일. simulation Scene은 양쪽 모두 기존 33-buffer GPU 한도로 초기화 실패. overlay 전체 동등성·성능·장시간·UI suspend 조작은 미검증. clang-format 5개 통과 및 CRLF-aware diff check. [실행 증거](../../../Build/Validation/20261109-213530-cpp-top-down/summary.md).
- **커밋:** 이번 구현 변경은 미커밋, push 없음. 다음 작업은 기존 4번 GLSL 계산 구조 정리.


## 2026-11-09 — Shader 누락 Region 및 CRLF 자동 접기 수정
- **읽은 문서:** AGENTS·Document Map·Code Style·GLSL·Verification·Shader Source Index·Project Brief. 설치된 Maptz/Fold on Open/Custom Folding 확장 구현과 VS Code Fold All Regions 처리도 로컬 코드로 확인.
- **코드/설정:** 짧아서 제외했던 shader 19개도 사용자 요청에 따라 comment Region 추가(전체 69개·103개). root/vault 설정에서 CRLF 대응 regex·defaultFoldStartRegex·자동 접기 지정. 사용자 전역 settings와 기존 C++ 리팩터링은 수정하지 않음.
- **원인:** 이전 시작 regex가 확장 parser에 남은 CR을 소비하지 못함을 실제 설치 parser에서 재현. AllMarkerRegions 값 자체는 유효하며 이미 인식된 영역만 접는 명령임. 초기 언어 provider 등록을 위해 Reload Window가 필요할 수 있음.
- **문서 동기화:** Project Brief에 모든 shader 적용 정책을 명시하고 README·Shader Source Index·Current Work·Validation 갱신. 계산/리소스/ABI/패스 순서 변경이 없어 Flow/Contract 본문 변경 없음.
- **검증:** 실제 설치 provider를 mock 값 API와 실행해 69개·103개 typed/default Region 확인. LF/CRLF 회귀 통과, 69개 주석 제외 코드·개행 동일. 150개 glslc/spirv-val 통과 및 기준선 SPIR-V 모두 바이트 동일. CUA native pipe 연결 실패로 UI 화면은 미확인. 전체 C++/CTest/GPU 실행 반복 없음. [증거](../../../Build/Validation/20261109-215915-shader-folding-fix/summary.md). 커밋/push 없음.

## 2026-11-09 — GLSL 상위 흐름 정리 (기존 4번)
- **읽은 문서:** AGENTS·Document Map·Code Style·GLSL·Source Comments·Verification, Solver/CPU–GPU 계약, Debug/Overlay/Solver Source와 기존 검증 기록 및 CPU 바인딩·dispatch 구현.
- **코드 변경:** Debug의 조회·변위/미분·진단 출력, Overlay의 dirty scan·예약·baseline/삼각형 compaction·segment 출력/combined sampling·normal·layer 합성, Solver의 raw outgoing·gather·state 갱신·reaction 적분을 책임별 helper로 추출. Geometry/Coverage helper 정의를 main 아래 배치. 총 13개 파일이며 기존 layout·수식·접근/분기 우선순위·barrier 참여·variant scope 보존. Shader folding은 전체 69개·116개 영역. 기존 C++ 5개 및 테스트 기대값 유지.
- **문서 동기화:** Debug/Overlay/Solver Source·Shader Index·README·Current Work·Validation 갱신. Contracts/Flows 검토 결과 pass 순서·수용 기준·ABI가 같아 계약/흐름 본문 변경 불필요.
- **검증:** 최종 build 성공, shader 150개 컴파일/spirv-val 통과 및 reflection 동일. CTest 7 통과/7 실패·skip 0, 상태/실패 원인 문구 기준선 동일. 별도 debug helper는 전후 동일한 기존 assertion 실패이고 도달한 GPU 출력 16개 동일, validation ERROR 없음. Region provider 116개 인식, CRLF-aware whitespace 확인. Overlay 전체·모든 mode/variant GPU 출력·성능/장시간/다른 장치는 미검증. [증거](../../../Build/Validation/20261109-221158-glsl-top-down/summary.md).
- **커밋:** 미커밋·push 없음. 다음 단계는 기존 5번 Unreal-Inspired 네이밍.

## 2026-11-09 — Unreal-Inspired 네이밍 전환 (기존 5번)
- **읽은 문서:** AGENTS·Document Map·Code Style·C++·Verification·Source Comments·Python, Project Brief·Constraints, Source Map·C++ Index·CPU–GPU Interface·GPU Layout/ABI Source 및 직전 검증 기록.
- **코드 변경:** 일반 타입/비템플릿 별칭 F·enum E·scalar bool b로 선언/정의/호출자/테스트/Research를 동기화(186개 C++ 파일). libclang 선언·참조 기반 bool 치환 후 generic lambda 및 기존 오류가 있는 실험 식의 누락 참조를 보완. TScopeExit/외부 타입, 파일명, 문자열/Scene schema, GLSL 선언·리소스/byte layout 유지. Python source marker 1개와 shader CPU 대응 주석 3개 동기화.
- **문서 동기화:** 현재 Architecture/Source/Contracts/Flows의 C++ 심벌 참조·GPU CPU/GLSL 명칭 구분, Project Brief·Current Work·Validation 갱신. 과거 실험/Work Log/ADR의 당시 이름은 보존. 계산·수명·pass/barrier·저장 형식 변경 없음.
- **검증:** 최종 Debug build 성공. CTest 7 통과/7 실패·skip 0, 상태/실패 원인 문구 동일. AST 100 TU 오류 0·scalar bool 553개/타입 규칙 적합. 계획 외 코드 토큰 변경 없음·문자열/개행 유지. shader 150개 통과·SPIR-V 바이트 동일, render-only 전후 화면/settings/validation 동일·ERROR 0. Source/Tests format dry-run 181개 통과. Research syntax 진단은 전후 동일한 기존 8개이며 별도 연구 실행/전체 overlay·성능·다른 플랫폼은 미검증. [증거](../../../Build/Validation/20261109-222214-unreal-naming/summary.md).
- **커밋:** 미커밋·push 없음. 다음 단계는 기존 6번 문서·스타일 정리.

## 2026-11-09 — Universal 5.5 규칙 반영 및 FDebugUI 1단계 구획
- **문서 먼저 반영:** 로컬 Universal `93dbbed`의 공통 2단계 지침, Code Style·언어 6개·검증·워크플로·구현 템플릿·AGENTS를 반영. Document Map은 공통 행만 추가하고 프로젝트 라우팅/규칙/설정 보존. 현재 우선순위와 후속 후보를 `11_Section-and-Responsibility-Plan.md`에 기록.
- **1단계 코드:** DebugUI.h에 Public API/기능별 메서드/관련 상태의 14개 섹션과 Region 추가. 멤버 선언·초기화·접근 지정자·코드 토큰 순서 유지.
- **검증:** 주석/Region 제외 토큰 동일, CRLF 및 Region 균형/지역 번호 확인. MDSS Debug target build 성공. 별도 stage1 헤더/검증 파일 보존. 이 구획만으로 책임 분리가 완료됐다고 보지 않음.
- **다음 별도 작업:** private profiling 상태와 Reset/Accumulate/Publish 동작 추출·검증. 커밋/push 없음.

## 2026-11-09 — FDebugUI 2단계 Profiling 책임 추출
- **읽은 문서:** 반영된 5.5 공통 지침·Code Style·C++·Verification·Document Workflow, Debug UI Source와 현재 초기화/리셋/metric 소비자, 기존 검증/CPU–GPU 경계 기록.
- **별도 코드 변경:** 1단계 보존 후 ProfilingUIState.h/.cpp의 FProfilingUIState에 값 소유 필드 13개·sample record·Reset/Accumulate/PublishWindow 추출. FDebugUI는 기존 API/metric 수집/시간·mode guard/ImGui 출력·reset 호출을 유지한다. 새 할당/간접 소유권·settings schema·GPU ABI 변경 없음. private layout 변화로 소비자를 함께 빌드.
- **검증:** 원래 transition 본문 정규화 및 UI inline expansion 토큰 대조 일치. reset 호출자 6개 파일 동일. 기존 코드 reference와 2,000프레임·4,023회 상태 비트 일치. 전체 build 성공, CTest 7 통과/7 실패·skip 0와 원인 문구 동일, render-only 전후 화면/settings/validation 동일·ERROR 0. format/Region/whitespace 통과. 실제 모든 UI/장시간·성능·다른 장치 및 기존 GPU 한도 경로 미검증.
- **동기화:** Debug UI Source·5.5 계획·Current Work·Validation 갱신. API/직렬화/GPU·pass 흐름 의미가 같아 Contracts/Flows 본문 추가 수정 불필요. [증거](../../../Build/Validation/20261109-224136-vault55-debugui/summary.md).
- **상태:** FDebugUI 첫 적용만 완료. 나머지 상태/대형 파일은 후보별 검토 후 별도 두 단계로 수행. 기존 미커밋 작업 보존·이번 커밋/push 없음.

## 2026-11-09 — Universal 5.5 프로젝트 전체 1단계
- 사용자 지시로 FDebugUI 첫 적용에서 Source/Shaders/Scripts/Tests/루트 실행 도구 전체(319개 파일)로 확대. 외부/생성물/중첩 템플릿 볼트/자산은 제외. 150줄은 후보 탐색용이며 작은 단일 책임 파일은 무리하게 분리하지 않는다.
- 먼저 42개 파일의 누락 구획 정리: Renderer/SurfaceStateSystem 등 헤더·테스트 시나리오 및 Python의 imports/config/helper/main/execution 구획. 기존 GPU shader 구획과 작은 실행 진입점은 유지.
- C++ 코드 토큰·선언/접근 순서 동일, Python AST 동일, 전체 Debug build 성공. stage1 스냅샷/검증을 보존한 뒤 실제 책임 추출을 별도 작업으로 진행한다. 기존 작업/실패 정책 보존·커밋 없음.

## 2026-11-09 — Universal 5.5 프로젝트 전체 2단계 및 전수 대조 완료
- **읽은 문서:** AGENTS·Map·Code Style·공통 2단계·활성 C++/GLSL/Python/Shell·Verification·현재 Source/CPU–GPU·Solver·Rendering/UI 경계와 기존 검증.
- **코드:** UI·Renderer·Solver 8개 값 소유 상태(97개 필드), frame timing/solver preparation·view popup/설명·world geometry 모듈 추출, Python 7개 처리 함수와 Shell preflight/cache 분리. GLSL 나머지 6개 entry의 helper 구현 배치를 main 아래로 정리. 자원/초기값/생성·리셋·종료/공개 API·문자열/직렬화·GPU record/dispatch/barrier 의미 유지.
- **전수 판단:** 323개 구현 + CMake 2개를 대조. 작은 파일·이미 단일 책임 또는 고정 ABI/공유 수명 단위에는 불필요한 추출을 하지 않고 파일별 이유를 기록. 구획만 추가한 파일을 책임 분리 완료라고 대체하지 않음.
- **검증:** 실제 추출 상태·동작 역전개 시 C++ 197개 토큰/5개 Python AST 동일. build 성공, CTest 7 통과/7 실패·원인 문구 동일. shader 150개 통과·reflection 동일. render-only 전후 출력/설정/validation 동일, geometry 3개 비트 동일, Python 정상/오류 보고서 및 Bash 6개 fixture 비교 동일. 구문/format/Region/링크/whitespace 통과. 연구 도구 기존 8개 진단 유지. native PowerShell·전체 연구/UI/장시간/성능/다른 플랫폼·기존 GPU 한도 경로 미검증.
- **동기화:** 전체 대조표·5.5 계획·Current Work·Rendering/Solver/UI/Shader Source·CPU–GPU 경계·Validation 갱신. Flow 의미와 pass 계약은 검토 후 유지. [원자료](../../../Build/Validation/20261109-230833-vault55-project-wide/summary.md).
- **상태:** 전체 적용 작업 완료, 미커밋·push 없음. 다음 기존 6번 문서·스타일 정리로 진행 가능.

## 2026-11-09 — 누적 변경 분할 커밋

- 사용자 요청에 따라 기존 변경을 의미 단위로 분리한다. 소스 작업 폴더를 유지하고 보존된 단계별 스냅샷으로 인덱스를 구성했다.
- 검증: 기존 단계별 빌드·테스트·동등성 검증을 근거로 사용한다. 각 중간 커밋을 별도로 빌드한 것으로 간주하지 않는다. 커밋마다 인덱스 공백 검사를 수행한다.
- 분리 항목:
  - Refactor(CPP): 데모 로딩·프레임·명령 기록의 상위 흐름 분리
  - Fix(Shaders): 누락 Region과 CRLF 자동 접기 설정 보완
  - Refactor(Shaders): 디버그·오버레이·솔버 진입점과 helper 정리
  - Refactor(Naming): C++ 타입과 bool에 Unreal-Inspired 접두어 적용
  - Docs(Standards): Universal 5.5의 2단계 리팩터링 규칙 반영
  - Style(DebugUI): 클래스 선언을 책임별 14개 섹션으로 구획
  - Refactor(DebugUI): 프로파일링 누적 상태와 계산 책임 추출
  - Style(Code): 프로젝트 전반의 섹션과 탐색 구조 정리
  - Refactor(Shaders): 나머지 렌더·기하·스케줄링 진입점 정리
  - Refactor(DebugUI): UI 상태 묶음과 뷰 설정 패널 책임 분리
  - Refactor(Rendering): 벤치마크 상태·타이밍·기준 기하 책임 분리
  - Refactor(Simulation): 솔버 실행 상태와 step 준비 흐름 분리
  - Refactor(Tools): 분석 보고서와 빌드 스크립트의 책임 분리
  - Docs(AgentVault): 전수 적용 범위·검증 결과와 분할 커밋 기록 동기화
