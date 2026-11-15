# Requirements — MDSS Engine

> 요구사항을 정의한다. 구체적인 완료 판정과 검증 근거는 Validation 문서에서 관리한다.

| ID | 기능 / 품질 요구사항 | 근거 | 검증 상태 |
|---|---|---|---|
| FR-001 | Scene 및 Asset/Profile을 읽어 Surface/Geometry 데이터를 준비한다 | [[02_Architecture/00_Flows/00_Initialization/00_Scene-Load|Scene Load]] | 검증 기록 별도 관리 |
| FR-002 | Surface State를 GPU에서 단계적으로 갱신하고 상태 입력을 1회 소비한다 | [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Simulation]] | 검증 기록 별도 관리 |
| FR-003 | Simulation State를 Geometry 및 렌더링에 반영한다 | [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Rendering]] | 검증 기록 별도 관리 |
| FR-004 | 지원하는 Reaction/State 전이 계약을 처리한다 | [[02_Architecture/01_Contracts/06_Reactions|Reactions]] | 검증 기록 별도 관리 |
| NFR-001 | GPU/CPU 인터페이스의 데이터 레이아웃 및 Binding을 일치시킨다 | [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]] | 검증 기록 별도 관리 |

> 구체적 성능 목표와 완성 조건은 근거를 확보한 뒤 추가한다. 기존 측정 기록은 [[05_Experiments/02_Performance-Measurements|성능 측정]]에서 확인한다.
