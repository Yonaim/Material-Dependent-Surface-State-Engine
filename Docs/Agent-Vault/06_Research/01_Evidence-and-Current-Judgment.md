# 근거와 현재 판정

2026-11-03 기준. 수치 원본은 [[05_Experiments/02_Performance-Measurements|PERF 기록]], 검증 범위는 [[05_Experiments/01_Validation-and-Tests|Validation]]을 따른다.

| 범위 | 확인된 사실 | 해석의 한계 |
|---|---|---|
| 구현 | 공통 State/Input·Transport·Decay, 2-pass flux, Profile v5 Reaction, Normal Map meso·cavity, 축적 형상과 후속 이동, sparse/cache 경로 | 새 보존 Solver·물리 부피 모델 또는 전 장면 정확도 보장은 아님 |
| PERF-041 | 판 장면에서 지속적 형상 갱신에 따른 물막 차이 2.95223%(공급량 기준), Flux·반응 장부 확인 | Bunny seam 누락·Mud 효과 제한. 일반적 우위는 아님 |
| PERF-042 | 모듈식 대조의 본 7쌍에서 저장 배열·Step 장부 일치, 선언 확장도 가능 | MDSSP의 제작 부담 우위 없음. 정식 비용 비교는 미실행 |
| PERF-043 | 표준 LS가 합성 높이 오차를 줄임 | 시험한 brick 장면의 수송 영향은 작아 일반적 이득 미입증 |
| 입력 감사·수식 | 실제 Heat Add 48개에서 두 사건 ±ε로 명목상 시간 합산 입력 보존 가능; 제한 모델 반례·상한 유도 | 최종 State 보호, 실제 최적화, 새 coarse 해법 미검증 |

첫 제작 후보는 목표 시점 전에 종료되어 **최종 목표 미평가**다. 사람의 제작 생산성 결과도 없다. Günther 등의 범용 aging·재질 규칙·재료량→형상→이동은 중요한 선례다. [[06_Prior-Model-Differentiation|선행모델 대조]]와 [[01_Related-Work#source-reference-07-related-work|문헌 장부]]를 참조한다.

**현재 분류는 ③ 시스템 통합.** 현 구현만의 확정 Contribution은 없고, 제안된 새 방법의 필요성도 아직 확인되지 않았다. [[01_Judgment-and-History#source-02-evidence-and-current-judgment|상세 원본]]에 실험 제한을 보존했다.
