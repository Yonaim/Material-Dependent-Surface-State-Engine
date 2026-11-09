# 실험 문서의 역할

이 폴더는 구현이 끝난 변경이나 검증 가능한 가설을 **테스트·검증·측정한 조건과 증거**를 보관한다. 미완료 구현 과제는 [[04_Development/00_Current-Work|Current Work]], 현행 동작 계약은 [[02_Architecture/03_System-Map|Architecture]]에 둔다.

| 문서 | 기록 내용 |
|---|---|
| [[05_Experiments/01_Validation-and-Tests|Validation and Tests]] | 테스트 목적·fixture·실행 결과, 수치 검증과 미실시 범위 |
| [[05_Experiments/02_Performance-Measurements|Performance Measurements]] | 성능 측정 규칙, 전후 비교, 정확성 확인, 해석과 채택 여부 |

성능 원자료·manifest·집계 파일은 볼트 바깥의 `Performance-Data/`에 둔다. 전체 자료를 일반 작업 문맥에 불러오지 않는다. 필요한 실험을 재검증할 때 [[05_Experiments/02_Performance-Measurements|측정 기록]]에서 해당 실행을 찾아 원자료 색인을 연다.

실험 기록에는 조건, 측정 범위, 반복 횟수, 실제 결과, 정확성 확인, 한계와 판단을 구분해 적는다. 미실행·실패·미지원은 각각 그대로 표시한다. 문서나 설정만 정리한 변경에는 성능 결과를 만들지 않는다.
