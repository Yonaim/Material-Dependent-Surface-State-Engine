# Week-09 — Heat 구현

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

Heat 입력·전파·Cooling을 구현하고 재질별 열 반응을 확인한다.

## 작업

- 입력 이벤트로 Heat State를 증가시킨다.
- Solver의 이웃 전파와 `DeltaTime` 기반 Decay를 적용한다.
- 재질별 Heat 입력·전달·감쇠 parameter를 조정한다.
- Heat field와 Cooling을 Debug View 및 화면 표현으로 확인한다.

## 산출물

- Heat 전파 demo.
- 재질별 parameter 기록과 Cooling 확인 결과.

## 참고

- [[03_Architecture/0004_Surface-State-Update|Surface State Update]]
- [[03_Architecture/0006_Rendering|Rendering]]
