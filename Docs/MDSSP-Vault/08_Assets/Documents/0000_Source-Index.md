# Source Index

> **한 줄 요약:** PDF 원본 자료와 각 자료가 뒷받침하는 설계 주제를 찾아볼 수 있는 색인이다.

설계 PDF는 검토·확정된 기준 내용을 판본 단위로 보존한다. 같은 판본을 작업 메모처럼 수시 수정하지 않지만, 이후 설계가 바뀌어 새 기준이 확정되면 PDF도 개정해 새 판본으로 갱신한다. Markdown의 Architecture·ADR·Flow Map은 현재 의미와 구현 상태를 계속 갱신하며, 이 색인은 PDF와 현재 설계 사이의 차이 및 보완 항목을 추적한다. 날짜가 있는 회의록은 역사 자료로 보존하고, 후속 결정은 현재 설계 문서와 개정 PDF에 반영한다.

| 파일 | 판본의 역할 | 현재 설계와 비교해 보완할 내용 |
|---|---|---|
| [[08_Assets/Documents/0001_Overall-Engine-Structure.pdf\|Overall-Engine-Structure.pdf]] | 엔진의 상위 모듈 구조 | 현재 C++의 소유 관계와 실행 순서 반영. `TRenderer`가 `TSurfaceStateSystem`을 소유하는 점, Scene별 Registry/GPU 자원 교체, Debug contact에서 Solver와 렌더 진단까지 이어지는 경로, `SurfaceGeometryUpdate`가 아직 placeholder인 점을 구분한다. 기준: [[04_Architecture/0001_Engine-Structure\|Engine Structure]], [[Flow-Maps/0000_Overview\|Flow Maps]]. |
| [[08_Assets/Documents/0002_Surface-System-Data.pdf\|Surface-System-Data.pdf]] | State/Profile와 공유·instance 데이터 기준 | `State ≤ Capacity` 상한을 폐기하고 초과량을 State에 보존한다. 단일 `TempState` 대신 목적별 scratch, Profile key로 생성되는 동적 State Registry, Scene 공유 Profile table, 고정 기준 면적에 따른 Capacity·Input·Decay 환산, 실제 GPU buffer/layout을 갱신한다. 기준: ADR 0006, 0020, 0027, 0030 및 [[04_Architecture/0002_Surface-State\|Surface State]]. |
| [[08_Assets/Documents/0003_Asset-Structure.pdf\|Asset-Structure.pdf]] | 원본 Asset과 Scene 직렬화 기준 | 확장자 대소문자 `.Scene`·`.SRProfile`, MTL 이름 기반 직접 Profile 연결 대신 `.SurfaceProfileMap`, valid texel별 `ProfileIndex`, 해상도 128/256/512 기본 256, 해상도별 `.Surface` 생성 캐시와 stale/corrupt fallback을 반영한다. Material Profile 할당과 Runtime texel map의 역할도 구분한다. 기준: ADR 0012, 0023, 0026 및 [[04_Architecture/0003_Assets-and-Profiles\|Assets and Profiles]]. |
| [[08_Assets/Documents/0004_Contact-Input.pdf\|Contact-Input.pdf]] | 접촉 입력 데이터와 라우팅 개념 | 고정 `SurfaceStateType` enum 대신 Profile에서 모은 Registry State를 쓴다. 공개 계약의 `Surface.SubmitContact(payload)`와 내부 instance 대상 식별자를 구분한다. 중심 texel의 same-triangle fallback, 별도 `texelSearchRadius`, 월드 반경/falloff, `InputDelta` 누적·업로드·1회 소비를 추가하고, Debug 경로는 연결됨·Collider/Physics adapter는 미연결로 표시한다. 기준: ADR 0013·0014, [[04_Architecture/0005_Surface-Input\|Surface Contact Input]]. |
| [[08_Assets/Documents/0005_Next-State-Calculation.pdf\|Next-State-Calculation.pdf]] | Input·Transport·Decay와 Solver 실행 방식 | 현재 기준식 하나로 정리한다. Capacity 상한 clamp를 제거하고, State/Capacity 초과 허용과 texel 면적 환산, `SaturationTransferFactor`·`GeometryTransferFactor`, `ProfileBoundaryWeight` 등 현재 용어·구동력/가중치 역할을 반영한다. 폐기된 1-pass와 예전 Height/Direction/Material Boundary weight를 현재안과 분리하고, 2-pass·선택 가능한 RawFlux cache·GPU barrier 및 검증 범위를 기록한다. 기준: ADR 0016·0020·0021·0030·0033, [[04_Architecture/0006_Surface-State-Update\|Surface State Update]]. |
| [[08_Assets/Documents/0006_Geometry-Integration.pdf\|Geometry-Integration.pdf]] | Macro/Meso Geometry, State 적층 및 렌더링 목표 | Static Normal Map 전처리의 실제 PCG height 복원·normal/curvature 계산, 실행 간 `.Surface` 캐시와 현재 WorldTexelArea/AreaScale를 설명한다. 이웃 거리는 Position에서 계산해 별도 저장하지 않는 점을 반영한다. State에서 표시용 Accumulation height 계산, texel 연결면 Preview와 Wetness/Mud/WaterFilm 데모 렌더는 구현됐지만 실제 Surface별 높이 기준과 Solver 동적 Geometry feedback은 미구현임을 구분한다. 기준: ADR 0018·0026·0030·0035–0038, [[04_Architecture/0004_Surface-Geometry\|Surface Geometry]], [[04_Architecture/0009_Rendering\|Rendering]]. |
| [[08_Assets/Documents/0007_Target-Demos.pdf\|Target-Demos.pdf]] | 목표 시연 시나리오 | 목표와 현재 완료 기능이 혼동되지 않도록 구현 현황·MVP 범위·미연결 사례를 보탠다. 현재는 Wetness/Mud/WaterFilm의 일부 외관·표시용 형상 효과가 연결되어 있으며, 옷의 Snow→Water→흡수, 실제 접촉 Physics, Heat→Burn 전이와 동적 형상 Solver feedback은 완성 시연으로 간주하지 않는다. 데모 범위가 바뀌면 PDF 목표 판본도 갱신한다. |
| [[08_Assets/Documents/0008_2026-09-11-Meeting.pdf\|2026-09-11-Meeting.pdf]] | 당시 논의의 역사 기록 | 회의록 원문은 당시 발언과 상태를 보존한다. 현재 기준과 충돌하는 Capacity 상한 등은 회의록에서 지우지 않고, 후속 결정의 날짜·ADR 링크를 Source Index와 개정 설계 PDF에서 연결한다. |

검토 기준일: **2026-09-30**. 이 표는 PDF 본문 갱신 시 반영할 항목을 정리한 검토안이며, PDF 파일 자체는 이 작업에서 변경하지 않았다.

## PDF 이후 반영된 최신 설계

- 초기 ADR 0001의 `State [0,stateCapacity]` 저장 상한은 [[05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]으로 변경했다. 현재는 전체 State A/B에 초과량을 보존하고 Capacity를 포화 기준량으로 쓴다. 초기 문서의 `TempState`는 일반 중간값 개념이며 현재는 목적별 scratch buffer로 구분한다. Shader 변경과 선택 GPU 회귀 fixture는 통과했으며, 5주차 통합 검증과 timestep 비교는 대기 중이다.
- `Saturation = State / Capacity`, 실제 texel Capacity에는 기준 면적 대비 WorldTexelArea를 반영한다. 전달 계산에서 1 초과를 허용하며 표시 정규화와 분리한다.
- Transport는 `SaturationDrive + GeometryDrive` 구조.
- `TransferWeight = Distance × Normal × Curvature × ProfileBoundary`.
- `ProfileBoundaryWeight`는 Material 이름이 아니라 **SRProfile 경계** 기준.
- `AccumulationAmount = State × accumulationFactor`.
- Wetness는 내부 흡수 수분, SurfaceWater는 표면 위 물로 구분.
- Simulation UV mapping, Meso Geometry 전처리와 GPU Resource Layout의 구현 상태·남은 한계를 별도 Markdown에서 추적한다.
