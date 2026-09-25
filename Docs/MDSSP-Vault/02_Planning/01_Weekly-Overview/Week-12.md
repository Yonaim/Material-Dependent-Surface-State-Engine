# Week-12 — 기본 상태 통합

상태: **계획** · 상위 계획: [[02_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

## 목표

기본 데모에 필요한 State들을 하나의 Registry 기반 Surface State System과 공통 update path에서 처리한다.

## 작업

- `Wetness`, `Mud`, `Heat`, `Burn`의 입력 및 갱신 경로를 통합한다.
- 공통 Solver 항과 상태별 SRProfile 설정을 구분한다.
- Heat → Burn transition과 Mud Accumulation을 통합 실행에서 검증한다.
- 목표 demo에 필요한 State들을 Profile에 선언하고 통합 실행을 검증한다. `SurfaceWater`/`Snow`는 이 주차의 필수 demo 범위가 아니며, Registry의 고정 채널 제한으로 배제되는 것은 아니다.
- 각 상태의 렌더링과 Debug View를 통합한다.

## 산출물

- 상태 통합 demo.
- 공통 처리 구조와 상태별 차이 정리.
- 통합 시나리오 test 결과.

## 참고

- [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]
- [[03_Architecture/0004_Surface-State-Update|Surface State Update]]
- [[../00_Project-Overview/0002_Demo|목표 데모]]
