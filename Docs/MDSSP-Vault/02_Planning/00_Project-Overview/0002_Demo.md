# 목표 데모

상태: **목표 정의** · 근거: [[07_Assets/Documents/0007_Target-Demos.pdf|목표 데모]]

## Brick 사이 홈을 따라 흐르는 물

```text
벽돌 위에 물을 붓는다
→ 표면 물이 홈에 고인다
→ 벽돌을 회전한다
→ 중력 방향 + 홈을 따라 흐른다
```

- **Simulation**: `SurfaceWater`가 표면 높이·기울기·중력 방향에 따라 이동하고 오목한 홈에 고임.
- **흡수**: 필요하면 SurfaceWater가 재질 내부 `Wetness`로 전환.
- **Rendering**: Wetness는 색 / roughness, SurfaceWater는 표면 물기와 Accumulation Height로 표현.

`SurfaceWater`는 이 데모가 요구하는 State 이름이다. State 종류는 코드 enum에 등록하지 않고 사용 Profile의 `states` key로 제공한다.

## 눈 맞고 들어온 사람의 옷

```text
Snow
→ SurfaceWater
→ Wetness
```

- 눈은 표면에 적층된다.
- 실내에서 녹으면 SurfaceWater가 된다.
- 녹은 물은 표면을 이동한 뒤 천 내부로 흡수되어 Wetness가 된다.

`Snow`, `SurfaceWater`는 모든 Profile이 반드시 정의할 필요는 없으며, 해당 반응을 지원하는 Profile에 State key로 추가한다.

## 등산화 밑창으로 진흙 웅덩이 밟기

- Mud가 밑창 홈과 오목한 영역에 쌓인다.
- `CurvatureWeight` / Concavity 계열 효과로 오목한 곳에서 이동이 억제된다.
- 높은 부착성 때문에 신발을 세우거나 뒤집어도 쉽게 떨어지지 않는 장면을 목표로 한다.
- `Mud State → Accumulation Height`로 밑창 홈이 실제로 메워지는 형상을 표현한다.

## 불타는 나무

```text
Heat → Burn
```

- Heat가 전파·누적되고 시간에 따라 냉각된다.
- Heat Saturation이 임계값을 넘으면 Burn이 증가한다.
- Burn은 잔류하며 표면의 그을림·탄 정도에 사용한다.

데모는 기능 목표이며 완료 기준과 성능 수치는 구현 단계에서 별도로 측정한다.
