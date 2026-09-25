# Week-02 — 시뮬레이션 방법 구체화

상태: **계획** · 상위 계획: [[02_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

## 목표

표면 상태·재질 반응·형상 데이터를 정의하고, State가 입력을 받아 다음 상태로 갱신되는 규칙을 정리한다.

## 작업

- SRProfile에서 구성하는 State Registry와 parameter, Shared Geometry 및 Instance State data의 계약을 정의한다.
- `stateCapacity`, `Saturation`, `TempState`의 역할을 구분한다.
- Input / Transport / Decay의 식과 discrete/continuous 시간 처리를 정리한다.
- SaturationDrive, GeometryDrive, TransferWeight와 alpha 제한을 설계한다.
- State transition과 `State × accumulationFactor` 기반 Accumulation을 정의한다.
- 2-Pass Solver 흐름과 필요한 임시 데이터·동기화 개념을 검토한다.

## 산출물

- Surface State 및 SRProfile 데이터 정의.
- State update 흐름도와 계산식 초안.
- Geometry와 Accumulation 규칙의 설계 문서.

## 참고

- [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]
- [[03_Architecture/0004_Surface-State-Update|Surface State 입력과 갱신]]
- [[03_Architecture/0005_Surface-Geometry|형상 정보와 적층]]
- [[07_Assets/Documents/0002_Surface-System-Data.pdf|시스템 데이터 구조 원본]]
- [[07_Assets/Documents/0005_Next-State-Calculation.pdf|Next State 계산 원본]]
