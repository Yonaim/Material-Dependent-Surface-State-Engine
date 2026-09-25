# Roadmap

상태: **16주 전체 일정 기준** · 프로젝트 목적과 범위는 [[02_Planning/0000_Project-Plan|Project Plan]], 실행 항목은 각 주차 문서를 본다.

| 주차 | 주제 | 주요 결과 |
|---:|---|---|
| [[02_Planning/Weekly/Week-01|1주차]] | 전체 구조 설계 | 엔진 모듈 및 Surface State System 범위 정의 |
| [[02_Planning/Weekly/Week-02|2주차]] | 시뮬레이션 설계 | 데이터 구조, 갱신식, Solver 흐름과 적층 규칙 정리 |
| [[02_Planning/Weekly/Week-03|3주차]] | 렌더링 기반 | Static Mesh, Material, Asset을 표시하는 Vulkan 엔진 기반 |
| [[02_Planning/Weekly/Week-04|4주차]] | Surface 데이터 구현 | UV mapping부터 Geometry·GPU·입력까지 최소 검증 경로 |
| [[02_Planning/Weekly/Week-05|5주차]] | 기본 Solver | Transport / Decay / Capacity 동작과 테스트 검증 |
| [[02_Planning/Weekly/Week-06|6주차]] | Wetness | 수분 상태의 재질별 반응과 외관 표현 |
| [[02_Planning/Weekly/Week-07|7주차]] | Mud 및 중간 Demo | Mud 적층을 Wetness와 통합한 중간 결과 |
| [[02_Planning/Weekly/Week-08|8주차]] | 중간 결과 검토 | 품질·성능·문제 분석 및 후반 범위 조정 |
| [[02_Planning/Weekly/Week-09|9주차]] | Heat | Heat 입력, 전파, cooling과 재질별 반응 |
| [[02_Planning/Weekly/Week-10|10주차]] | Heat → Burn | 임계값 기반 상태 전이와 잔류 Burn 표현 |
| [[02_Planning/Weekly/Week-11|11주차]] | 형상 반영 개선 | Height·방향·곡률·경계에 따른 계산 개선 |
| [[02_Planning/Weekly/Week-12|12주차]] | 상태 통합 | 네 기본 상태를 하나의 System / Solver로 통합 |
| [[02_Planning/Weekly/Week-13|13주차]] | GPU 최적화 | 측정된 병목과 GPU pass·memory 접근 개선 |
| [[02_Planning/Weekly/Week-14|14주차]] | 성능 실험 | 해상도·Instance·갱신 조건별 FPS·GPU·메모리 측정 |
| [[02_Planning/Weekly/Week-15|15주차]] | 비교와 실패 분석 | 품질 비교, 실패 조건 및 한계 정리 |
| [[02_Planning/Weekly/Week-16|16주차]] | 최종 결과 | Repository, 보고서, Demo 영상, 발표 자료 완성 |

## 주요 Milestone

1. **4주차 — 최소 수직 경로**: 준비한 UV가 있는 테스트 Mesh에서 Mapping → Geometry → GPU State → Solver → Contact Input을 확인한다. 자동 unwrap은 범위에서 제외한다.
2. **7주차 — 중간 Demo**: Wetness와 Mud의 상태 갱신·외관·적층을 통합해 시연한다.
3. **8주차 — 중간 검토**: 결과와 위험 요소를 바탕으로 후반 구현 범위를 재조정한다.
4. **12주차 — 상태 통합**: `Wetness`, `Mud`, `Heat`, `Burn`을 공통 Surface State System에서 처리한다.
5. **13–15주차 — 최적화·평가**: 성능을 측정하고 비교 실험과 실패 사례를 문서화한다.
6. **16주차 — 제출 결과**: 실행 가능한 저장소, 최종 보고서, Demo 영상과 발표 자료를 정리한다.

주차별 세부 계획은 `Weekly/`에 둔다. 일정 변경은 변경된 주차 문서와 이 Roadmap을 함께 수정한다.
