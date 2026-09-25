# Project Plan

상태: **프로젝트 범위 및 목표 기준** · 일정 요약은 [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]], 주차별 목표는 [[02_Planning/01_Weekly-Overview/Week-01|Weekly Overview]], 세부 구현 계획은 [[02_Planning/02_Weekly-Details/Week-04/0000_Week4-Branch-Plan|Weekly Implementation Details]]에서 관리한다.

## 프로젝트 목표

C++20 / Vulkan 기반 렌더링 엔진에 **Material-Dependent Surface State** 시뮬레이션을 추가한다. Mesh 표면을 Simulation texel graph로 표현하고, Surface별 Profile과 Instance별 상태를 이용해 입력·전파·감쇠를 계산한다. 상태 변화는 렌더링 표현과 필요한 경우의 형상 적층으로 연결한다.

초기 구현은 Static Mesh를 대상으로 한다. 프로젝트가 지향하는 최종 데모는 [[03_Architecture/0007_Demos|목표 데모]]에 정리한다.

## 범위와 결과

| 영역 | 계획 범위 | 기대 결과 |
|---|---|---|
| 엔진 기반 | Vulkan 초기화, 렌더러, Scene 및 Asset 로딩 | Static Mesh와 Material을 표시하는 실행 가능한 기반 |
| 표면 데이터 | Simulation Mapping, Shared Geometry, Instance State, SRProfile 연결 | Solver에서 사용할 수 있는 texel 단위 데이터 |
| 상태 갱신 | discrete Input, `DeltaTime` 기반 Transport / Decay, Capacity 제한 | 재현 가능한 기본 Surface State Solver |
| 형상 반영 | Macro / Meso 형상과 Accumulation Height | 형상에 반응하는 이동과 적층 표현 |
| 렌더링 | 상태별 외관 변화와 적층 결과 표시 | Wetness / Mud / Heat / Burn 데모 및 평가 |
| 평가·보고 | 성능 측정, 실패 사례 분석, 저장소·보고서·발표 정리 | 수치와 재현 절차를 포함한 최종 결과물 |

## 현재 설계 기준

- 기본 상태 채널은 `Wetness`, `Heat`, `Burn`, `Mud` 네 가지다. `SurfaceWater`와 `Snow`는 별도 범위 결정 전까지 기본 채널에 포함하지 않는다.
- `Wetness`는 재질 내부에 흡수된 수분이고, 표면 위에서 흐르거나 고이는 물은 `SurfaceWater`다.
- `State`는 상태별 `stateCapacity` 범위 안에 두고 `Saturation = State / stateCapacity`를 사용한다. 별도 `Overflow` 저장은 하지 않는다.
- Input은 discrete event, Transport와 Decay는 시간 기반 연속 갱신으로 처리한다.
- Accumulation은 `State × accumulationFactor`에서 계산하고 Cavity Fill / Excess를 구분한다.
- State 갱신의 GPU 기본안은 2-Pass `alpha` solver와 State A/B ping-pong이다. `InputDelta`는 dense buffer를 사용한다.

세부 의미와 식은 [[03_Architecture/0002_Surface-State|Surface State]], [[03_Architecture/0004_Surface-State-Update|State Update]], [[03_Architecture/0005_Surface-Geometry|Surface Geometry]]를 기준으로 한다.

## 범위 경계

- 4주차에는 자동 UV unwrap을 구현하지 않는다. 미리 준비한 UV를 검증한 뒤 Mapping에 사용한다.
- 완전한 Normal Map 적분, 비적분 가능 입력의 최종 보정, 동적 Accumulation geometry의 최종 저장 구조는 현재 기본 구현의 선행 조건이 아니다. 검증·일정 결과를 바탕으로 후속 범위를 정한다.
- `SurfaceWater`와 `Snow`가 필요한 목표 데모는 기본 네 채널과 구분한다. 해당 기능의 구현 시점과 별도 layer/channel 여부는 별도 결정으로 다룬다.

## 일정과 진척 관리

- [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]은 16주 전체의 순서와 주요 milestone을 요약한다.
- `01_Weekly-Overview/Week-XX.md`는 전체 주차의 목표와 주요 산출물을 요약한다.
- `02_Weekly-Details/Week-XX/`에는 해당 주의 상세 작업 순서와 구현 계획을 둔다.
- [[TODO|TODO]]는 실제 미완료 작업과 완료 여부를 추적한다. 계획 문서의 목표를 완료로 간주하지 않는다.
- 일정·범위가 변경되면 Roadmap과 영향을 받는 주차 문서를 함께 갱신한다.

## 최종 산출물

- 실행 가능한 프로젝트 Repository와 빌드·실행 안내.
- 목표 기능을 보여주는 통합 Demo 영상.
- 설계, 구현, 성능 실험, 실패 사례 및 한계를 설명하는 최종 보고서.
- 발표 자료와 결과 시각화.
