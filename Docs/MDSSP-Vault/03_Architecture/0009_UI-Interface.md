# UI Interface

> **한 줄 요약:** 개발 UI에서 Scene을 편집하고 렌더링과 Surface simulation을 확인·조정한다.

상태: **현재 Debug UI 구조**
관련: [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]], [[03_Architecture/0008_Rendering|렌더링]], [[03_Architecture/0005_Surface-Input|Surface Contact Input]]

---

## 화면 구성

| 영역 | 기능 |
|---|---|
| 가운데 3D Viewport | Scene 표시, object 선택, 렌더·Surface 진단 뷰 선택 |
| 좌측 패널 | 상단 `Scene`·`Animation` 탭, Camera, 선택 object Transform, 렌더 설정 |
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
- 전역 렌더 설정은 `Render Settings`에 둔다. 선택한 뷰에만 적용되는 조절기와 설명은 Viewport의 선택 뷰 설명 상자에 표시한다.
- **Display:** Lit, Unlit, Vertex Normal, Normal Texture, Mapped Normal
- **Debug:** Surface, Geometry, Texel, Solver 하위 메뉴로 구분한다.

| Group | Debug View | 표시 내용 |
|---|---|---|
| Surface | State Heatmap | 선택 State의 용량 대비 비율 (`State / (Profile Capacity × AreaScale)`) |
| Surface | Validity, Surface ID | 유효 texel, Surface 구분 |
| Surface | Neighbor Count, UV Seam | texel 이웃 수, UV seam 연결 |
| Geometry | Macro Geometry, Meso | 표면 형상, Normal Map 기반 meso 정보 |
| Geometry | Accumulation | texel 연결면에서 선택 State의 총 높이·Cavity 높이·Following 높이·Cavity Fill 비율 표시 |
| Geometry | Final Geometry | Compute가 만든 texel별 위치·normal을 연결된 삼각형으로 표시 |
| Texel | Texel Grid, Texel Area Heatmap | Simulation UV 격자, 표면 면적 분포 |
| Solver | Outgoing Flux Scale, Solver Transfer Weights | Solver 전달 관련 값 |

- State Heatmap은 용량 기준 0–100%를 표시하고 초과량은 주황색으로 표시한다. 실제 총량은 Texel Inspector에서 확인한다. 표시 결과는 Solver에 입력되지 않는다.
- Accumulation 높이 색은 선택 Profile의 Capacity로 제한된 높이를 100% 기준으로 정규화한다. Total/Cavity/Following은 각 성분의 기준 높이를 사용하며, Cavity Fill은 cavity depth 대비 0–100%다. `Display scale`은 형상 표시만 과장하며 색상 기준과 Solver 형상에는 반영하지 않는다.
- 적층 뷰는 선택 State를 따로 미리보기한다. Simulation의 `Accumulation Geometry Update`은 지원되는 모든 적층 State를 합산하는 별도 Solver 옵션이다 ([[05_Decisions/0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014]]).
- Meso 뷰는 texel 연결면의 색상 표시 또는 Displacement를 선택한다. 높이 형상 뷰의 chart 경계는 열린 상태다 ([[05_Decisions/0015_Texel-Geometry-Preview|Decision 0015]]).
- Wireframe 뷰 설명 상자에서 선을 흰색으로 통일할지 선택하고, GPU가 wide lines를 지원하면 선 굵기도 조절한다. 기본 굵기는 2 px다.
- Meso·Accumulation·Final Geometry 뷰 설명 상자에서 `Height Surface Grid`의 Off / Overlay / Grid only와 셀당 texel 수를 조절한다. Grid only도 어두운 면으로 depth를 유지한다. 새 View Mode는 추가하지 않는다 ([[05_Decisions/0016_Texel-Grid-and-Demo-Lit-Effects|Decision 0016]]).
- 좌측 `Render Settings`: Normal strength, Ambient light, Normal Y 반전
- `Lit Demo Effects`: Wetness/Mud/WaterFilm/Lava 반응, 적층 레이어 적용 여부, Dry/Wet/Mud roughness, Lit·선택 State 미리보기·Inspector가 공유하는 `Lit height display scale`. Registry와 texel Profile이 지원하는 데모 State만 반응한다.
- 선택한 뷰의 State·보조 옵션은 Viewport 상단에 표시한다.

## Scene 편집

- `Scene` 창에서 저장된 Scene을 열거나 다시 시작하거나 저장하고, Viewport에서 Static Mesh object를 선택한다.
- Transform 편집:
  - Position: 숫자 입력 또는 이동 gizmo
  - Rotation: 숫자 입력 또는 Rotate 모드의 월드 축 회전 링
  - Scale: 숫자 입력
- Scene 파일 저장 항목: 시작 Camera, 해상도, Lit height display scale, Mesh 경로, 선택적 `.SurfaceProfileMap` 경로, object Transform, 선택적 `.DemoAnim` 경로와 `initialContacts`
- Lit height display scale은 렌더링 전용 Scene 설정이다. 이전 `.Scene` 파일에서 필드가 없으면 `4.0`을 사용한다.
- 저장 경로는 Scene 파일 위치 기준 상대 경로다.
- Scene camera의 Position, Target, 수직 FOV를 저장한다. Simulation State는 저장하지 않는다.
- `Config/Engine.ini`의 `[Application] StartupScene`이 앱 시작 Scene을 지정한다. 실행 중 Scene을 바꿔도 설정은 자동 저장하지 않는다.

### Demo Animation 제어 — 구현

`.Scene`이 `.DemoAnim`을 참조하면 좌측 맨 위 도킹 영역에 `Scene`과 나란한 `Animation` 탭을 표시한다. 별도 Animation 창을 `Scene`과 같은 도킹 노드에 배치하며, Scene에 `.DemoAnim`이 없으면 탭을 표시하지 않는다. 애니메이션은 오브젝트 Transform과 선택적 Camera keyframe을 재생한다 ([[05_Decisions/0020_Scene-Referenced-Demo-Animation|Decision 0020]]).

| 설정 | 동작 |
|---|---|
| Animation Play / Pause | 애니메이션 시간과 keyframe 평가만 진행·정지 |
| Restart Animation | 애니메이션 시간을 처음으로 되돌리고 초기 keyframe을 적용. Solver State는 유지 |
| Restart Scene | 메모리에 보관한 초기 Camera·Transform·애니메이션 상태를 복원하고 Solver State를 초기화한다. 기존 Scene/GPU 자원은 유지하며 `initialContacts`를 다음 Solver step에 다시 적용 |
| Animation Speed | `0.25×`, `0.5×`, `1×`, `2×` 프리셋 또는 `0.05×`–`4×` 슬라이더로 애니메이션 시간 배율 조절 |
| Simulation Run / Pause / Step | 기존 Simulation 제어대로 Solver 갱신만 진행·정지·한 번 실행 |

- 두 재생 제어는 별도 상태와 시간 진행을 가진다. Animation을 재생해도 Solver는 자동 시작하지 않고, Solver를 재생해도 Animation은 자동 시작하지 않는다.
- Animation Speed는 UI의 애니메이션 시간에만 곱한다. Simulation Speed는 Solver에만 적용한다.
- 둘 다 실행 중이면 각 Solver step은 그 시점의 현재 Transform을 사용한다. Simulation을 멈춘 상태에서도 Animation만 계속 재생할 수 있으며 Surface State는 바뀌지 않는다.
- Animation만으로 접촉 입력을 만들거나 Simulation State를 초기화하지 않는다. `.Scene.initialContacts`는 Scene 로드와 Restart Scene 후 첫 Solver step에 적용한다.
- `.DemoAnim`이 있는 Scene에서만 Animation 제어를 표시한다.
- Save Scene이 성공하면 현재 Camera와 Transform을 재시작 기준으로 갱신한다. Scene을 다시 읽지 않아 메시·텍스처·Pipeline 로딩 비용이 Restart에 들지 않는다.

## 공통 Simulation 제어

초기 UI는 실행 제어와 Solver 설정을 `Simulation` 탭에 함께 배치했다. 현재는 자주 사용하는 실행 제어를 탭 위에 두고, 세부 기능을 `Solver`, `Contact Input`, `Profile Tuning`, `Inspector`, `Global Settings` 탭으로 나눈다.

- Running/Paused, Step, Reset State와 Speed 프리셋·조절기는 항상 표시한다.
- Playback과 Controls는 일반 텍스트 라벨을 왼쪽에, 조절 위젯을 오른쪽에 정렬한다. Speed는 프리셋과 배속 조절기를 한 행에 둔다. 패널 폭이 좁으면 같은 조절 열에서 다음 줄로 이어진다.
- 슬라이더는 더블클릭으로 수치를 직접 입력할 수 있다.
- 공통 제어와 탭 바는 고정하고, 선택한 탭의 본문만 스크롤한다.
- 공통 제어와 탭 사이 구분선 위아래에 추가 여백을 둔다. 탭 본문은 별도 배경을 그리지 않고 부모 패널의 배경을 그대로 사용한다.

| 설정 | 동작 |
|---|---|
| Run / Pause | Solver 갱신을 재생·일시정지. Pause 중 접촉 입력은 유지 |
| Step | Paused에서만 활성화. 한 번 갱신하고 Paused를 유지 |
| Reset State | State, 누적 입력, 진행·대기 시간 초기화. Profile override 복원과 별개 |
| Speed | 실제 누적 시간에 곱하는 배속. 프리셋 또는 슬라이더로 조절하며, 슬라이더 더블클릭으로 값을 직접 입력한다. 고정 기본 구간 1/60초는 유지 |

## Global Settings 탭

탭 순서는 `Solver → Contact Input → Profile Tuning → Inspector → Global Settings`다. `Global Settings`는 맨 오른쪽 탭이며 Simulation Resolution, Fixed timestep, Auto substepping을 포함한다.

| 설정 | 동작 |
|---|---|
| Simulation Resolution | Low `128 × 128`, Medium `256 × 256`, High `512 × 512` |
| Fixed timestep | 기본 ON. 실제 시간×배속을 누적하고 1/60초 구간이 모일 때 계산. Auto OFF이면 dt는 정확히 1/60초 |
| Auto substepping | 기본 OFF. ON에서만 Transport 상한에 맞춰 구간을 작은 Solver step으로 나눔. Fixed OFF·Auto OFF는 누적 시간을 한 번에 계산 |

- Scene에 해상도가 없으면 Medium을 사용한다.
- 해상도 변경 시 Surface 데이터와 GPU 자원을 다시 준비하고 State·입력을 초기화한다.
- 변경 실패 시 기존 해상도를 유지한다. (해상도 전환: Simulation Resolution Presets)

한 frame 최대 8 Solver step을 실행하며 남은 시간은 버리지 않고 이월한다. Fixed ON·Auto ON에서는 1/60초가 모인 뒤 세분화하며 미완료 구간도 다음 frame에서 재개한다. 옵션 변경은 State와 누적 시간을 유지한다. Auto OFF로 전환 시 진행 중인 구간의 잔여 길이를 한 번 마무리한 뒤 새 구간부터 1/60초를 사용한다.

Global Settings에서 step 수, step 간격, 진행 시간과 backlog를 확인한다. Pause 중에는 시간을 누적하지 않고 Step은 Solver 한 번이다. 수동 실행 dt는 Auto OFF에서 1/60초, ON에서 현재 Transport 상한이다. 지속 GPU 과부하에서는 backlog가 늘 수 있다. [[05_Decisions/0013_Fixed-Timestep-and-Auto-Substepping|Decision 0013]], [[02_Research/0004_Substepping-and-Adaptive-Time-Stepping|용어와 공식 문서]]

## Solver 탭

- Solver debug terms를 runtime에 켜고 끌 수 있다:
  - Accumulation Geometry Update: 이 옵션은 기본 OFF이며, ON에서 모든 적층 State의 Profile별 `thicknessPerAmount`로 계산한 위치·갱신 normal·이웃 거리를 다음 Solver step에 반영한다.
  - ON은 Solver step마다 DynamicGeometry와 edge weight를 갱신하는 GPU dispatch 두 개를 추가한다. OFF는 기존 정적 Geometry cache 경로를 쓴다.
  - Transport: `Saturation spreading` (`SaturationDrive`), `Gravity-guided flow` (`GeometryDrive`), DirectionDrive: MesoNormal, DistanceWeight, NormalWeight, ProfileBoundaryWeight
  - Decay: Decay, `Cavity decay protection` (`ConcavityRetention`)
  - Lit Demo Effects의 `Lit height display scale`은 Lit과 디버그 미리보기에서 공유하는 렌더링 전용 설정이며 Simulation이 읽지 않는다.
- Solver 설정에는 방향별 RawFlux cache 전환 항목이 없다. Solver는 항상 Pass 2에서 방향 flux를 재평가한다. Fixed timestep과 Auto substepping은 `Global Settings` 탭에서 조절한다.
- `Diagnostics`는 기본 접힘이다. 전체 texel 수와 유효 texel 비율을 표시하며, Paused에서는 다음 read buffer와 최근 Solver GPU 시간도 표시한다.

## Contact Input과 Profile Tuning

**Contact Input**

- 설정: State, World radius, Strength, Falloff
- Inject가 켜진 상태에서 Space를 새로 누르면 카메라 방향 Raycast로 접촉을 생성한다.
- ImGui가 키보드 입력을 처리 중이면 접촉을 생성하지 않는다.
- 게임 Physics 입력 어댑터는 아직 제공하지 않는다. 계약: [[03_Architecture/0005_Surface-Input|Surface Contact Input]]

**Profile Tuning**

- 현재 Scene에서 참조하는 `.SRProfile`과 State를 선택한다.
- 지원 parameter: StateCapacity, InputFactor, SaturationTransferFactor, GeometryTransferFactor, DecayRate, CavityRetentionFactor, CavityTransportRetentionFactor, AccumulationFactor, CavityFillFactor, ThicknessPerAmount
- UI는 `Transport: flow`에 `Saturation spread`와 `Gravity flow`, `Transport: resistance`에 `Cavity exit resistance`를 표시한다. 앞의 두 계수는 각각 포화도 차이와 중력·높이차에 의한 이동을 조절하고, 마지막 계수는 더 낮은 오목도로 나갈 때만 수송을 줄인다. `Cavity decay protection`은 별도 `Decay` 구역에서 자연 감소를 줄인다. version 4 `.SRProfile` 키도 각각 `saturationSpreadFactor`, `gravityFlowFactor`, `cavityExitResistanceFactor`, `cavityDecayProtectionFactor`다.
- 두 TransferFactor는 `[0,1]` Slider로 조절한다. UI와 GPU에는 무차원 계수를 저장하고 Solver가 기준 속도를 적용한다.
- 초안은 `Apply Override`를 눌러 적용하고 원래 값으로 복원할 수 있다.
- Override는 실행 중에만 유지되며 `.SRProfile` 파일은 수정하지 않는다.

## Texel Inspector

- Simulation Debug의 `Inspector` 탭에서 사용한다. View Mode는 추가하지 않는다.
- 어느 뷰에서든 `Shift + 왼쪽 클릭`으로 Macro mesh의 표면 texel을 선택한다. 클릭은 gizmo 조작·object 선택 대신 검사에 사용되며 Inject 모드에서도 동작한다. 변위된 실루엣을 대상으로 raycast하지 않는다.
- Instance·Surface·texel 좌표·hit triangle·Profile과 선택 State를 표시한다. State 선택은 State Heatmap/Accumulation/Final Geometry와 공유한다.
- GPU snapshot은 Raw State, texel Capacity, 상한 없는 Saturation, 기준면적 환산량, world area, 적층 파라미터, Meso 높이, Profile 두께값, 렌더 표시 배율, Cavity depth/fill/excess, 각 높이 항목과 최종 mesh-local normal을 표시한다.
- step 번호와 State A/B는 완료 sample 기준이다. Pause 후 snapshot 완료를 기다리고 Step으로 검사한다. invalid·미할당·미지원·비정상 면적/수치를 별도 상태로 표시한다.
- Scene/해상도 교체 성공 시 선택을 해제하고, 채널·표시 배율·Profile override·State reset 변경 시 이전 snapshot을 무효화한다. 표시 배율은 미리보기 설정이며 Profile 파일에 저장하지 않는다.

## 성능 표시와 경계

- Viewport overlay: FPS/frame time, GPU Render, frame의 모든 Solver 반복을 합한 전체 시간, Pass 1·2 시간.
- GPU Rendering은 overlay 준비를 형상 계산·높이 스무딩·옆면 생성으로 나눠 표시한다. 옆면 생성은 메시 정점별 State coverage 샘플링, 경계 검색 compute, 각 compute 사이 barrier, segment 생성 compute로 나눠 표시한다. 버퍼 쓰기 전·draw 읽기 전 barrier도 표시한다.
- Apple GPU의 MoltenVK에서는 render pass 내부 timestamp가 Metal encoder 끝에서 함께 기록될 수 있다. 이 환경에서는 `Render pass` 전체 시간만 표시하고 내부 draw·UI·pass begin 세부 시간을 숨긴다. `Render GPU`는 render prep과 render pass 전체의 합이다. render pass 내부의 실제 비용은 Metal GPU Capture 또는 Metal System Trace로 조사한다.
- 그 밖의 장치에서는 `Scene draw`를 render pass 준비·base mesh·mud/water draw·draw 사이·grid/gizmo 구간으로 나누고, 이후의 `UI draw`와 `Render pass end`도 표시한다. `Render GPU`는 render prep·Scene draw·UI draw·render pass 종료의 합이다. `Pass begin total`은 render pass 시작 명령 전후의 timestamp 구간이며 `Color stage`와 `Depth stage`는 각각 color attachment output과 early fragment tests 단계에서 측정한다. 이 구간들은 중첩될 수 있으므로 합산하거나 clear 전용 시간으로 해석하지 않는다.
- 시간은 1초 구간 평균으로 갱신한다. GPU timestamp query 미지원 장치에서는 측정값을 사용할 수 없다고 표시한다.
- UI는 Solver나 GPU State를 직접 수정하지 않는다. 요청은 Renderer와 Surface State System을 거친다.
- Scene 편집과 Debug 접촉 입력은 현재 Static Mesh instance에 한정된다.

## 관련 문서

- [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]]
- [[03_Architecture/0008_Rendering|Surface State Rendering]]
- [[03_Architecture/0005_Surface-Input|Surface Contact Input]]
- Surface System Tests
