# 형상 정보와 적층

> **한 줄 요약:** Surface Solver가 사용하는 Macro Geometry와 Virtual Meso Geometry, texel 이웃 및 누적 형상 데이터의 계약을 정의한다.

상태: **형상 의미와 반영 범위 확정 / 전처리 알고리즘 일부 검증 필요** · 근거: [[08_Assets/Documents/0006_Geometry-Integration.pdf|형상 정보 반영]], [[08_Assets/Documents/0002_Surface-System-Data.pdf|시스템 데이터 구조]]

## 반영하는 형상 정보

1. Normal
2. Distance
3. Height
4. Curvature / Concavity

Transport에는 거리, 높이·중력 방향, 표면 방향, 국소 요철이 영향을 주고, Decay에는 오목함에 따른 잔류 효과를 반영한다. 실제 Solver 수식은 [[04_Architecture/0006_Surface-State-Update|Surface State Update]]가 기준이다.

## 계산 및 사용 시점

```mermaid
flowchart LR
  Mesh["Mesh + Simulation UV"] --> Lookup["Mesh + Map + resolution\nCache validation"]
  NormalMap["Normal Map"] --> Lookup
  ProfileMap["Profile Distribution"] --> Lookup
  Lookup -->|valid .Surface| Shared["Shared static CPU geometry"]
  Lookup -->|missing / stale / corrupt| Preprocess["Mapping / Normal Map sample\nMeso integration / derivatives"]
  Preprocess --> Shared
  Preprocess --> Save["Save resolution-specific .Surface"]

  State["Per-instance State"] --> Amount["State × AccumulationFactor"]
  Profile["CavityFillFactor"] --> Split["Cavity / surface allocation"]
  Amount --> Split
  Geometry["Virtual Height"] --> Split
  Split --> Accumulation["AccumulationHeight"]
  Geometry --> Final["Virtual Height + Accumulation Height"]
  Accumulation --> Final
  Final --> Render["Rendering\nNormal / Parallax / Displacement"]
  Final -. "dynamic geometry feedback\n(designed path)" .-> Updated["Updated positions / normals"]
  Updated -.-> Simulation["Later simulation steps"]
```

정적 전처리는 고유 Mesh·Profile Distribution·해상도 조합의 `.Surface` 캐시가 없거나 무효일 때 load 시 수행한다. 유효한 캐시에서는 최종 CPU Geometry를 직접 복원하며, 같은 조합의 instance는 Runtime 메모리 결과를 공유한다. 매 frame이나 instance마다 전처리하지 않는다. 초기 Runtime 전처리 전용 설계에서 해상도별 persistent cache를 재사용하는 설계로 변경했다. 저장 수명과 무효화 규칙은 [[05_ADR/0026-Resolution-Surface-Cache|ADR 0026]]와 [[04_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]을 따른다.

## Macro Geometry and Virtual Meso Geometry

- **Macro Geometry**: 실제 Mesh polygon이 만드는 거시 형상.
- **Virtual Meso Geometry**: Normal Map 등 세부 표면 정보에서 유도해 Solver가 기하 정보처럼 사용하는 중간 규모 형상 표현. 원본 Mesh Geometry는 바꾸지 않으며, Virtual Height와 유효 Normal·Curvature 등으로 구성해 Simulation 입력으로 사용한다. 렌더링은 별도 경로에서 Virtual Height를 이용해 render vertex를 변위할 수 있다.

따라서 `Meso`는 표면 형상의 스케일을, `Virtual`은 실제 Mesh 변형이 아닌 시뮬레이션용 표현 방식을 나타낸다. 이 문서에서 Virtual Meso Geometry 내부의 개별량은 `Virtual Height`, `Normal`, `Curvature` 등으로 부른다.

| 항목        | 개념식 / 의미                                                         |
| --------- | ---------------------------------------------------------------- |
| Normal    | Macro Surface의 tangent basis를 이용해 Virtual Meso Geometry에서 유도한 normal을 변환하여 최종 Normal 구성 |
| Distance  | `Macro_Surface_Distance × Meso_Path_Stretch`                     |
| Height    | `Macro_Height + Meso_Virtual_Height`                             |
| Curvature | `Macro_Curvature + Meso_Curvature`                               |

Normal Map은 Simulation UV에 대응하는 각 texel에서 sample한다. 이웃 texel 간 mesh-local 위치와 변환된 Normal Map normal로 방향별 높이차를 계산한 뒤, 연결 graph 전체에서 그 차이를 최소제곱으로 만족시키는 height field를 구한다. Normal Map의 기울기는 완전히 적분 가능하지 않을 수 있다. 이 경우 residual을 기록하고 least-squares 해를 그대로 사용한다. 유효한 normal sample이 없거나 정규 macro normal과 반대 방향인 texel은 높이 0으로 남기고 적분 graph에서 제외한다. 자세한 수식과 한계는 [[05_ADR/0018-Normal-Map-Meso-Geometry|ADR 0018]]을 따른다.

```mermaid
flowchart LR
  Map["Normal Map"] --> Sample["Simulation texel UV로 sample"]
  Sample --> Edge["이웃 간 signed height difference"]
  Neighbor["Texel neighbor graph\nUV seam 연결 포함"] --> Edge
  Edge --> Solve["Anchored least-squares / PCG"]
  Solve --> Height["MesoVirtualHeight"]
  Height --> Derivative["Local height derivative fit"]
  Derivative --> Normal["MesoNormal"]
  Derivative --> Curvature["Mean + Gaussian curvature"]
  Curvature --> Concavity["Positive mean curvature → ConcavityWeight"]
  Height --> GeometryDrive["GeometryDrive + DistanceWeight"]
  Normal --> Transfer["TransferWeight NormalWeight cache"]
```

## Virtual Height

Virtual Height는 Virtual Meso Geometry의 높이 성분이며, Normal Map에서 복원한 Macro Geometry 기준 상대 높이다. 구현 필드 이름은 `MesoVirtualHeight`다.

`Meso_Virtual_Height = 0`이면 Macro Geometry 그대로다.

| 값     | 의미                                  |
| ----- | ----------------------------------- |
| `< 0` | Macro Geometry보다 안쪽으로 들어간 Virtual Meso Geometry 형상 |
| `= 0` | Macro Geometry 그대로                  |
| `> 0` | Macro Geometry보다 바깥쪽으로 튀어나온 Virtual Meso Geometry 형상 |

각 연결 component에서 첫 유효 texel을 내부 기준점으로 고정해 해의 임의 상수를 없앤 뒤, component 평균 높이를 0으로 이동한다. 서로 끊긴 chart는 같은 기준 높이를 강제로 공유하지 않는다. UV seam은 Mapping neighbor graph가 연결한 경우에만 함께 적분한다. 높이차는 mesh-local 길이 단위이므로 별도 `β_meso` 또는 authoring scale을 곱하지 않는다. 이 선택은 물리적 mesh scale을 사용하므로 Mesh 크기를 바꾸면 복원 높이도 같은 비율로 바뀐다.

Non-integrable 입력에 별도 임계값 기반 거부는 두지 않는다. 최소제곱이 가장 가까운 일관된 height field를 반환하며, relative edge residual을 전처리 로그에 남긴다. 기울기 입력이 invalid하거나 macro normal에 거의 수직/반대인 texel은 neutral height 0으로 두고, 해당 연결은 적분에서 제외한다.

## Shared Surface Geometry Data

형상 값은 Mesh·Normal Map·해상도에 종속된다. 현재 `TSharedSurfaceGeometryData`는 texel Profile map도 함께 소유하므로 같은 Mesh·Profile Distribution·해상도 조합의 instance가 공유한다.

### Surface Geometry Field

| 항목 | 저장 단위 | 의미 |
|---|---|---|
| `Normal` | Texel별 | Macro mesh의 기저 표면 방향 |
| `TransferNormal` | 전처리 중 CPU texel별 | Simulation mapping의 triangle/barycentric 대응으로 Normal Map을 sample하고 tangent-space 방향을 mesh-local로 바꾼 값. TransferWeight cache 생성에 사용하며 geometric `Normal`이 fallback이다. GPU shared-geometry buffer에는 올리지 않는다. |
| `MesoNormal` | Texel별 CPU/GPU | 적분한 Virtual Height의 국소 미분으로부터 재구성한 mesh-local 유효 normal. 미분 fit이 불가능하면 sampled `TransferNormal`, 그것도 없으면 macro `Normal`을 사용한다. |
| `NeighborIndex` | Texel × 최대 8개 | seam을 포함한 실제 이웃 texel 인덱스 |
| Neighbor Distance | 저장하지 않음 | Solver가 이웃 Position 간 차이에서 필요할 때 계산 |
| `Meso_Virtual_Height` | Texel별 | Virtual Height를 저장하는 구현 필드. Macro Geometry 기준 Normal Map 복원 상대 높이 |
| `MesoMeanCurvature` | Texel별 CPU/GPU | height field의 국소 이차 fit에서 계산한 signed mean curvature. 단위는 1/mesh-local length다. |
| `MesoGaussianCurvature` | Texel별 CPU/GPU | height field의 국소 이차 fit에서 계산한 Gaussian curvature. 단위는 1/(mesh-local length²)다. 곡면이 볼록/오목/안장인지 보조적으로 구분한다. |
| `ConcavityWeight` | Texel별 CPU/GPU | 양의 signed mean curvature에 평균 이웃 간격을 곱해 `[0,1]`로 clamp한 Decay 전용 cavity retention 입력. 평탄/볼록 영역은 0이다. |

현재 Solver의 Decay는 `ConcavityWeight`를 직접 읽는다. Mean/Gaussian curvature는 형상 데이터로 생성한다. Transport의 `CurvatureWeight`는 기본 OFF에서 중립값 `1.0`이며, ON이면 Virtual Height에서 유도한 mean curvature 기반 사전 계산 가중치를 사용한다. 식과 제한은 [[05_ADR/0019-Optional-Curvature-Transfer-Weight|ADR 0019]]를 따른다. `NormalWeight`가 이웃 유효 normal 차이를 반영하므로 곡률 항을 더하면 굽힘 효과를 중복할 수 있다. GPU storage layout은 [[06_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]와 ADR 0018을 따른다.

### Geometry Common Parameters

| 항목 | 저장 단위 | 의미 |
|---|---|---|
| `Meso_Height_Reference` | Surface당 1개 | 적층량을 실제 높이로 변환할 때 사용하는 Virtual Meso Geometry의 대표 높이 규모 |

## 해상도별 정적 Geometry 캐시

캐시 지점은 **`BuildMesoGeometry()` 완료 직후, GPU 업로드와 instance별 TransferWeight 생성 전**이다. Normal Map 법선만 저장하면 PCG 높이 적분과 미분 fit 비용이 남으므로 최종 CPU Geometry 전체를 저장한다.

| texel별 저장 필드 | 표현 | 복원하는 의미 |
|---|---|---|
| `Surface`, `Triangle`, `Chart` | 각각 `uint32` | 유효성/sentinel, Surface 소속, 원본 삼각형과 UV chart |
| `Barycentric` | `float32 × 3` | 삼각형 내 표면 대응 |
| `Position`, `Normal` | 각각 `float32 × 3` | mesh-local Macro 위치와 법선 |
| `TransferNormal`, `MesoNormal` | 각각 `float32 × 3` | 샘플 Normal Map 법선과 높이에서 유도한 mesh-local 법선 |
| `HasTransferNormal`, `HasMesoNormal` | 하나의 `uint32`에 두 bit | 기존 법선 fallback 규칙 보존 |
| `MesoVirtualHeight`, `ConcavityWeight`, `MesoMeanCurvature`, `MesoGaussianCurvature` | 각각 `float32` | 최종 상대 높이·오목함·두 곡률 |
| `NeighborIndices[8]` | `uint32 × 8` | seam을 포함한 최종 이웃 graph. invalid 슬롯 포함 |
| texel `ProfileIndex` | `uint32` | 별도 순서 있는 Profile table 참조. render-only sentinel 포함 |

각 레코드는 위 필드를 순서대로 저장하며 padding 없는 128 byte다. 이는 디스크 직렬화 크기이며 CPU `sizeof`나 GPU buffer stride를 뜻하지 않는다. 파일 전체에는 Surface별 ID·해상도를 저장하고 dense range는 로드 시 동일하게 재구성한다. Profile 경로/순서와 입력·버전 metadata도 함께 저장한다.

128·256·512의 Mapping, neighbor graph, PCG 적분과 미분 결과는 각각 해당 grid에서 계산해 별도 파일로 유지한다. 높은 해상도의 결과를 낮은 해상도로 단순 축소하지 않는다. 같은 Mesh·Map에서 해상도를 다시 선택하면 보관된 변형을 로드할 수 있다.

다음 항목은 `.Surface`에 저장하지 않는다.

- Normal Map image decode 결과, tangent basis, PCG의 RHS·탐색 벡터·잔차 등 전처리 임시값.
- 이웃 Distance와 간선별 Height Difference. 저장한 Position·Normal·Virtual Height에서 계산한다.
- GPU packing으로 생성하는 `ReverseNeighborSlots`, GPU buffer·descriptor handle.
- instance의 월드 위치·법선, transform/옵션에 의존하는 `TransferWeight` 및 debug averages. Geometry를 로드한 뒤 instance마다 계산한다.
- State A/B, InputDelta, RawOutgoing·RawFlux·OutgoingFluxScale 등 매 실행/step의 동적 데이터와 `.SRProfile` 반응 파라미터.

CPU 저장 표현은 GPU ABI와 분리한다. 캐시 hit는 정적 CPU 전처리를 생략하는 최적화이며 GPU 자원 생성과 Solver 동작·동적 적층 설계를 변경하지 않는다. `.Surface` 로드 결과는 기존 `TSharedSurfaceGeometryData`를 복원하므로 Normal fallback, UV seam, TransferWeight 및 렌더링은 같은 데이터를 사용한다.

## World Gravity

Static Mesh의 World Gravity를 UV-space 전파 방향으로 반영할 때는 다음 순서를 사용한다.

```text
1. 삼각형의 현재 Surface Normal 계산
2. World Gravity를 해당 삼각형 평면에 투영
3. 투영 벡터를 UV-space 방향으로 변환
4. DirectionDrive 계산에 사용
```

Static Mesh이므로 Actor Transform을 사용해 Surface Normal을 World Space로 변환할 수 있다.

```mermaid
flowchart LR
  Gravity["World Gravity"] --> Project["Project onto triangle plane"]
  Normal["Current Surface Normal"] --> Project
  Project --> Tangent["Convert direction to UV / tangent space"]
  Triangle["Triangle UV basis"] --> Tangent
  Tangent --> Drive["DirectionDrive for neighbor transfer"]
```

## 동적 형상

적층으로 Height가 변하면 Normal / Curvature가 달라져 **후속 Simulation에 다시 반영**된다. 최신 유효 Position에서 이웃 거리를 계산하며, Solver 성능 경로는 원시 거리 대신 이 값에서 만든 간선별 TransferWeight를 형상 revision 동안 캐시한다. 현재 Runtime은 MesoVirtualHeight를 geometry scalar로 보유하고, instance별 동적 AccumulationHeight 생성은 후속 구현이다. 높이 또는 갱신 normal이 바뀌면 해당 instance TransferWeight cache를 다시 준비한다. GPU 소유와 동기화는 [[04_Architecture/0007_Simulation-Optimization|Simulation Optimization]]와 [[06_Development/Notes/0003_Surface-State-GPU-Resource|GPU resource 설계]]를 따른다.

Simulation UV 생성, Mesh→Texel mapping, Valid Texel, UV Seam 및 Neighbor Index는 [[06_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]에서 정의한다. Shared Geometry의 GPU 배치는 [[06_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]를 본다.

## Accumulation Height

적층은 State를 직접 변경하는 Solver 항이 아니라, 계산된 State를 **형상상의 높이 변화**로 변환하는 후속 Geometry 계산이다.

$$
Accumulation\_Height = Cavity\_Filling\_Height + Surface\_Following\_Height
$$

- **Cavity Filling**: Macro Surface 기준 아래쪽의 Virtual Meso Geometry cavity를 메운다.
- **Surface Following**: 기존 Virtual Meso Geometry의 요철을 따라 표면 바깥쪽으로 쌓인다.

적층은 전체 State에서 계산하므로 Capacity 초과량을 별도로 다시 더하지 않는다. `stateCapacity`는 형상 두께의 상한도 아니다. 아래 Accumulation 경로는 설계이며 실제 동적 적층은 미구현이다.

### 전체 적층량과 배분

$$
Accumulation\_Amount = State \times Accumulation\_Factor
$$

- `State`는 유한한 비음수 전체 상태량이며 Capacity 초과량도 포함한다. 포화 기준량으로 상한 clamp하지 않는다. [[05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]의 Solver 변경은 구현했으며 GPU 실행 검증은 대기 중이다.
- `Accumulation_Factor ∈ [0,n]`
- `Accumulation_Factor = 0`이면 State가 있어도 형상 적층을 만들지 않는다.

$$
Cavity\_Amount = Accumulation\_Amount \times Cavity\_Fill\_Factor
$$

$$
Surface\_Amount = Accumulation\_Amount \times (1-Cavity\_Fill\_Factor)
$$

`Cavity_Fill_Factor ∈ [0,1]`이며 SRProfile에서 결정한다. `Cavity_Amount`는 전체 cavity 깊이를 100% 채우는 양을 `1`로 둔 정규화 비율이다. 따라서 `0.4`는 깊이의 40%를 채우며, `1`을 넘는 초과분은 cavity를 더 채우지 않고 Surface Following으로 넘긴다.

### 실제 높이와 Cavity 상한

```text
Cavity_Depth = max(-Meso_Virtual_Height, 0)
Cavity_Fill = min(Cavity_Amount, 1)
Cavity_Excess = max(Cavity_Amount - 1, 0)
Cavity_Filling_Height = Cavity_Fill × Cavity_Depth
Surface_Following_Height = (Surface_Amount + Cavity_Excess)
                           × Meso_Height_Reference
Accumulation_Height = Cavity_Filling_Height + Surface_Following_Height
```

Cavity는 최대 100%까지만 채우며, 초과 적층량은 버리지 않고 Surface Following으로 넘긴다.

```mermaid
flowchart LR
  State["State"] --> Total["AccumulationAmount\nState × AccumulationFactor"]
  AccFactor["CavityFillFactor"] --> Split["Split total amount"]
  Total --> Split
  Split --> Cavity["CavityAmount"]
  Split --> Surface["SurfaceAmount"]
  Depth["CavityDepth = max(-MesoVirtualHeight, 0)"] --> Fill["CavityFill = min(CavityAmount, 1)"]
  Cavity --> Fill
  Fill --> CavityHeight["CavityFillingHeight\nCavityFill × CavityDepth"]
  Cavity --> Excess["CavityExcess = max(CavityAmount - 1, 0)"]
  Surface --> Following["SurfaceFollowingHeight"]
  Excess --> Following
  Reference["MesoHeightReference"] --> Following
  CavityHeight --> Sum["AccumulationHeight"]
  Following --> Sum
  Sum --> Final["Final Surface Height"]
```

### 최종 높이

$$
DynamicFinalHeight = MacroHeight + MesoVirtualHeight + AccumulationHeight
$$

Accumulation Height로 변한 형상은 Rendering뿐 아니라 다음 Simulation의 Normal / Height / Curvature에도 다시 반영한다. Neighbor Distance는 Position 기반으로 필요할 때 계산한다. [[05_ADR/0003-Dynamic-Accumulation-Geometry|ADR 0003]]

예를 들어 Wetness / Heat / Burn은 형상 적층이 없도록 `accumulationFactor = 0`을 사용할 수 있고, Mud는 적층을 표현할 수 있다. State 종류는 고정 목록이 아니며, SurfaceWater / Snow 등 다른 State의 적층 동작도 해당 Profile 파라미터로 정의한다.
