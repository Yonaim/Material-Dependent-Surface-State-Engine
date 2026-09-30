# UI Interface

> **한 줄 요약:** 개발 UI에서 Scene을 편집하고 렌더링과 Surface simulation을 확인·조정한다.

상태: **현재 Debug·Scene 편집 UI 구현 기준**
최종 확인: 2026-09-29
관련: [[0001_Engine-Structure|엔진 구조와 데이터 흐름]], [[0009_Rendering|렌더링]], [[0005_Surface-Input|Surface Contact Input]]

---

## 화면 구성

| 영역 | 기능 |
|---|---|
| 가운데 3D Viewport | Scene 표시, object 선택, 렌더·Surface 진단 뷰 선택 |
| 좌측 패널 | Scene 열기·저장, Camera, 선택 object Transform, 렌더 설정 |
| 우측 패널 | 상단 공통 실행 제어, 하단 `Solver`, `Contact Input`, `Profile Tuning`, `Inspector`, `Global Settings` 탭 |
| Viewport 상단 | FPS, GPU Render·Solver 성능 표시 |
| 하단 `Log` | 로그 level 필터, 검색, 복사·삭제 |

- 패널은 도킹 방식이며 위치와 크기를 조정할 수 있다.
- 배치 설정은 `Config/EditorLayout.ini`에 저장한다.

## UI 표시 규칙

- 설명·보조 정보는 전역 `TextDisabled` 색을 사용하며 일반 라벨·값보다 어두운 회색으로 표시한다. 줄바꿈 설명과 설명 Tooltip에도 같은 색을 적용한다.
- 체크박스는 모든 패널에서 `체크박스 → 라벨` 순서로 왼쪽에 배치한다. 슬라이더·드롭다운의 라벨과 값 열 정렬은 별도 규칙이다.

## Viewport와 렌더 설정

- `Render Options`에서 한 번에 하나의 뷰를 선택한다.
- **Display:** Lit, Unlit, Vertex Normal, Normal Texture, Mapped Normal
- **Debug:**

| Debug View | 표시 내용 |
|---|---|
| State Heatmap | Saturation (`State / (Profile Capacity × AreaScale)`) 또는 Raw State |
| Accumulation | texel 연결면에서 선택 State의 총 높이·Cavity 높이·Following 높이·Cavity Fill 비율 표시 |
| Final Geometry | Compute가 만든 texel별 위치·normal을 연결된 삼각형으로 표시 |
| Validity, Surface ID | 유효 texel, Surface 구분 |
| Neighbor Count, UV Seam | texel 이웃 수, UV seam 연결 |
| Outgoing Flux Scale, Solver Transfer Weights | Solver 전달 관련 값 |
| Texel Grid, Texel Area Heatmap | Simulation UV 격자, 표면 면적 분포 |
| Macro Geometry, Meso | 표면 형상, Normal Map 기반 meso 정보 |

- Saturation 표시 범위는 `[0,1]`이며 Capacity 초과량은 같은 색이다. Raw State는 texel 총량을 조절 가능한 고정 범위로 표시한다. 범위 초과는 주황색이다. 표시 결과는 Solver에 입력되지 않는다.
- Accumulation의 높이 범위·Height reference는 mesh-local 단위다. Cavity Fill만 0–100% 고정 범위다. 이 Height reference와 Accumulation/Final Geometry의 공통 Display scale은 디버그 렌더링 전용이며 Solver의 물리 형상에 반영하지 않는다.
- 적층 뷰는 선택 State를 따로 미리보기한다. Simulation의 `Accumulation feedback`은 지원되는 모든 적층 State를 합산하는 별도 Solver 옵션이다 ([[../05_ADR/0035-Accumulation-Debug-and-Texel-Inspector|ADR 0035]]).
- Meso 뷰는 texel 연결면의 색상 표시 또는 Displacement를 선택한다. 높이 형상 뷰의 chart 경계는 열린 상태다 ([[../05_ADR/0036-Texel-Geometry-Preview|ADR 0036]]).
- 좌측 `Render Settings`: Normal strength, Ambient light, Normal Y 반전
- `Lit Demo Effects`: Wetness/Mud 반응, Mud height 적용 여부, Dry/Wet/Mud roughness, 독립 Mud height reference. Registry와 texel Profile이 지원하는 데모 State만 반응한다.
- `Height Surface Grid`: Off / Overlay / Grid only, 셀당 texel 수. Meso·Accumulation·Final Geometry에 적용하며 Grid only도 어두운 면으로 depth를 유지한다. 새 View Mode는 추가하지 않는다 ([[../05_ADR/0037-Texel-Grid-and-Demo-Lit-Effects|ADR 0037]]).
- 선택한 뷰의 State·보조 옵션은 Viewport 상단에 표시한다.

## Scene 편집

- Scene을 열거나 저장하고, Viewport에서 Static Mesh object를 선택한다.
- Transform 편집:
  - Position: 숫자 입력 또는 이동 gizmo
  - Rotation: 숫자 입력 또는 Rotate 모드의 월드 축 회전 링
  - Scale: 숫자 입력
- Scene 파일 저장 항목: 해상도, Mesh 경로, 선택적 `.SurfaceProfileMap` 경로, object Transform
- 저장 경로는 Scene 파일 위치 기준 상대 경로다.
- Camera 설정과 simulation State는 저장하지 않는다.
- `Config/Engine.ini`의 `[Application] StartupScene`이 앱 시작 Scene을 지정한다. 실행 중 Scene을 바꿔도 설정은 자동 저장하지 않는다.

## 공통 Simulation 제어

초기 UI는 실행 제어와 Solver 설정을 `Simulation` 탭에 함께 배치했다. 현재는 자주 사용하는 실행 제어를 탭 위에 두고, 세부 기능을 `Solver`, `Contact Input`, `Profile Tuning`, `Inspector`, `Global Settings` 탭으로 나눈다.

- Running/Paused, Step, Reset State, 속도 프리셋과 Time scale은 항상 표시한다.
- Playback, Controls, Speed, Time scale은 일반 텍스트 라벨을 왼쪽에, 조절 위젯을 오른쪽에 정렬한다. 패널 폭이 좁으면 같은 조절 열에서 다음 줄로 이어진다.
- 공통 제어와 탭 바는 고정하고, 선택한 탭의 본문만 스크롤한다.
- 공통 제어와 탭 사이 구분선 위아래에 추가 여백을 둔다. 탭 본문은 별도 배경을 그리지 않고 부모 패널의 배경을 그대로 사용한다.

| 설정 | 동작 |
|---|---|
| Run / Pause | Solver 갱신을 재생·일시정지. Pause 중 접촉 입력은 유지 |
| Step | Paused에서만 활성화. 한 번 갱신하고 Paused를 유지 |
| Reset State | State, 누적 입력, 진행·대기 시간 초기화. Profile override 복원과 별개 |
| Time scale | 실제 누적 시간에 곱하는 배속. 고정 기본 구간 1/60초는 유지 |

## Global Settings 탭

탭 순서는 `Solver → Contact Input → Profile Tuning → Inspector → Global Settings`다. `Global Settings`는 맨 오른쪽 탭이며 Simulation Resolution, Fixed timestep, Auto substepping을 포함한다.

| 설정 | 동작 |
|---|---|
| Simulation Resolution | Low `128 × 128`, Medium `256 × 256`, High `512 × 512` |
| Fixed timestep | 기본 ON. 실제 시간×배속을 누적하고 1/60초 구간이 모일 때 계산. Auto OFF이면 dt는 정확히 1/60초 |
| Auto substepping | 기본 OFF. ON에서만 Transport 상한에 맞춰 구간을 작은 Solver step으로 나눔. Fixed OFF·Auto OFF는 누적 시간을 한 번에 계산 |

- Scene에 해상도가 없으면 Medium을 사용한다.
- 해상도 변경 시 Surface 데이터와 GPU 자원을 다시 준비하고 State·입력을 초기화한다.
- 변경 실패 시 기존 해상도를 유지한다. (해상도 전환: [[05_ADR/0023-Simulation-Resolution-Presets|ADR 0023]])

한 frame 최대 8 Solver step을 실행하며 남은 시간은 버리지 않고 이월한다. Fixed ON·Auto ON에서는 1/60초가 모인 뒤 세분화하며 미완료 구간도 다음 frame에서 재개한다. 옵션 변경은 State와 누적 시간을 유지한다. Auto OFF로 전환 시 진행 중인 구간의 잔여 길이를 한 번 마무리한 뒤 새 구간부터 1/60초를 사용한다.

Global Settings에서 step 수, step 간격, 진행 시간과 backlog를 확인한다. Pause 중에는 시간을 누적하지 않고 Step은 Solver 한 번이다. 수동 실행 dt는 Auto OFF에서 1/60초, ON에서 현재 Transport 상한이다. 지속 GPU 과부하에서는 backlog가 늘 수 있다. [[../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]], [[../02_Research/0004_Substepping-and-Adaptive-Time-Stepping|용어와 공식 문서]]

## Solver 탭

- Solver debug terms를 runtime에 켜고 끌 수 있다:
  - Geometry Feedback: `Accumulation feedback`은 기본 OFF이며, ON에서 모든 적층 State의 위치·갱신 normal·이웃 거리를 다음 Solver step에 반영한다. Simulation height reference는 고정 `0.01` mesh-local 단위다.
  - ON은 Solver step마다 DynamicGeometry와 edge weight를 갱신하는 GPU dispatch 두 개를 추가한다. OFF는 기존 정적 Geometry cache 경로를 쓴다.
  - Transport: SaturationDrive, GeometryDrive, DirectionDrive: MesoNormal, DistanceWeight, NormalWeight, ProfileBoundaryWeight, CurvatureWeight
  - Decay: Decay, ConcavityRetention
  - CurvatureWeight는 기본 OFF이며 변경은 이후 Solver step에 적용한다. 같은 초기 조건 비교에는 Reset이 필요하다.
  - Render Options와 Lit Demo Effects의 Height Reference는 디버그 렌더링 전용이며 Simulation height reference와 별도다.
  - CurvatureWeight 계산식과 범위는 [[0007_Simulation-Optimization|Simulation Optimization]]에 정리한다 ([[05_ADR/0019-Optional-Curvature-Transfer-Weight|ADR 0019]]).
- `Cache Comparison`은 기본 접힘이다. RawFlux Cache ON/OFF, 실제 cache buffer 크기와 비교 조건을 표시한다. Fixed timestep과 Auto substepping은 `Global Settings` 탭에서 조절한다.
- `Diagnostics`는 기본 접힘이다. 전체 texel 수와 유효 texel 비율을 표시하며, Paused에서는 다음 read buffer와 최근 Solver GPU 시간도 표시한다.

## Contact Input과 Profile Tuning

**Contact Input**

- 설정: State, World radius, Strength, Falloff
- Inject가 켜진 상태에서 Space를 새로 누르면 카메라 방향 Raycast로 접촉을 생성한다.
- ImGui가 키보드 입력을 처리 중이면 접촉을 생성하지 않는다.
- 게임 Physics 입력 어댑터는 아직 제공하지 않는다. 계약: [[0005_Surface-Input|Surface Contact Input]]

**Profile Tuning**

- 현재 Scene에서 참조하는 `.SRProfile`과 State를 선택한다.
- 지원 parameter: StateCapacity, InputFactor, SaturationTransferFactor, GeometryTransferFactor, DecayRate, CavityRetentionFactor, AccumulationFactor, CavityFillFactor
- 두 TransferFactor는 `[0,1]` Slider로 조절한다. UI와 GPU에는 무차원 계수를 저장하고 Solver가 기준 속도를 적용한다.
- 초안은 `Apply Override`를 눌러 적용하고 원래 값으로 복원할 수 있다.
- Override는 실행 중에만 유지되며 `.SRProfile` 파일은 수정하지 않는다.

## Texel Inspector

- Simulation Debug의 `Inspector` 탭에서 사용한다. View Mode는 추가하지 않는다.
- 어느 뷰에서든 `Shift + 왼쪽 클릭`으로 Macro mesh의 표면 texel을 선택한다. 클릭은 gizmo 조작·object 선택 대신 검사에 사용되며 Inject 모드에서도 동작한다. 변위된 실루엣을 대상으로 raycast하지 않는다.
- Instance·Surface·texel 좌표·hit triangle·Profile과 선택 State를 표시한다. State 선택은 State Heatmap/Accumulation/Final Geometry와 공유한다.
- GPU snapshot은 Raw State, texel Capacity, 상한 없는 Saturation, 기준면적 환산량, world area, 적층 파라미터, Meso 높이, Height reference, Cavity depth/fill/excess, 각 높이 항목과 최종 mesh-local normal을 표시한다.
- step 번호와 State A/B는 완료 sample 기준이다. Pause 후 snapshot 완료를 기다리고 Step으로 검사한다. invalid·미할당·미지원·비정상 면적/수치를 별도 상태로 표시한다.
- Scene/해상도 교체 성공 시 선택을 해제하고, 채널·Height reference·Profile override·State reset 변경 시 이전 snapshot을 무효화한다. Height reference는 미리보기 설정이며 Profile 파일에 저장하지 않는다.

## 성능 표시와 경계

- Viewport overlay: FPS/frame time, GPU Render, frame의 모든 Solver 반복을 합한 전체 시간, Pass 1·2 시간
- 시간은 1초 구간 평균으로 갱신한다. GPU timestamp query 미지원 장치에서는 측정값을 사용할 수 없다고 표시한다.
- UI는 Solver나 GPU State를 직접 수정하지 않는다. 요청은 Renderer와 Surface State System을 거친다.
- Scene 편집과 Debug 접촉 입력은 현재 Static Mesh instance에 한정된다.

## 관련 문서

- [[0001_Engine-Structure|엔진 구조와 데이터 흐름]]
- [[0009_Rendering|Surface State Rendering]]
- [[0005_Surface-Input|Surface Contact Input]]
- [[../03_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|Branch 6 구현 계약]]
