# MDSSP 에이전트 볼트

이 볼트는 **현재 구현을 읽고 변경·검증하기 위한 작업 문서**다. Decision 원문·회의 PDF·개인 학습 기록은 별도 개인 볼트의 역사적 자료이며 현행 계약으로 해석하지 않는다.

## 읽는 순서와 기준

1. [[02_Architecture/00_Overview|Architecture]]에서 실행 흐름, 계산 계약, C++·셰이더 위치를 찾는다.
2. [[03_Development/00_Overview|Development]]에서 진행 중인 작업과 검증 기록의 역할을 확인한다.
3. [[03_Development/02_Current-Choices|Current Choices]]와 [[03_Development/01_Current-Work|Current Work]]에서 현재 판단과 미완료 범위를 확인한다.
4. 검증에는 [[04_Experiments/01_Validation-and-Tests|Validation and Tests]]를 사용한다. 성능 비교 전에는 [[04_Experiments/02_Performance-Measurements|Performance Measurements]]의 측정 규칙을 읽는다.

코드·테스트가 현재 실제 동작의 근거다. Architecture와 충돌하면 차이를 확인해 문서를 갱신한다. 결정 번호와 오래된 PDF 제목은 역사적 출처 표시일 뿐 현재 지시가 아니다.
