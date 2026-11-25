# Flows — 실행 흐름 지도

이 폴더는 **실제 실행 순서와 데이터 이동**을 다룬다. State 의미·수식·GPU ABI의 정본은 [[02_Architecture/01_Contracts/01_State-and-Profile|Contracts]]에 있고, 구현 파일 위치는 [[02_Architecture/02_Source/00_Source-Map|Source]]에서 찾는다.

> **C++ 상위 경로:** `FApplication::MainLoop` → `FRenderer::RenderFrame` → `FRenderer::RecordCommandBuffer`. GPU 패스 실행 순서는 아래 흐름과 [[02_Architecture/04_System-Map|System Map]]을 참고한다.

## 전체 실행 경로

```mermaid
flowchart TD
  Load[Scene + Asset Load] --> Build[Mapping + Shared Geometry + GPU Setup]
  Build --> Frame[Frame / Step Clock]
  Frame --> Contact[Contact -> InputDelta]
  Frame --> Step[0..N Simulation Step]
  Contact --> Step
  Step --> P1[Pass 1: RawFlux + Outgoing Scale]
  P1 --> B[Compute Barrier]
  B --> P2[Pass 2: Incoming + 기본 State]
  P2 --> R{Scene에 활성 Reaction 규칙?}
  R -->|있음| RP[Reaction Pass: 최종 Next State]
  R -->|없음| Geo[Accumulation + Dynamic Geometry]
  RP --> Geo
  Geo --> Next[다음 Step의 Geometry]
  Geo --> Prepare[Render State Texture + Overlay Prepare]
  Prepare --> Draw[Base + Overlay + Debug Draw]
```

## 문서 목록

| 구간 | 작은 단위 문서 | 핵심 경계 |
|---|---|---|
| 01 Initialization | [[02_Architecture/00_Flows/00_Initialization/00_Scene-Load|Scene Load]] · [[02_Architecture/00_Flows/00_Initialization/01_Surface-Build-and-GPU-Setup|Surface Build]] | 정적 전처리와 instance GPU 준비 |
| 02 Frame | [[02_Architecture/00_Flows/01_Frame/00_Frame-and-Step-Scheduling|Step Scheduling]] · [[02_Architecture/00_Flows/01_Frame/01_Contact-to-InputDelta|Contact]] | 프레임 시간과 실제 step·입력 소비 시점 |
| 03 Simulation | [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Overview]] · [[02_Architecture/00_Flows/02_Simulation/01_PreStep-and-Sparse-Scheduling|Pre-Step]] · [[02_Architecture/00_Flows/02_Simulation/02_Flux-Pass1|Pass 1]] · [[02_Architecture/00_Flows/02_Simulation/03_Gather-Pass2|Pass 2]] · [[02_Architecture/00_Flows/02_Simulation/04_Accumulation-and-Geometry-Feedback|Geometry Feedback]] | GPU pass와 barrier, State A/B |
| 04 Rendering | [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Overview]] · [[02_Architecture/00_Flows/03_Rendering/01_Render-State-Texture|State Texture]] · [[02_Architecture/00_Flows/03_Rendering/02_Texel-Overlay-Preparation|Overlay]] · [[02_Architecture/00_Flows/03_Rendering/03_Draw-and-Debug|Draw & Debug]] | 렌더 전용 파생값은 Solver로 되돌아가지 않음 |

## 조사 순서

1. 문제가 발생한 **단계**를 고른다.
2. 해당 단계에서 **읽기/쓰기 resource**와 다음 소비자를 확인한다.
3. Source 문서에서 owner와 entry point를 찾아 코드를 읽는다.
4. 값의 정합성은 Contracts, 검증 방법은 [[05_Experiments/01_Validation-and-Tests|Validation]]에서 확인한다.


> 주요 구성 관계는 [[02_Architecture/04_System-Map|System Map]]을 참고한다.

## 기존 에디터의 Surface Simulation

기본 실행은 Heated Water Channel을 연다. 좌측 `Scenes`에서 데모를 선택하거나 `File → Open Scene…`으로 `SurfaceScene` 타입의 `.Scene`을 연다. 뷰포트 상단에서 표시 모드·레이아웃·재생을 조작한다. 우측 `Simulation`은 주입과 솔버를, `Scene Setup`은 형상과 에셋을 맡는다. 실험은 좌측에서 실행하고 로그는 하단에서 확인한다. 에디터와 `--capture-surface` 및 `--check-surface`는 같은 `FSurfaceSimulationGpu` 구현를 사용한다.

```mermaid
flowchart LR
    UI["ImGui · 씬 선택 / 시뮬레이션 설정 / 재생"] --> S["고정 스텝 입력"]
    S --> R["에디터 command buffer · offscreen 뷰"]
    R --> I["같은 프레임의 main pass · ImGui Image"]
```

## 독립 실행 및 재현

`--capture-surface`은 장면 선택, 고정 서브스텝, 두 표시 슬롯, 비교 렌더링으로 진행한다. CPU·GPU 검증 플래그는 화면 대신 실험을 실행한다. [Surface Simulation 프레임과 수송 순서](../01_Contracts/08_Surface-Simulation.md#01-수송과-연산-순서)
