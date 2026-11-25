# Architecture Decision Records (ADR)

> 설계 선택의 이유·대안·트레이드오프·재검토 조건을 보존한다. 현재 코드의 세부 구현은 Contracts·Flows·Source에 둔다.

## 상태와 ID

- `Proposed`: 제안/논의 중, `Accepted`: 채택됨, `Superseded`: 다른 ADR로 대체, `Rejected`: 채택하지 않음.
- 새 ADR은 `ADR-0001`처럼 순차 ID를 사용한다. ID는 파일의 탐색용 번호와 별개다.
- 채택된 결정의 현재 계약·흐름은 Architecture에도 반영하고, 선택 이유와 대안은 ADR에 둔다.

## 현재 결정 요약

현재 채택된 방향과 변경 제약은 [[03_Decisions/01_Current-Decisions|Current Decisions]]에 통합되어 있다. 새로 승인되는 독립 설계 결정부터 ADR 기록으로 분리한다.

기존 요약의 과거 결정 ID는 역사적 참조다. 새 ADR은 아래 개별 문서에서 추적한다.

새 ADR은 [`ADR Template`](../00_Engineering-Standards/04_Docs_Templates/05_Docs_Template-ADR.md)을 복사해 이 폴더의 `00_Records/ADR-0001-...md`처럼 저장하고 이 표에 등록한다.

- [ADR-0001 — 표면 물의 경계와 출구 표시](00_Records/ADR-0001-Surface-Boundaries.md)
- [표면 반응에서 수증기와 불을 표시하는 방식](06_Surface-Emissions.md)

- [ADR-0002 — 표면 접촉, 퇴적 형상과 수송 시간 간격](00_Records/ADR-0002-Surface-Contact-and-Coupling.md)
- [ADR-0003 — 씬에서 Height Map을 직접 지정](00_Records/ADR-0003-Scene-Height-Maps.md)
