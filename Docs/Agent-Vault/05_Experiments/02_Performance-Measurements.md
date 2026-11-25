# Performance Measurements — 측정 인덱스

> **성능 수치는 Scene·GPU·커밋·옵션·측정 구간 종속적**이다. 아래는 제공된 과거 문서의 탐색 인덱스이며, 이번 패키징에서 새 성능 측정을 실행하지 않았다. 상세 원자료는 저장소의 `Performance-Data/` 및 [[05_Experiments/Records/Performance-History-Through-2026-11-08|기존 전체 성능 기록]]을 따른다.

## 현재 Surface Simulation 최적화 검토

- 활성 영역 경로는 전체 활동 판정과 작업 목록 생성 비용이 있다. 동일 스텝 수·초기 상태로 마른 표면, 국소 주입, 넓은 피복에서 전체 디스패치와 비교해야 한다. 솔버 전체, 예약·후보·유출 제한·셀 반영 각각의 GPU 시간과 활성 작업 그룹 수를 기록한다.
- 국소 주입 준비의 전역 수위 재계산과 셀 반영의 반복 수위 복원은 줄일 수 있는 작업 후보다. 젖음·건조 경계와 자세 변화에서 질량·수위가 기준 실행과 같은지 함께 검증한다.
- 표시 스냅샷 복사 및 굴절 패스는 기존 타임스탬프로 측정할 수 있다. 배리어 범위와 배치 변경은 GPU 타임라인에서 실제 오버랩 또는 대기 비용이 확인될 때 비교한다.
- 아래 구형 GPU 솔버의 데이터와 연구 입력은 재현 기록이다. 구형 실행 옵션과 결합 연구 타깃은 현재 빌드에서 제거했다.

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

- Surface Simulation의 국소 분지 분할은 32×32 표본에서 64·65·128개 액체 저장 영역의 고정 스텝을 비교했다. [측정 조건과 원자료](04_Surface-Simulation-Validation.md#계산-셀-내부의-넘침과-gpu-시간)
- 에디터 `Profiling → Timings`는 매 프레임 표본을 수집하고 1초 간격으로 화면 값을 갱신한다. `Hold readings`을 켜면 표시값을 고정한다. 기존 메쉬 측정의 최근 100 ms 값도 갱신 시점의 값을 표시한다.
- 차트 GPU 시간은 스텝 묶음, 표시 스냅샷 복사, 뷰포트 전체 그리기 및 그 안의 굴절 prepass와 색상 패스를 측정한다. 워밍업 30프레임 후 최대 120개 완료 표본의 최근값·평균·P90을 표시한다. 계산과 복사는 스텝을 기록한 프레임에서만, 굴절 prepass는 해당 패스를 그린 프레임에서만 집계한다. 뷰포트 전체 그리기 시간에는 두 패스가 포함된다.
- CPU의 펜스 대기·이미지 획득·명령 기록·제출·표시는 각각 한 행이다. CPU 최신값과 1초 평균·최댓값을 표시한다. 차트 작업량은 스텝, 격자, 표본, 영역, 면, 뷰 수를 표시한다.

## 표면 수송 세분화

[격자별 수송 세분화 측정](05_Surface-Physics-Comparison.md#수송-횟수와-gpu-시간)을 참조한다.
