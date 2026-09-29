# Week-11 — 형상 반영 개선

> **한 줄 요약:** 표면의 높이·방향·곡률이 이동과 잔류에 미치는 영향을 조정하고 경계 사례를 개선한다.

상태: **계획** · 상위 계획: [[03_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[03_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

## 목표

표면의 높이·방향·곡률이 이동과 잔류에 미치는 영향을 조정하고 경계 사례를 개선한다.

## 작업

- HeightDrive와 DirectionDrive를 개별 확인해 중력·표면 방향에 따른 이동을 조정한다.
- NormalWeight, CurvatureWeight와 ConcavityWeight의 역할을 구분해 검증한다.
- 4주차 mapping의 seam / neighbor 연결 실패 사례를 보완한다.
- 서로 다른 Surface 또는 SRProfile 경계에서 전파를 확인한다.
- Week-05에 반영한 Normal Map `NormalWeight` 및 Virtual Meso Geometry를 평가하고, 관찰된 형상·전달 한계에 대한 보정 범위를 결정한다.

## 산출물

- 형상 반영 전·후 비교 이미지.
- 보정 parameter 또는 수식 기록.
- seam 및 Surface/Profile 경계 테스트 결과.

## 참고

- [[04_Architecture/0006_Surface-State-Update|Surface State Update]]
- [[04_Architecture/0004_Surface-Geometry|형상 정보와 적층]]
- [[../../06_Development/Notes/Geometry-Preprocessing|Geometry Preprocessing]]
- [[06_Development/Experiments/0001_Normal-Map-Integration|Normal Map Integration 실험]]
