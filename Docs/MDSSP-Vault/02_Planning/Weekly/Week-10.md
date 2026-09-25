# Week-10 — Heat → Burn Transition

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

Heat Saturation 조건을 만족할 때 Burn이 증가하고, Heat 냉각 뒤에도 Burn 흔적이 남도록 한다.

## 작업

- `source`, `target`, `threshold`, `transitionRate`를 사용하는 Heat → Burn 규칙을 적용한다.
- 재질별 임계값과 전이 속도를 조정한다.
- Heat의 Cooling과 Burn의 잔류를 분리해 검증한다.
- Burn State를 그을림·탄 흔적 Shader 표현에 연결한다.

## 산출물

- Heat/Burn transition demo.
- 임계값·전이 속도별 결과와 parameter 기록.

## 참고

- [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]
- [[03_Architecture/0006_Rendering|Rendering]]
- [[03_Architecture/0007_Demos|목표 데모]]
