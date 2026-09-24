# 형상 정보 반영

상태: **형상 의미와 반영 범위 확정 / 전처리 알고리즘 일부 검증 필요** · 근거: [[05_Assets/Documents/Geometry-Integration.pdf|형상 정보 반영]], [[05_Assets/Documents/Surface-System-Data.pdf|시스템 데이터 구조]]

## 반영하는 형상 정보

1. Normal
2. Distance
3. Height
4. Curvature / Concavity

Transport에는 거리, 높이·중력 방향, 표면 방향, 국소 요철이 영향을 주고, Decay에는 오목함에 따른 잔류 효과를 반영한다. 실제 Solver 수식은 [[02_Architecture/Propagation-Solver|Propagation Solver]]가 기준이다.

## 계산 및 사용 시점

```text
Offline
Normal Map
→ Meso_Virtual_Height 및 정적 형상 정보

Runtime Simulation
State
→ Accumulation_Height
→ 형상 정보 갱신

Rendering
Meso_Virtual_Height + Accumulation_Height
→ Final Meso Height
→ Normal / Parallax / Displacement 등에 반영
```

## Macro / Meso Geometry

- **Macro Geometry**: 실제 Mesh polygon이 만드는 큰 형상.
- **Meso Geometry**: Normal / Height detail이 만드는 작은 요철.

Normal Map은 실제 Mesh를 바꾸지는 않지만 Simulation에서는 Meso-Structure로 취급한다.

| 항목 | 개념식 / 의미 |
|---|---|
| Normal | Macro Surface의 tangent basis를 이용해 Meso Normal을 변환하여 최종 Normal 구성 |
| Distance | `Macro_Surface_Distance × Meso_Path_Stretch` |
| Height | `Macro_Height + Meso_Virtual_Height` |
| Curvature | `Macro_Curvature + Meso_Curvature` |

모든 Normal Map이 integrable하지는 않다. 적분 불가한 경우에는 정규화된 가상 Height / Curvature와 별도 스케일 계수를 사용하는 근사안이 있으며, 실제 알고리즘은 검증이 필요하다. [[04_Development/Experiments/Normal-Map-Integration|Normal Map 적분 실험]]

## Meso Virtual Height

`Meso_Virtual_Height = 0`이면 Macro Geometry 그대로다.

| 값 | 의미 |
|---|---|
| `< 0` | Macro Geometry보다 안쪽으로 들어간 Meso 형상 |
| `= 0` | Macro Geometry 그대로 |
| `> 0` | Macro Geometry보다 바깥쪽으로 튀어나온 Meso 형상 |

Non-integrable fallback에서는 정규화 높이를 `[-1,1]`로 두고 대표 Height 스케일을 곱하는 방식을 검토한다.

## Shared Surface Geometry Data

같은 Mesh + Normal Map을 사용하는 Instance가 공유할 수 있는 정적 형상 데이터다.

### Surface Geometry Field

| 항목 | 저장 단위 | 의미 |
|---|---|---|
| `Normal` | Texel별 | Macro + Meso를 반영한 표면 방향 |
| `Distance` | Texel × 인접 방향별 | 해당 texel과 실제 이웃 texel 사이의 표면 거리 |
| `Meso_Virtual_Height` | Texel별 | Macro 기준 Normal Map에서 복원한 상대 높이 |
| `Curvature / ConcavityWeight` | Texel별 | 국소 곡률 또는 Solver가 읽는 오목함 파생값 |

현재 Solver의 Decay는 `ConcavityWeight`를 직접 읽는다. `CurvatureWeight` 역시 Curvature/Concavity 정보에서 계산한다. Raw Curvature를 저장할지 파생 Weight만 저장할지는 GPU Resource 설계에서 확정한다.

### Geometry Common Parameters

| 항목 | 저장 단위 | 의미 |
|---|---|---|
| `Meso_Height_Reference` | Surface당 1개 | 적층량을 실제 높이로 변환할 때 사용하는 Meso 대표 높이 규모 |

## World Gravity

Static Mesh의 World Gravity를 UV-space 전파 방향으로 반영할 때는 다음 순서를 사용한다.

```text
1. 삼각형의 현재 Surface Normal 계산
2. World Gravity를 해당 삼각형 평면에 투영
3. 투영 벡터를 UV-space 방향으로 변환
4. DirectionDrive 계산에 사용
```

Static Mesh이므로 Actor Transform을 사용해 Surface Normal을 World Space로 변환할 수 있다.

## 동적 형상

적층으로 Height가 변하면 Normal / Distance / Curvature도 함께 달라지고 **후속 Simulation에 다시 반영**한다. 이 동적 형상 데이터의 실제 Instance별 GPU 저장 구조는 아직 별도 설계 전이다.

Simulation UV 생성, Mesh→Texel mapping, Valid Texel, UV Seam 및 Neighbor Index는 이 문서에서 확정하지 않는다. [[TODO|TODO]]의 후속 `Surface Simulation Mapping` 설계에서 다룬다.
