# 에이전트 문서 정책

이 볼트의 정본은 `02_Architecture`의 **현재 계산 계약**이다. 코드와 불일치하면 확인 후 수정한다. 설계 이유는 `03_Development/02_Current-Choices.md`에 짧게 남기고, 오래된 선택·대안·개인 학습 과정은 개인 볼트에 둔다.

| 위치 | 내용 |
|---|---|
| `01_Project-Policy` | 공식 용어, 코드·주석·커밋 규칙, 필요한 작업 템플릿 |
| `02_Architecture` | Flows의 실행 순서, Contracts의 계산 정의, Source의 C++·셰이더 지도 |
| `03_Development` | 미완료 작업, 현행 선택 이유, 검증·테스트, 성능 측정 규칙과 결과 |

실험 수치는 화면에서 전사하지 않고 자동 출력 원자료로 기록한다. 성능 실험은 [[04_Experiments/02_Performance-Measurements|측정 규칙]]과 [[01_Project-Policy/Templates/03_Experiment|실험 템플릿]]을 따른다. 원자료와 실행별 manifest는 저장소의 `Performance-Data/`에 두고, 수행하지 않은 측정 결과는 만들지 않는다.

한 개념의 현재 정의는 한 곳에만 둔다. Architecture에는 작업 일지·개인적 해석·과거 대안을 넣지 않는다. 이력의 결정 번호를 인용할 때는 현재 상태를 확인하고, 필요한 이유만 Current Choices에 반영한다. [[03_Development/00_Overview|Development 역할]]을 따른다.
