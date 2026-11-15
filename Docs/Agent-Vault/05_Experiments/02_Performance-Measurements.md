# Performance Measurements — 측정 인덱스

> **성능 수치는 Scene·GPU·커밋·옵션·측정 구간 종속적**이다. 아래는 제공된 과거 문서의 탐색 인덱스이며, 이번 패키징에서 새 성능 측정을 실행하지 않았다. 상세 원자료는 저장소의 `Performance-Data/` 및 [[05_Experiments/Records/Performance-History-Through-2026-11-08|기존 전체 성능 기록]]을 따른다.

## 최근 비교 실험

### PERF-044 — Reaction Transition 비교 씬의 윗면만 시뮬레이션
- 2026-11-08 원본 기록. 비교 Scene의 시뮬레이션·렌더링 범위 축소 전후를 동일 입력으로 측정한 사례.
- 조건·샘플 수·실제 결과: [[05_Experiments/Records/Performance-History-Through-2026-11-08#PERF-044 — Reaction Transition 비교 씬의 윗면만 시뮬레이션|PERF-044 전체 기록]]. 변경 효과를 다른 Scene에 일반화하지 않는다.

### PERF-045 — 비교 씬 편집 화면의 Sparse Solver 기본값
- 2026-11-08 원본 기록. *이전 Ice/Lava 비교 씬*에서 `sparse_solver` ON/OFF 비교. 해당 설정은 Scene 특화, 전역 기본값과 구분.
- 조건·숫자: [[05_Experiments/Records/Performance-History-Through-2026-11-08#PERF-045 — 비교 씬 편집 화면의 Sparse Solver 기본값|PERF-045 전체 기록]]. 이후 Fuel/Burn 비교 씬의 성능 근거가 아니다.

## 다른 실험의 탐색

- [[05_Experiments/Records/Performance-History-Through-2026-11-08#PERF-001 — 현재 버전 기준 측정 (2026-10-22)|PERF-001 기준 측정]]
- [[05_Experiments/Records/Performance-History-Through-2026-11-08#8개 묶음 최적화 비교 — PERF-002~035|PERF-002~035 최적화 묶음]]
- [[05_Experiments/Records/Performance-History-Through-2026-11-08#최적화 후속 구현·전후 측정 — 2026-10-25|PERF-036~040 및 기타 후속 구현]]
- PERF-041~043의 연구적 해석은 **개인 볼트의 `06_Experiments/Case-Studies/`**에서 관리한다. 이 볼트 안에서는 코드를 검증할 때 관련 실제 로그를 확인한다.

## 측정 규약

[[05_Experiments/Records/Performance-History-Through-2026-11-08#성능 측정 규칙|기존 성능 측정 규칙]]을 기본으로 하되, 실제 `Performance-Data`의 manifest와 측정 스크립트를 우선 확인한다.

## 신규 기록 계약

`PERF ID / 질문 / 변경 하나 / HEAD+작업트리 / 하드웨어·빌드 / 입력 SHA / 초기 State / 반복 / raw logs / 집계 / 정확성 비교 / 적용·불적용 범위`
