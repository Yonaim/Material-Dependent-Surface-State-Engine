# 에셋과 Surface Response Profile

상태: **파일 구조 설계** · 근거: [[07_Assets/Documents/0003_Asset-Structure.pdf|에셋 구조]]

이 문서는 데이터의 **파일 직렬화와 Asset 연결 관계**만 정의한다. SRProfile 파라미터의 의미와 범위는 [[03_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]를 기준으로 한다.

## 파일 형식

| 용도 | 확장자 | 비고 |
|---|---|---|
| OBJ Mesh | `.obj` | 정점, UV, Normal, Face, Surface별 Material 할당 |
| Render Material | `.mtl` | OBJ Surface의 외관용 Material |
| Texture | `.png`, `.jpg` 등 | Albedo, Normal 등 |
| Scene | `.Scene` | JSON 형식. 배치 및 Asset 연결 관계 |
| Surface Response Profile | `.SRProfile` | JSON 형식. Surface State 반응 데이터 |
| Runtime Surface Data | 파일 없음 | Mesh load 때 생성해 메모리에서 공유하는 정적 Geometry·Texel 관계 및 Texel별 Profile map |

## Surface와 Profile의 관계

Render Material과 Surface Response Profile은 서로 다른 책임이다. Render Material은 외관을 정의하고, `.SRProfile`은 State에 대한 반응 파라미터와 Transition을 정의한다. 하나의 Render Material 영역이 반드시 하나의 SRProfile만 사용한다고 가정하지 않는다.

Runtime 전처리 결과는 UV texel별 Profile 연결을 포함해 각 위치의 `ProfileIndex`를 지정한다. 이로써 동일한 Render Material 내부에서도 위치별로 서로 다른 SRProfile을 사용할 수 있다. Profile Distribution은 전처리의 입력이며, 구체적인 authoring 형식과 `.Scene` 연결 규칙은 별도 결정으로 정한다. 결과는 실행 중 메모리에만 두고 `.Surface` 파일로 저장하지 않는다.

```text
UV Texel
├── Render Material assignment (rendering)
└── SurfaceProfileMap
    └── ProfileIndex → SRProfile response data
```

Runtime Surface Data는 Mesh, Normal Map, Profile Distribution으로부터 Asset/Scene load 때 생성하는 정적 데이터다. 같은 Mesh 및 Profile Distribution 조합을 사용하는 instance들은 하나의 결과를 공유한다. persistent cache metadata나 stale 판정은 두지 않는다. 데이터 범위는 다음과 같다.

| Runtime Surface Data에 포함 | Runtime Surface Data에 포함하지 않음 |
|---|---|
| 유효성, Normal, Meso Virtual Height, Curvature/Concavity 등 정적 Geometry 값 | 시간에 따라 변하는 State와 Overflow |
| Neighbor, Distance, Height Difference, Boundary, UV seam 연결 등 texel 관계 | `.SRProfile`의 반응 파라미터와 Transition |
| Texel → `ProfileIndex` map | Instance별 `SurfaceInstanceStateData` |

전처리 시점과 저장 수명은 [[../04_ADR/0008-Runtime-Surface-Preprocessing|ADR 0008 — Runtime Surface 전처리]]를 따른다. 이전 `.Surface` persistent cache 결정은 [[../04_ADR/0007-Surface-Preprocessed-Asset|ADR 0007]]에서 superseded 상태로 보존한다.

## State Registry

`.SRProfile`의 `states` key가 프로젝트에서 사용하는 State 이름을 제공한다. Profile을 로드하면서 이 이름들을 모아 `SurfaceStateRegistry`를 구성하고, 문자열 State 이름을 런타임 `StateId`/`ChannelIndex`로 변환한다. 이름 정규화 규칙과 Solver의 데이터 주도 처리 원칙은 [[../04_ADR/0006-Dynamic-State-Registry|ADR 0006 — SRProfile 기반 동적 State Registry]]를 따른다.

## `.SRProfile` 예시

아래 수치는 **튜닝 전 예시값**이며, Profile이 여러 State 응답을 정의할 수 있음을 보여준다. 예시 State 이름은 고정된 전역 채널 목록이 아니다.

```json
{
  "type": "SurfaceResponseProfile",
  "version": 1,
  "name": "Brick",
  "states": {
    "wetness": {
      "stateCapacity": 1.0,
      "inputFactor": 0.75,
      "saturationTransferRate": 0.40,
      "geometryTransferRate": 0.05,
      "decayRate": 0.06,
      "cavityRetentionFactor": 0.50,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0
    },
    "heat": {
      "stateCapacity": 1.0,
      "inputFactor": 0.30,
      "saturationTransferRate": 0.30,
      "geometryTransferRate": 0.0,
      "decayRate": 0.10,
      "cavityRetentionFactor": 0.0,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0
    },
    "burn": {
      "stateCapacity": 1.0,
      "inputFactor": 0.0,
      "saturationTransferRate": 0.0,
      "geometryTransferRate": 0.0,
      "decayRate": 0.01,
      "cavityRetentionFactor": 0.0,
      "accumulationFactor": 0.0,
      "cavityFillFactor": 0.0
    },
    "mud": {
      "stateCapacity": 1.0,
      "inputFactor": 0.65,
      "saturationTransferRate": 0.04,
      "geometryTransferRate": 0.08,
      "decayRate": 0.04,
      "cavityRetentionFactor": 0.80,
      "accumulationFactor": 0.65,
      "cavityFillFactor": 0.60
    }
  },
  "transitions": [
    {
      "source": "heat",
      "target": "burn",
      "threshold": 0.7,
      "transitionRate": 0.2
    }
  ]
}
```

## `.Scene` 연결 예시

현재 예시는 이전의 Material 이름 기반 Profile 연결 형식을 기록한 것이다. Texel별 `SurfaceProfileMap` 결정의 최종 authoring/직렬화 형식은 아직 확정되지 않았으므로, 이 예시의 `materialProfiles`를 최종 Profile 배치 계약으로 간주하지 않는다.

```json
{
  "type": "Scene",
  "version": 1,
  "objects": [
    {
      "name": "TestClothes",
      "mesh": "Assets/Models/clothes.obj",
      "surface": {
        "materialProfiles": {
          "Silk": "Assets/SurfaceProfiles/silk.SRProfile",
          "Steel": "Assets/SurfaceProfiles/steel.SRProfile",
          "Leather": "Assets/SurfaceProfiles/leather.SRProfile"
        }
      }
    }
  ]
}
```

현재 4주차 구현에서는 Simulation grid 해상도를 사용자가 `.Scene`에서 지정하지 않는다. 모든 Surface에 `512 × 512`를 적용하며, 이 값은 전처리 코드의 한 곳에서 관리한다.

Simulation UV는 렌더링 UV와 논리적으로 분리한다. UV 생성·검증과 현재 구현 범위는 [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]을 본다.
