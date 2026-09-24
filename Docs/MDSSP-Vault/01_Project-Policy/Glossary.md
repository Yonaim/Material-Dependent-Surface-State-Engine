# 용어집

MDSSP Engine에서 사용하는 주요 용어의 현재 의미다.

| 용어 | 뜻 | 관련 문서 |
|---|---|---|
| **Surface State** | 표면 텍셀에 저장되는 시간에 따른 상태량. 현재 기본 채널은 `Wetness`, `Heat`, `Burn`, `Mud`. | [[02_Architecture/Surface-State\|표면 상태]] |
| **Surface Response Profile / SRProfile** | Surface가 각 State에 어떻게 반응하는지 정의하는 공유 프로필. 파일 확장자는 `.srprofile`. | [[02_Architecture/Surface-State\|표면 상태]], [[02_Architecture/Assets-and-Profiles\|에셋]] |
| **stateCapacity** | 특정 State가 가질 수 있는 최대 상태량. 상태별 독립 프로필 파라미터. | [[02_Architecture/Surface-State\|표면 상태]] |
| **Saturation** | `State / stateCapacity`로 계산하는 런타임 파생값. | [[02_Architecture/Surface-State\|표면 상태]] |
| **State** | 현재 표면에 반영된 상태량. `0 ≤ State ≤ stateCapacity`. | [[02_Architecture/Surface-State\|표면 상태]] |
| **TempState** | Solver의 중간 계산 또는 State 갱신에 사용하는 임시 상태값. Capacity 초과량 저장값이 아니다. | [[02_Architecture/Surface-State\|표면 상태]] |
| **Shared Surface Geometry Data** | 같은 Mesh + Normal Map을 사용하는 인스턴스가 공유할 수 있는 정적 형상 데이터. | [[02_Architecture/Surface-Geometry\|형상 정보]] |
| **Surface Instance State Data** | 특정 Mesh Instance가 개별적으로 가지는 동적 State 데이터. | [[02_Architecture/Surface-State\|표면 상태]] |
| **SaturationDrive** | 보내는 texel과 받는 texel의 Saturation 차이에 의해 발생하는 전달 구동력. | [[02_Architecture/Propagation-Solver\|Solver]] |
| **GeometryDrive** | 높이 차이와 중력·표면 방향에 의해 발생하는 전달 구동력. | [[02_Architecture/Propagation-Solver\|Solver]] |
| **TransferWeight** | 해당 이웃 관계를 실제 State가 얼마나 잘 통과하는지 보정하는 가중치. | [[02_Architecture/Propagation-Solver\|Solver]] |
| **ProfileBoundaryWeight** | 서로 다른 SRProfile 영역 사이의 전달 정도를 조절하는 가중치. | [[02_Architecture/Propagation-Solver\|Solver]] |
| **ContactWeight** | 접촉 중심에서의 거리와 반경·falloff에 따라 texel이 외부 입력을 받는 정도. | [[02_Architecture/Contact-Input\|Contact Input]] |
| **Meso Virtual Height** | Normal Map에서 복원한, Macro Geometry 기준의 가상 미세 높이. | [[02_Architecture/Surface-Geometry\|형상 정보]] |
| **Accumulation Height** | State를 기반으로 계산한 동적 적층 높이. Cavity Filling과 Surface Following으로 구성. | [[02_Architecture/Accumulation\|적층]] |
| **Wetness** | 재질 내부에 흡수된 수분 상태. 기본적으로 형상 적층을 만들지 않는다. | [[02_Architecture/Demos\|데모]] |
| **SurfaceWater** | 표면 위에 존재하고 흐르거나 고이는 물. 현재 기본 enum에는 없으며 확장 대상이다. | [[02_Architecture/Demos\|데모]] |
| **Simulation UV** | UV-space 상태 시뮬레이션에 사용할 좌표계. 생성·매핑·seam 처리 세부는 아직 설계 예정. | [[TODO\|TODO]] |

`Overflow`는 과거의 초과 상태량 모델에 속하는 용어이며 현재 상태 저장 모델에서는 사용하지 않는다.
