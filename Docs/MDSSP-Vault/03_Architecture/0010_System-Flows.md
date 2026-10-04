# System Flows

> **한 줄 요약:** Asset 로딩부터 Surface State 갱신과 Rendering까지의 전체 경로를 네 개의 짧은 흐름도로 연결한다.

이 문서는 **길찾기용**이다. 데이터의 정확한 의미와 수식은 각 Architecture 문서에서 정의한다.

## 전체 흐름

```mermaid
flowchart LR
  Files["Scene + Mesh + Texture<br/>Profile Map + SRProfile"] --> Runtime["Runtime Surface Data<br/>Geometry + Profile Map"]
  Files --> Registry["Scene State Registry<br/>Profile Table"]
  Runtime --> GPU["Shared Geometry<br/>Per-instance State"]
  Registry --> GPU
  Contact["Debug / Game Contact"] --> Input["InputDelta"]
  GPU --> Solver["2-Pass Surface Solver"]
  Input --> Solver
  Solver --> State["Updated Surface State"]
  State --> Render["Rendering / Debug / Accumulation"]
```

## Asset → Runtime Surface Data

```mermaid
flowchart LR
  Scene[".Scene"] --> Assets["Asset Manager"]
  Mesh["OBJ / MTL / Texture"] --> Assets
  Profiles[".SRProfile + .SurfaceProfileMap"] --> Assets
  Assets --> Preprocess["Surface Preprocess / Cache Lookup"]
  Preprocess --> Runtime["Runtime Surface Data"]
  Runtime --> Geometry["Shared Geometry"]
  Runtime --> ProfileMap["Texel Profile Map"]
  Profiles --> Registry["State Registry + Shared Profile Table"]
```

- 파일과 Asset 연결: [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]]
- 형상 전처리: [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
- GPU 소유 범위: [[03_Architecture/0007_Surface-GPU-Data-Layout|Surface GPU Data Layout]]

## Contact → InputDelta

```mermaid
flowchart LR
  Debug["Debug Raycast"] --> Target["Target Surface"]
  Physics["Game / Physics Contact"] --> Target
  Target --> Resolve["World hit → center texel"]
  Resolve --> Spread["Radius + Falloff"]
  Spread --> Delta["Accumulate InputDelta"]
  Delta --> Solver["Pass 2 consumes once"]
```

세부 API와 실패 조건은 [[03_Architecture/0005_Surface-Input|Surface Contact Input]]에서 정의한다.

## Solver Step

```mermaid
flowchart TD
  Current["Current State A/B"] --> Pass1["Pass 1<br/>Decay + RawFlux + source scale"]
  Geometry["Geometry + Neighbors"] --> Pass1
  Profile["Profile Parameters"] --> Pass1
  Pass1 --> Scratch["RawOutgoing / scale / optional RawFlux cache"]
  Scratch --> Barrier["Compute barrier"]
  Current --> Pass2["Pass 2<br/>Gather Incoming + write Next"]
  Input["InputDelta"] --> Pass2
  Barrier --> Pass2
  Pass2 --> Next["Next State"]
  Next --> Swap["A/B swap"]
```

갱신식과 시간 계약은 [[03_Architecture/0006_Surface-State-Update|Surface State Update]]를 기준으로 한다.

## State → Rendering

```mermaid
flowchart LR
  Mesh["Mesh + Material"] --> Base["Base Surface"]
  State["Current Surface State"] --> Appearance["Wetness / Mud / WaterFilm response"]
  State --> Height["Accumulation Height"]
  Height --> Overlay["Accumulation Overlay"]
  Runtime["Runtime Surface Geometry"] --> Debug["Surface / Geometry / Texel debug"]
  Base --> Frame["Viewport"]
  Appearance --> Frame
  Overlay --> Frame
  Debug --> Frame
```

렌더링 계약은 [[03_Architecture/0008_Rendering|Surface State Rendering]], 조작 UI는 [[03_Architecture/0009_UI-Interface|UI Interface]]를 본다.
