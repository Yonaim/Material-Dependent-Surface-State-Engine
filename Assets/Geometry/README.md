# Surface Simulation 형상 입력

씬의 외부 형상은 씬 기준 상대 경로인 PNG로 지정한다. `heightRangeM`은 검정에서 흰색까지 높이 범위 [m]다. 선택적인 `heightOffsetM`은 검정의 높이 [m]이며 기본값은 0이다.

```json
"heightMap": {
  "texture": "../Geometry/pocket.height.png",
  "heightRangeM": 0.01
}
```

| Height Map | 사용하는 씬 |
|---|---|
| `pocket.height.png` | Cavity Fill & Spill, Height Map Cavities의 높이맵 조건 |
| `twin-pockets.height.png` | Cavity Fill & Spill · Twin Pockets |
| `terraced-channel.height.png` | Cavity Fill & Spill · Terraced Channel |
| `brick-cube.height.png` | BrickCube Lava |
| `flat.height.png` | Areal Heat Capacity · 0.5 mm / 1 mm |
| `wax-ramp.height.png` | Wax Melt & Set |
| `phase-volume-tray.height.png` | Wax Phase Volume |
| `stepped-water-tray.height.png` | Stepped Water Leveling |
| `grooved-plate-smooth.height.png` | Drainage Geometry Comparison · Smooth Channel |

Cavity의 세 맵은 100 × 62 mm 차트의 높이 0–10 mm를 인코딩한다. BrickCube의 16비트 맵은 이전 노멀 적분 경로가 기본 256×256 격자에서 계산한 바닥을 저장한다. 용암 씬에 적힌 오프셋과 범위로 높이를 미터 단위로 복원한다.

`flat.height.png`는 일정한 0 m 높이를 입력하는 4×4 단색 맵이다. 직육면체 강판들의 상면을 동일하게 두며, 옆면 깊이는 씬의 `bodyThicknessM`을 따른다.

`wax-ramp.height.png`는 내려가는 중심 홈과 그 양옆의 뱅크를 높이 범위 15 mm로 인코딩한다. 위아래 온도는 씬의 별도 지정장에 속한다.

`phase-volume-tray.height.png`의 중앙 60% × 60%는 0 m 바닥이고 바깥은 14.7–16 mm 높이의 벽이다. Wax Phase Volume은 중앙 3×3 계산 셀에만 고체를 배치하며 바닥의 투영 면적은 40×30 mm다.

`stepped-water-tray.height.png`의 72×40 표본은 240×120 mm 차트의 0 mm와 12 mm 바닥, 36 mm 바깥 턱을 정의한다. 물이 놓인 양쪽 바닥의 투영 면적은 각각 80×72 mm다. 계산 격자는 6×5다.

`grooved-plate-smooth.height.png`는 계단형 배수 수로를 바탕으로 만든 256×144, 16비트 높이장이다. 90×50 mm 표면에서 0–5 mm 높이를 인코딩한다. 9개 바닥 구획의 높이는 유지하고 경계의 9 mm 구간을 3차 smoothstep으로 연결했다. 양쪽 벽은 각 경계의 8 mm 구간에서 완만하게 올라간다.

OBJ, normal, signed displacement는 이전 소스 비교의 기준 및 실험용 파일이다. 합성 비교 입력을 다시 생성하려면 `python Tools/surface_simulation_geometry_fixture.py Assets/Geometry --cavity`를 실행한다.
