# 용어집

MDSSP Engine에서 사용하는 주요 용어의 현재 의미다.

| 용어 | 뜻 | 관련 문서 |
|---|---|---|
| **Surface State** | 표면 텍셀에 저장되는 시간에 따른 상태량. State 종류는 `.SRProfile`에서 수집한 Registry가 결정하며 `Wetness`, `Heat`, `Burn`, `Mud`는 기본 demo 예시다. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **Surface Response Profile / SRProfile** | Surface가 각 State에 어떻게 반응하는지 정의하는 공유 프로필. 파일 확장자는 `.SRProfile`. | [[03_Architecture/0002_Surface-State\|표면 상태]], [[03_Architecture/0003_Assets-and-Profiles\|에셋]] |
| **stateCapacity** | 특정 State가 가질 수 있는 최대 상태량. 상태별 독립 프로필 파라미터. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **Saturation** | `State / stateCapacity`로 계산하는 런타임 파생값. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **State** | 현재 표면에 반영된 상태량. `0 ≤ State ≤ stateCapacity`. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **TempState** | Solver의 중간 계산값. 4주차 GPU 구현에서는 Pass 1의 Registry channel별 `alpha`를 저장하며 Capacity 초과량이 아니다. | [[05_Development/Notes/0003_Surface-State-GPU-Resource\|GPU Resource]] |
| **Shared Surface Geometry Data** | 같은 Mesh + Normal Map을 사용하는 인스턴스가 공유할 수 있는 정적 형상 데이터. | [[03_Architecture/0005_Surface-Geometry\|형상 정보]] |
| **Surface Instance State Data** | 특정 Mesh Instance가 개별적으로 가지는 동적 State 데이터. | [[03_Architecture/0002_Surface-State\|표면 상태]] |
| **SaturationDrive** | 보내는 texel과 받는 texel의 Saturation 차이에 의해 발생하는 전달 구동력. | [[03_Architecture/0004_Surface-State-Update\|Solver]] |
| **GeometryDrive** | 높이 차이와 중력·표면 방향에 의해 발생하는 전달 구동력. | [[03_Architecture/0004_Surface-State-Update\|Solver]] |
| **TransferWeight** | 해당 이웃 관계를 실제 State가 얼마나 잘 통과하는지 보정하는 가중치. | [[03_Architecture/0004_Surface-State-Update\|Solver]] |
| **ProfileBoundaryWeight** | 서로 다른 SRProfile 영역 사이의 전달 정도를 조절하는 가중치. | [[03_Architecture/0004_Surface-State-Update\|Solver]] |
| **ContactWeight** | 접촉 중심에서의 거리와 반경·falloff에 따라 texel이 외부 입력을 받는 정도. | [[03_Architecture/0004_Surface-State-Update\|Contact Input]] |
| **Meso Virtual Height** | Normal Map에서 복원한, Macro Geometry 기준의 가상 미세 높이. | [[03_Architecture/0005_Surface-Geometry\|형상 정보]] |
| **Accumulation Height** | State를 기반으로 계산한 동적 적층 높이. Cavity Filling과 Surface Following으로 구성. | [[03_Architecture/0005_Surface-Geometry\|적층]] |
| **Wetness** | 재질 내부에 흡수된 수분 상태. 기본적으로 형상 적층을 만들지 않는다. | [[03_Architecture/0007_Demos\|데모]] |
| **SurfaceWater** | 표면 위에 존재하고 흐르거나 고이는 물. `Wetness`와 구별되는 State이며 Profile/Registry에서 정의할 수 있다. 필요한 동작이 별도 물리 layer를 요구하는지는 별도 결정한다. | [[03_Architecture/0007_Demos\|데모]] |
| **Simulation UV** | UV-space 상태 시뮬레이션에 사용하는 전용 좌표계. Mesh→Texel mapping과 seam neighbor 생성의 기준이다. | [[05_Development/Notes/0000_Surface-Simulation-Mapping\|Mapping]] |

`Overflow`는 과거의 초과 상태량 모델에 속하는 용어이며 현재 상태 저장 모델에서는 사용하지 않는다.
