# Week-06 — Wetness 구현

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

재질 내부에 흡수된 수분인 Wetness의 입력·전파·감쇠와 렌더링 반응을 구현한다.

## 작업

- Wetness 입력을 만들고 재질별 `inputFactor`, transfer rate, decay를 조정한다.
- 표면 방향과 중력·형상 조건을 반영하는 흐름을 검증한다.
- 색과 roughness 등 Wetness의 외관 반응을 Shader에 연결한다.
- 재질별 결과를 같은 입력 조건에서 비교한다.

## 범위 경계

`Wetness`는 내부 흡수 수분이다. 표면 위에서 흐르거나 고이는 물은 `SurfaceWater`이며, 기본 네 채널과 같은 의미로 구현하지 않는다. 자유 표면수 동작이 이 주차 목표에 필요하면 별도 channel/layer 범위를 먼저 결정한다.

## 산출물

- Wetness demo와 재질별 비교 이미지.
- Input, Transport, Decay와 시각 표현을 확인할 수 있는 Debug View.

## 참고

- [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]
- [[03_Architecture/0006_Rendering|Rendering]]
- [[03_Architecture/0007_Demos|목표 데모]]
