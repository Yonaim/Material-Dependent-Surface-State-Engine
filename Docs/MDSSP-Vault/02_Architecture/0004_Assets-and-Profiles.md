# 에셋과 Surface Response Profile

상태: **파일 구조 설계** · 근거: [[05_Assets/Documents/0003_Asset-Structure.pdf|에셋 구조]]

이 문서는 데이터의 **파일 직렬화와 Asset 연결 관계**만 정의한다. SRProfile 파라미터의 의미와 범위는 [[02_Architecture/0003_Surface-State|표면 상태와 데이터 구조]]를 기준으로 한다.

## 파일 형식

| 용도 | 확장자 | 비고 |
|---|---|---|
| OBJ Mesh | `.obj` | 정점, UV, Normal, Face, Surface별 Material 할당 |
| Render Material | `.mtl` | OBJ Surface의 외관용 Material |
| Texture | `.png`, `.jpg` 등 | Albedo, Normal 등 |
| Scene | `.Scene` | JSON 형식. 배치 및 Asset 연결 관계 |
| Surface Response Profile | `.SRProfile` | JSON 형식. Surface State 반응 데이터 |

## Surface와 Profile의 관계

현재 설계에서 **하나의 Surface는 하나의 Render Material과 하나의 SRProfile을 사용**한다. 서로 다른 Surface가 같은 Material / SRProfile을 공유할 수 있다.

OBJ를 로드하면 각 Surface가 사용하는 MTL Material 이름을 알 수 있고, `.Scene`의 `materialProfiles`에서 같은 이름으로 `.SRProfile`을 찾는다.

```text
Surface
├── Render Material
│   └── MTL "Silk"
└── Surface Response
    └── materialProfiles["Silk"]
        └── silk.SRProfile
```

별도의 Texel별 Profile ID Map은 현재 설계에 필요하지 않다.

## `.SRProfile` 예시

아래 수치는 **튜닝 전 예시값**이며, 키 구조를 보여주기 위한 것이다.

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

```json
{
  "type": "Scene",
  "version": 1,
  "objects": [
    {
      "name": "TestClothes",
      "mesh": "Assets/Models/clothes.obj",
      "surface": {
        "stateResolution": [512, 512],
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

Simulation UV는 렌더링 UV와 논리적으로 분리한다. 4주차에는 조건을 만족하도록 준비한 OBJ의 기존 `vt`를 Simulation UV로 임시 사용하며, 별도 채널 또는 전처리 cache로 보존할 최종 Asset 형식은 후속 설계로 남긴다. [[04_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]을 본다.
