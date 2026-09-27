# UI Interface

상태: **현재 Debug·Scene 편집 UI 구현 기준** · 최종 확인: 2026-09-27 · 관련: [[0001_Engine-Structure|엔진 구조와 데이터 흐름]], [[0006_Rendering|렌더링]], [[0008_Surface-Input|Surface Contact Input]], [[../04_ADR/0014-Surface-Contact-Target-API|ADR 0014 — Surface Contact Target API]]

## 목적과 범위

현재 UI는 렌더링·Surface simulation을 확인하고 Demo Scene을 편집하기 위한 개발 인터페이스다. `TDebugUI`는 Dear ImGui의 GLFW/Vulkan backend로 UI를 그리고, 입력 설정 및 Scene 편집을 `TApplication`, `TInputSystem`, `TRenderer`, `TScene`에 연결한다. UI는 Solver 계산이나 GPU buffer를 직접 수정하지 않는다.

## 구성요소와 책임

| 구성요소 | 책임 |
|---|---|
| `TDebugUI` | ImGui 창, 렌더·State 진단 선택, Inject 설정, runtime Profile parameter draft/override, Scene 편집, 로그 표시 및 UI 입력 capture 상태 제공 |
| `TInputSystem` | Inject 설정과 키보드 상태를 받아 Debug 접촉 event를 생성하고 Raycast 결과를 접촉 payload로 변환 |
| `TSceneFileDialog` | 플랫폼별 파일 대화상자로 `.Scene` 열기·저장 경로를 얻음 |
| `TSceneLoader` | Scene 파일을 읽어 Scene/Asset을 만들고, Scene instance와 Transform을 파일로 저장 |
| `TApplication` | 매 frame UI를 갱신하고 생성된 접촉 event를 Renderer에 전달 |
| `TRenderer` | Render mode와 선택 State를 반영하고 Surface system을 통해 접촉 입력·Profile override·Scene resource 갱신을 처리 |

## UI 배치와 기능

| 위치 / 창 | 기능 |
|---|---|
| 3D 뷰포트 상단 `Render Options` | View 선택과 State/Weight 등 선택한 렌더 뷰의 보조 선택 |
| 좌측 도킹 영역 `Scene File`, `Camera`, `Selected Transform`, `Render Settings` | Scene 열기·저장, Camera 및 선택 object Transform 조정, Normal strength·Ambient light·Normal Y 반전 설정 |
| 우측 도킹 영역 `Simulation Debug` | `Control`, `Inject`, `Runtime Override` 탭에서 simulation 재생·속도, 접촉 입력, Solver 진단, Profile parameter runtime override를 설정 |
| `Control` 탭의 `Solver debug terms` | GeometryDrive와 NormalWeight의 runtime on/off. 기본은 둘 다 on이며 solver 결과에 즉시 적용 |
| 3D 뷰포트 좌상단 | FPS, frame time, GPU Solver 시간을 반투명 overlay로 표시 |
| 하단 도킹 영역 `Log` | 로그 level 필터링, 대소문자 구분 없는 키워드 검색, 복사·삭제 |
| 중앙 도킹 영역 | 3D Scene viewport. Surface State Heatmap 등 선택한 진단 모드로 Mesh Surface를 색칠한다 |

Dear ImGui docking 기능을 사용한다. 기본 레이아웃은 좌측 Scene·렌더 설정, 우측 Surface simulation·진단 도구, 하단 Log, 가운데 3D viewport다. Render Options 바는 전체 창의 위가 아니라 가운데 영역의 위쪽에 붙는다. 3D pass의 Vulkan viewport와 scissor는 이 바 아래에서 시작하므로 바 영역에는 Scene geometry를 렌더링하지 않는다. 같은 영역을 Camera projection aspect, object 선택, gizmo projection과 Inject crosshair에도 사용한다. 도킹 분할자를 조절하거나 창을 다른 영역으로 옮길 수 있다. 배치는 `Config/MDSS_EditorLayout.ini`에 저장된다. 로그 기본 높이는 기존 360px에서 약 1.5배인 540px이며, 초기 dock 분할도 화면 높이의 약 36%를 로그에 할당한다. 시스템 Arial/Segoe UI/DejaVu Sans 폰트 중 사용 가능한 것을 쓰며, 앱 창이 1280×800 이상일 때만 폰트를 15% 키운다. 그보다 작은 창에서는 기본 크기를 유지한다.

뷰포트 좌상단 overlay의 `Solver GPU` 수치는 Vulkan timestamp query로 Solver compute 구간을 측정한다. 완료된 frame slot의 결과를 읽으므로 진단 표시를 위해 별도 GPU 대기를 추가하지 않는다. GPU timestamp를 지원하지 않는 장치에서는 `unavailable`로 표시한다.

```mermaid
flowchart TB
  subgraph Dockspace["ImGui Dockspace"]
    direction TB
    subgraph WorkArea["작업 영역"]
      direction LR
      subgraph Left["좌측 Scene 편집"]
        direction TB
        SceneFile["Scene File"]
        Camera["Camera"]
        Transform["Selected Transform"]
        RenderSettings["Render Settings"]
      end
      subgraph Center["가운데 3D viewport"]
        direction TB
        Toolbar["viewport 상단: Render Options"]
        Viewport["3D Scene View\nSurface State Heatmap"]
      end
      subgraph Right["우측 Surface simulation / debug"]
        direction TB
        SimulationDebug["Simulation Debug\nControl | Inject | Runtime Override"]
      end
    end
    Log["하단: Log + keyword search"]
  end
```

`Render Options`는 가운데 3D viewport의 상단 바에서 선택한다. 하나의 View 드롭다운 안에서 `Display`와 `Debug` 제목으로 항목을 구분한다. Display 목록은 Lit, Unlit, Vertex Normal (WS), Normal Texture (TS), Mapped Normal (WS)이고, Debug 목록은 State Heatmap, Validity, Surface ID, Neighbor Count, UV Seam, Solver Transfer Weights, Macro Geometry, Meso다. 한 번에 하나의 View Mode를 선택한다. Meso를 고르면 `Height`와 `Offset` 라디오 버튼을 선택한다. Height는 signed MesoVirtualHeight를 색으로 표시하고, Offset은 render vertex UV에서 대응 texel 높이를 읽어 mesh vertex를 변위한다. 따라서 보이는 세부 해상도는 render mesh의 vertex density로 제한된다. State/Weight selector 등 선택된 모드에 직접 필요한 옵션만 상단에 표시한다. Normal strength, Ambient light, Normal Y flip은 좌측 `Render Settings` 창에 둔다. `State Heatmap`은 선택된 State channel의 현재 GPU State 값 `State / Profile Capacity`를 3D Mesh Surface에 실시간 색으로 출력한다. `Relief Shading`은 Meso height 미분에서 복원한 normal로 밝기만 조절해 State 색상 위에 형상 음영을 얹으며, 토글로 끄면 기존 Heatmap 색을 그대로 표시한다. `Solver Transfer Weights`는 State Heatmap과 분리된 팔레트에서 낮은 값을 어두운 자주색, 높은 값을 밝은 청록색으로 표시한다. 별도의 2D UV 텍스처 창은 아직 제공하지 않는다. `OutgoingFluxScale` 시각화는 이 목록에 포함되지 않는다.

우측 `Simulation Debug` 창은 `Control`, `Inject`, `Runtime Override` 탭을 제공한다. `Control` 탭은 Running/Paused 라디오 버튼, `0.25x`, `0.5x`, `1x`, `2x` 속도 프리셋과 `0.05x`–`4x` Time scale 슬라이더를 제공한다. Pause는 Solver Delta Time을 0으로 만들고 그동안의 Inject 제출도 무시한다. Time scale은 실제 frame Delta Time에 곱해 다음 Solver step부터 적용한다. single-step 및 State snapshot/restore는 아직 구현하지 않았다.

같은 창의 `Solver debug terms`는 GeometryDrive와 NormalWeight의 runtime on/off 토글을 제공한다. GeometryDrive를 끄면 해당 flux 항만 0이 되고 SaturationDrive는 유지된다. NormalWeight를 끄면 이웃 법선 dot 가중치를 1로 두어 NormalWeight 감쇠만 제거하며 DistanceWeight와 ProfileBoundaryWeight는 유지한다. 기본은 둘 다 on이다. NormalWeight 변경 시 instance별 TransferWeight cache를 재생성하고, 사용 중인 GPU 작업이 끝난 뒤 다음 Solver step부터 적용한다. 이미 누적된 State는 토글해도 보존되므로 같은 초기 조건 비교에는 scene 재로드 등 초기화가 필요하다.

`Runtime Override` 탭은 현재 Scene에서 참조하는 `.SRProfile`만 선택할 수 있다. State parameter는 임시 draft로 편집하고 `Apply Override`를 눌렀을 때만 활성화한다. 현재 Solver가 읽는 `StateCapacity`, `InputFactor`, `SaturationTransferRate`, `GeometryTransferRate`, `DecayRate`, `CavityRetentionFactor`만 노출하며, 구현되지 않은 parameter는 편집하지 않는다.

Override는 실행 중 메모리에만 보관하며 `.SRProfile` 파일을 수정하지 않는다. 같은 Profile을 여러 Surface나 instance가 공유하면 모두 같은 값을 사용한다. Apply/Restore 시 Renderer는 Graphics queue가 사용 중인 Profile buffer를 다 쓰기를 기다린 뒤 해당 Profile record를 갱신한다. `InputFactor`는 이후 접촉 입력에, 나머지 parameter는 다음 Solver dispatch부터 반영한다. Scene resource를 다시 만들면 현재 Scene에서 사용 가능한 override를 다시 적용한다.

Surface 진단 색은 다음과 같다.

| View | 표시 |
|---|---|
| State Heatmap | 선택한 단일 State의 `State / Profile Capacity`를 0–1 범위로 표시. 짙은 남색 → 파랑 → 청록 → 노랑 gradient를 사용하고, 미지원 State는 회색, invalid texel은 어두운 색으로 표시 |
| Validity | valid texel은 초록색, invalid texel은 빨간색 |
| Surface ID | Surface마다 결정적인 표시 색상 |
| Neighbor Count | 이웃 0개는 어둡게, 8개는 밝게 표시 |
| UV Seam | 다른 UV chart에 속한 이웃이 있는 texel을 분홍색으로 표시 |

## Debug 접촉 입력 흐름

Inject UI는 입력 모드, State와 세기를 설정한다. 실제 키 입력과 Scene hit 처리는 `TInputSystem`이 담당하고, UI는 그 로직을 위한 설정값과 ImGui keyboard capture 상태만 제공한다.

```mermaid
sequenceDiagram
  actor User
  participant UI as TDebugUI
  participant App as TApplication
  participant Input as TInputSystem
  participant Ray as TRaycaster
  participant SurfaceAPI as Surface-bound SubmitContact
  participant SSS as TSurfaceStateSystem
  participant Solver as GPU Solver
  participant Params as Profile GPU buffer
  participant ParameterUI as Simulation Parameters UI
  participant Renderer as TRenderer

  App->>UI: BeginFrame and draw interface
  App->>Input: poll Debug contact using UI settings
  Input->>Input: detect Space press edge
  alt inject mode on and UI does not capture keyboard
    Input->>Ray: cast camera-forward ray
    Ray-->>Input: closest front-facing hit
    Input->>SurfaceAPI: target Surface + contact payload
    SurfaceAPI->>SSS: shared contact submission
    SSS->>SSS: map contact and upload InputDelta
    SSS->>Solver: record compute step
  else otherwise
    Input-->>App: no contact
  end
  Note over Input,SurfaceAPI: Public wrapper is designed; current code returns internal TSurfaceContactInput to App, which submits via TRenderer.

  User->>ParameterUI: edit draft and click Apply Override
  ParameterUI->>Renderer: send Profile / State / parameters
  Renderer->>Renderer: wait for Graphics queue idle
  Renderer->>Params: update selected Profile record
  Note over ParameterUI,Params: Runtime only; Restore writes the original .SRProfile values back.
```

Space를 누르고 있는 동안 반복하지 않고 새로 누른 순간 한 번만 event를 만든다. ImGui가 키보드를 capture 중이면 접촉을 만들지 않는다. `Simulation Debug` 창의 `Inject` 탭에서 State, World radius, Strength, Falloff를 조절한다. 법선·입사각 weighting은 적용하지 않는다. 입력 API의 target 결정과 공통 제출 경로는 [[0008_Surface-Input|Surface Contact Input]]을 따른다. 이 경로는 게임 Physics 입력과 같은 공개 API 계약을 사용하도록 설계한다. 현재 C++ 구현은 내부 `TSurfaceContactInput` 및 instance index로 라우팅하며, Surface-bound 공개 wrapper와 Collider adapter는 아직 연결되어 있지 않다.

## Scene 편집 흐름

- Scene의 Mesh를 화면에서 클릭해 object를 선택한다. 선택한 object는 `Selected Transform` 창과 렌더링 gizmo에 반영된다.
- Position은 숫자 입력 또는 X/Y/Z 축 이동 gizmo로 편집한다. Rotation과 Scale은 현재 숫자 입력으로 편집한다.
- `Load Scene`은 파일 대화상자와 `TSceneLoader`로 Scene을 읽고 Renderer의 Surface/GPU resource를 다시 만든 뒤 활성 Scene을 교체한다.
- `Save Scene`은 Mesh 경로, 선택적 `.SurfaceProfileMap` 경로, object Transform을 저장한다. 경로는 저장할 Scene 파일 위치 기준 상대 경로다.
- 현재 Scene 직렬화에는 Camera 설정과 시뮬레이션 State가 포함되지 않는다. Scene 편집 후 State를 보존하는 기능은 제공하지 않는다.

```mermaid
sequenceDiagram
  actor User
  participant UI as TDebugUI
  participant Dialog as TSceneFileDialog
  participant Loader as TSceneLoader
  participant Renderer as TRenderer
  participant Scene as Active TScene

  User->>UI: Load Scene
  UI->>Dialog: choose .Scene file
  Dialog-->>UI: path
  UI->>Loader: Load(path, AssetManager)
  Loader-->>UI: new Scene + assets
  UI->>Renderer: ReloadSceneResources(new Scene)
  UI->>Scene: replace active Scene

  User->>UI: edit Transform / move gizmo
  UI->>Scene: update selected instance Transform
  User->>UI: Save Scene
  UI->>Dialog: choose save path
  Dialog-->>UI: path
  UI->>Loader: Save(Scene, path)
```

## 경계와 불변 조건

- `TDebugUI`는 State 값을 직접 쓰거나 Solver pass를 dispatch하지 않는다. 입력은 접촉 payload로 전달하고, State 표시는 Renderer가 GPU의 현재 State buffer에서 읽는다. Profile parameter 편집은 Renderer API로 runtime override를 요청하며, asset 파일을 바꾸지 않는다.
- Solver term 진단 토글은 Renderer API를 거쳐 Surface State System에 전달한다. GeometryDrive 활성 상태는 push constant flag로 Solver에 전달하고, NormalWeight 상태는 CPU TransferWeight cache 재생성에 사용한다. 토글은 runtime 전용이며 설정 파일에 저장하지 않는다.
- 공개 API 설계에서는 Debug Raycast와 게임 충돌 입력이 같은 Surface contact 제출 경로를 사용한다. 현재 코드는 Debug 경로만 내부 `TSurfaceContactInput`으로 연결되어 있으며 게임 Collider adapter는 미연결이다. Debug UI 전용 State 갱신 수식이나 buffer는 두지 않는다.
- 현재 입력 조작과 Scene 편집은 Static Mesh instance에 한정된다.
- ImGui keyboard capture 중에는 Space 접촉 event를 막아 UI 조작이 State 입력으로 오인되지 않게 한다.

## 관련 문서

- [[0001_Engine-Structure|엔진 구조와 데이터 흐름]]
- [[0006_Rendering|Surface State Rendering]]
- [[0008_Surface-Input|Surface Contact Input]]
- [[../02_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|Branch 6 구현 계약]]
