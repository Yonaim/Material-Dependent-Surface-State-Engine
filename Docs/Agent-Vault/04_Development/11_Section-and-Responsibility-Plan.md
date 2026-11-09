# Universal 5.5 — 큰 클래스·파일의 단계적 구조화

2026-11-09 사용자 우선 요청. [공통 지침](../00_Engineering-Standards/00_Core/03_Section-and-Responsibility-Refactoring.md)을 먼저 프로젝트에 반영한다. `93dbbed`를 반영하며 공통 예시의 T 접두어는 프로젝트 F/E/b 규칙을 대체하지 않는다.

## 우선 적용: FDebugUI — 첫 단위 완료

1. **섹션 정리:** DebugUI.h의 public API → 기능별 내부 메서드 → 관련 상태를 번호 섹션과 Region으로 표시한다. 로직·토큰·멤버 선언 순서·접근 지정자·전처리 조건을 보존하고 별도 스냅샷/검증/Work Log를 남긴다.
2. **책임 추출:** profiling의 누적 시간·frame 수·metric sum/count·평균/최대·최근 sample deque·가용 상태 및 Reset/Accumulate/Publish 동작을 FProfilingUIState로 추출한다. owner는 FDebugUI의 값 멤버로 유지하고 새 포인터/할당/독립 수명을 추가하지 않는다. 렌더러 통계 수집·ImGui 화면 출력은 UI에 남긴다.

Profiling은 현재 private 연속 필드이며 참조는 DebugUIProfiling.cpp에 모인다. 생성 기본값과 Reset은 서로 다른 기존 sentinel 정책(평균 초기 0, Reset 후 -1)을 그대로 보존한다. scene 교체·solver 옵션 변경·performance 옵션 변경의 기존 Reset 호출을 유지한다. FDebugUI는 GPU/직렬화 record가 아니며 private layout은 변경될 수 있으므로 소비자를 함께 빌드한다. 외부 API·settings JSON·Scene 형식·CPU–GPU ABI는 유지한다.

첫 적용 결과: 1단계 14개 구획/코드 토큰 동일/MDSS target build 성공. 2단계 상태 13개와 동작 3개 추출, 2,000프레임·4,023회 전후 비트 일치, 전체 build 성공, CTest 상태/실패 문구와 render-only 출력 동일. [원자료](../../../Build/Validation/20261109-224136-vault55-debugui/summary.md).

## 검증

- 1단계: 주석/Region 제외 토큰 동일, Region 번호·균형, 실제 Clang parser와 변경 영역 build.
- 2단계: 원래 reset/update 수식의 정규화 대조, 초기화/Reset/시간 창/유효하지 않은 값/최근 100ms sample 만료/평균 게시를 원래 코드와 비교하는 artifact probe, 전체 build/CTest 및 render-only 전후 실행 비교.
- 현재 기준선 CTest 7 통과/7 실패·GPU storage-buffer 한도는 별도 회귀 없이 임의 변경하지 않는다. UI 모든 조작/장시간·성능은 실행 범위와 구분해 기록한다.

## 후속 검토 후보

| 우선 | 범위 | 판단할 책임 경계 |
|---|---|---|
| 후속 1 | FDebugUI 나머지 상태 | viewport/선택·gizmo·profile draft·simulation UI의 공유 필드/리셋/수명부터 대조. 일괄 추출하지 않음 |
| 후속 2 | Renderer.h / RendererCommands.cpp | 자원 소유/viewport command orchestration/계측 경계. 기존 추출 helper를 전체 God Object 해소로 보지 않음 |
| 후속 3 | Overlay·Solver의 큰 shader/include | 기존 Resources/main/helpers 구획과 CPU–GPU/derivative/barrier 경계 확인 후 추가 추출 유용성 판단 |
| 후속 4 | 큰 Python/Shell 연구·실행 모듈 | 진입점·단계/상태 경계·외부 파일·종료 코드/실행 조건 확인. 기존 6번 스타일 정리와 연결 |

이 목록은 완료 선언이나 고정 추출 타입 목록이 아니다. 작은 단일 책임 파일에는 두 단계를 강제하지 않는다. 기존 상위 함수 추출/네이밍 완료 기록은 그대로 보존하며 이번 첫 적용으로 저장소 전체 책임 분리가 완료됐다고 보지 않는다. 커밋은 별도 요청 없이 만들지 않는다.
