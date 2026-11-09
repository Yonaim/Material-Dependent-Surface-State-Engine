# Universal 5.5 — 큰 클래스·파일의 단계적 구조화

2026-11-09 사용자 우선 요청. [공통 지침](../00_Engineering-Standards/00_Core/03_Section-and-Responsibility-Refactoring.md)을 먼저 프로젝트에 반영한다. `93dbbed`를 반영하며 공통 예시의 T 접두어는 프로젝트 F/E/b 규칙을 대체하지 않는다.

## 우선 적용: FDebugUI — 첫 단위 완료

1. **섹션 정리:** DebugUI.h의 public API → 기능별 내부 메서드 → 관련 상태를 번호 섹션과 Region으로 표시한다. 로직·토큰·멤버 선언 순서·접근 지정자·전처리 조건을 보존하고 별도 스냅샷/검증/Work Log를 남긴다.
2. **책임 추출:** profiling의 누적 시간·frame 수·metric sum/count·평균/최대·최근 sample deque·가용 상태 및 Reset/Accumulate/Publish 동작을 FProfilingUIState로 추출한다. owner는 FDebugUI의 값 멤버로 유지하고 새 포인터/할당/독립 수명을 추가하지 않는다. 렌더러 통계 수집·ImGui 화면 출력은 UI에 남긴다.

첫 적용 전 Profiling은 private 연속 필드였고 참조는 DebugUIProfiling.cpp에 모여 있었다. 생성 기본값과 Reset은 서로 다른 기존 sentinel 정책(평균 초기 0, Reset 후 -1)을 그대로 보존한다. scene 교체·solver 옵션 변경·performance 옵션 변경의 기존 Reset 호출을 유지한다. FDebugUI는 GPU/직렬화 record가 아니며 private layout은 변경될 수 있으므로 소비자를 함께 빌드한다. 외부 API·settings JSON·Scene 형식·CPU–GPU ABI는 유지한다.

첫 적용 결과: 1단계 14개 구획/코드 토큰 동일/MDSS target build 성공. 2단계 상태 13개와 동작 3개 추출, 2,000프레임·4,023회 전후 비트 일치, 전체 build 성공, CTest 상태/실패 문구와 render-only 출력 동일. [원자료](../../../Build/Validation/20261109-224136-vault55-debugui/summary.md).

## 검증

- 1단계: 주석/Region 제외 토큰 동일, Region 번호·균형, 실제 Clang parser와 변경 영역 build.
- 2단계: 원래 reset/update 수식의 정규화 대조, 초기화/Reset/시간 창/유효하지 않은 값/최근 100ms sample 만료/평균 게시를 원래 코드와 비교하는 artifact probe, 전체 build/CTest 및 render-only 전후 실행 비교.
- 현재 기준선 CTest 7 통과/7 실패·GPU storage-buffer 한도는 별도 회귀 없이 임의 변경하지 않는다. UI 모든 조작/장시간·성능은 실행 범위와 구분해 기록한다.

## 프로젝트 전체 적용 — 사용자 범위 확정

2026-11-09 후속 지시로 적용 범위를 FDebugUI 첫 단위에서 프로젝트 전체로 확대했다. Source/Shaders/Scripts/Tests 및 루트 실행 스크립트를 전수 목록화하고 파일별 1단계/2단계 적용 또는 유지 근거를 최종 coverage 목록에 남긴다. 크기 150줄은 검토 후보를 찾는 보조 신호이며 분리 강제 기준이 아니다. 외부 ThirdParty/생성 Build/중첩 Universal 볼트/자산 데이터는 구현 대상에서 제외한다. 현재 JS/TS·C 소스가 없다면 부재를 기록한다.

작업 순서: (1) 선언/실행 순서 보존 구획 정리·별도 검증 → (2) DebugUI 상태, Renderer benchmark, Solver 실행 설정, shared geometry helper 및 긴 Python 분석 함수의 응집된 책임 추출 → (3) 전체 build/CTest/shader/스크립트 구문과 비교 검증. 테스트 fixture와 GPU ABI record도 검토하되 고정 필드/바인딩·실행/실패 정책을 편의상 바꾸지 않는다. Shell/PowerShell의 짧은 진입점은 함수 추출이 유용한 경우에만 분리한다.

## 프로젝트 전체 검토 결과

구현 코드 323개(C++ 201, GLSL 69, Python 44, Bash 5, PowerShell 4) 및 CMake 설정/manifest 2개를 검토했다. 파일별 [대조표](12_Section-and-Responsibility-Coverage.md)에 1단계/2단계 적용 또는 유지 판단을 기록했다. 1단계의 코드/AST 순서를 보존한 뒤 별도 2단계 변경으로 UI·Renderer·Solver의 97개 필드를 8개 값 소유 상태에 추출했고, frame timing/solver preparation·view popup·world geometry·Python 처리 7개·Shell preflight/cache 단계를 분리했다. 나머지는 이미 단일 책임 또는 기존 함수/모듈 경계·GPU ABI/수명 단위여서 유지했다.

최종 build 성공, CTest 7 통과/7 실패·기준선 동일, shader 150개 컴파일/검증·reflection 동일, render-only/geometry/Python/Shell 전후 비교 동일. [원자료](../../../Build/Validation/20261109-230833-vault55-project-wide/summary.md). PowerShell 실행·전체 UI 조작·장시간/성능·지원하지 않는 GPU 경로는 미검증이며 유지 판단을 실행 성공으로 대체하지 않는다.

## 대상별 책임 경계 — 검토 완료

| 우선 | 범위 | 판단할 책임 경계 |
|---|---|---|
| 완료 1 | FDebugUI 나머지 상태 | Presentation/Simulation/ProfileEditing/Gizmo/Viewport/Log 상태 추출. 작은 camera/dock·binding은 조율자에 유지 |
| 완료 2 | Renderer.h / RendererCommands.cpp | benchmark 상태·query 회수·world geometry 분리. 나머지 GPU 자원·기존 frame/view helper의 공유 수명 유지 |
| 완료 3 | Overlay·Solver의 큰 shader/include | 6개 entry 추가 정리. 전 69개에서 기존 단일 pass/include·고정 ABI/derivative/barrier 경계 유지 |
| 완료 4 | 큰 Python/Shell 연구·실행 모듈 | Python 처리 7개·Bash preflight/cache 분리. import/global/exit 보존, PowerShell은 코드 동일 구획만 추가 |

이 목록은 고정 추출 타입 목록이 아니다. 전체 적용은 모든 파일에 새 타입을 만드는 뜻이 아니라 파일별로 경계를 검토하고 필요한 변경을 완료했다는 의미다. 작은 단일 책임 파일에는 두 단계를 강제하지 않는다. 기존 상위 함수 추출/네이밍 완료 기록은 그대로 보존하며 이번 첫 적용으로 저장소 전체 책임 분리가 완료됐다고 보지 않는다. 커밋은 별도 요청 없이 만들지 않는다.
