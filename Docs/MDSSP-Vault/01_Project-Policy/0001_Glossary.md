# 용어집

> **한 줄 요약:** MDSS Engine에서 사용하는 주요 용어의 현재 의미다.

MDSS Engine에서 사용하는 주요 용어의 현재 의미다.

본문에서는 코드·설계 개념에 대응하는 영문 이름을 그대로 사용한다. 예를 들어 `GeometryDrive`를 ‘형상 구동’, `TransferWeight`를 ‘전달 가중치’로 바꾸지 않으며, 한국어 설명은 각 용어의 의미를 풀어 쓰는 데 사용한다.

State / Capacity / Saturation은 [[05_Decisions/0004_State-Overcapacity-Transport|Decision 0004]]의 새 계약을 따른다. Shader의 상한 clamp 제거와 선택 GPU 회귀 fixture는 통과했으며, 5주차 통합 검증과 timestep 비교는 대기 중이다.

| 용어 | 뜻 | 관련 문서 |
|---|---|---|
| **Surface State** | 표면 텍셀에 저장되는 시간에 따른 상태량. State 종류는 `.SRProfile`에서 수집한 Registry가 결정하며 `Wetness`, `Heat`, `Burn`, `Mud`는 기본 demo 예시다. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **Surface Response Profile / SRProfile** | Surface가 각 State에 어떻게 반응하는지 정의하는 공유 프로필. 파일 확장자는 `.SRProfile`. | [[03_Architecture/0002_Surface-State\|표면 상태]], [[03_Architecture/0003_Assets-and-Profiles\|에셋]] |
| **stateCapacity** | 특정 State의 포화 기준량. 유한한 양수인 상태별 독립 프로필 파라미터이며 저장 상한이 아니다. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **Saturation** | `State / stateCapacity`로 계산하는 런타임 파생값. Transport에서는 1 초과를 허용하며 표시용 clamp와 구분한다. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **State** | 초과량까지 포함한 전체 상태량. finite, `State ≥ 0`이며 Capacity를 넘을 수 있다. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **TempState** | 초기 문서의 일반적인 Solver 중간값 이름이다. 현재 GPU 배치에는 이 이름의 단일 버퍼가 없고, `OutgoingFluxScale`, `RawOutgoing`, `InputDelta`처럼 목적별 scratch로 나뉜다. Capacity 초과량은 State에 포함한다. | [[03_Architecture/0007_Surface-GPU-Data-Layout\|GPU Resource]] |
| **Shared Surface Geometry Data** | 같은 Mesh + Normal Map을 사용하는 인스턴스가 공유할 수 있는 정적 형상 데이터. | [[03_Architecture/0004_Surface-Geometry\|형상 정보]] |
| **Surface Instance State Data** | 특정 Mesh Instance가 개별적으로 가지는 동적 State 데이터. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **SaturationDrive** | 보내는 texel과 받는 texel의 Saturation 차이에 의해 발생하는 전달 구동력. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **GeometryDrive** | 높이 차이와 중력·표면 방향에 의해 발생하는 전달 구동력. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **HeightDrive** | 이웃 texel 사이 `EffectiveHeight` 차이의 크기. `DirectionDrive`와 곱해 `GeometryDrive`를 구성한다. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **DirectionDrive** | source 표면에 투영한 중력과 source→target 방향의 정렬도. 중력 반대 방향은 0으로 처리한다. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **TransferFactor** | Profile의 `[0,1]` 무차원 전달 조절값. Saturation/Geometry 경로의 기준 속도 `1.0`, `6000.0`을 곱해 실제 Rate를 구한다. | [[05_Decisions/0008_Normalized-Transport-Factors\|Decision 0008]] |
| **TransferWeight** | 해당 이웃 관계를 실제 State가 얼마나 잘 통과하는지 보정하는 가중치. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **DistanceWeight** | 주변 평균 이웃 간격에 대한 상대 거리로 전달량을 보정하는 가중치. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **NormalWeight** | 이웃 texel의 유효 world normal 내적으로 전달량을 보정하는 가중치. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **CurvatureWeight** | 초기 비교 항목으로 추가됐지만 현재 Solver 전달량에는 적용하지 않는다. 곡률 진단 데이터와 Decay의 `ConcavityWeight`는 유지한다. | [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal\|Decision 0024]] |
| **ProfileBoundaryWeight** | 서로 다른 SRProfile 영역 사이의 전달 정도를 조절하는 가중치. | [[03_Architecture/0006_Surface-State-Update\|Solver]] |
| **ContactWeight** | 접촉 중심에서의 거리와 반경·falloff에 따라 texel이 외부 입력을 받는 정도. | [[03_Architecture/0006_Surface-State-Update\|Contact Input]] |
| **Falloff** | 접촉 중심에서 멀어질수록 입력이 줄어드는 정도를 조절하는 파라미터. State의 시간에 따른 감소를 계산하는 Decay와 구분한다. | [[03_Architecture/0005_Surface-Input\|Surface Input]] |
| **Macro Geometry** | 실제 Mesh가 만드는 거시 형상. | [[03_Architecture/0004_Surface-Geometry\|형상 정보]] |
| **Virtual Meso Geometry** | Normal Map 등에서 유도해 Simulation이 사용하는 중간 규모의 가상 표면 형상. | [형상 정보](../03_Architecture/0004_Surface-Geometry.md), [Decision 0003](../05_Decisions/0003_Normal-Map-Meso-Geometry.md) |
| **Virtual Height** | Virtual Meso Geometry의 높이 성분. Macro Geometry 기준 상대 높이 (`MesoVirtualHeight`). | [형상 정보](../03_Architecture/0004_Surface-Geometry.md) |
| **EffectiveHeight** | Macro Surface 높이와 Virtual Height에 instance transform을 반영해 평가한 world-length 높이. `HeightDrive` 계산에 사용한다. | [[05_Decisions/0010_Geometry-Transport-Mobility\|Decision 0010]] |
| **Surface Geometry Field** | Simulation texel별 정적 표면 형상 데이터 집합. | [형상 정보](../03_Architecture/0004_Surface-Geometry.md), [GPU Resource](../03_Architecture/0007_Surface-GPU-Data-Layout.md) |
| **Accumulation Height** | State를 기반으로 계산한 동적 적층 높이. Cavity Filling과 Surface Following으로 구성. | [[03_Architecture/0004_Surface-Geometry\|적층]] |
| **Wetness** | 재질 내부에 흡수된 수분 상태. 기본적으로 형상 적층을 만들지 않는다. | [[Roadmap|데모]] |
| **SurfaceWater** | 표면 위에 존재하고 흐르거나 고이는 물. `Wetness`와 구별되는 State이며 Profile/Registry에서 정의할 수 있다. 필요한 동작이 별도 물리 layer를 요구하는지는 별도 결정한다. | [[Roadmap|데모]] |
| **Simulation UV** | UV-space 상태 시뮬레이션에 사용하는 전용 좌표계. Mesh→Texel mapping과 seam neighbor 생성의 기준이다. | [[03_Architecture/0004_Surface-Geometry#Simulation Mapping\|Mapping]] |
| **State Transition** | source State가 조건을 만족하면 target State를 증가시키는 규칙. 실제 Solver 적용은 미구현이다. | [[03_Architecture/0002_Surface-State\|State Transitions]] |

`Overflow`는 과거의 초과 상태량 모델에 속하는 용어이며 현재 상태 저장 모델에서는 사용하지 않는다.
