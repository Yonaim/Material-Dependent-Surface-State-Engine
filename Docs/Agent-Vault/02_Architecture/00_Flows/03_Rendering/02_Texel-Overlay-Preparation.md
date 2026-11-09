# Texel Overlay Preparation

Solver의 Accumulation Height를 화면용 top geometry와 side·coverage·indirect command로 변환한다. 이 단계의 smoothing/표시 배율은 Solver 입력으로 피드백하지 않는다.

**연결 문서**
- [[02_Architecture/02_Source/01_Shaders/01_Overlay/01_Geometry-and-Coverage|Overlay Geometry Shaders]]
- [[02_Architecture/00_Flows/03_Rendering/03_Draw-and-Debug|Draw / Debug]]

## 실행 조건과 산출물

| 단계 | 입력 → 출력 | 실행 조건·담당 코드 |
|---|---|---|
| Texel preview | 정적 texel mesh·현재 높이 → computed top vertex·normal | geometry debug view 또는 Lit overlay가 필요할 때 `TexelGeometryPreview.cpp` |
| Overlay 준비 | computed geometry·State coverage → top/side index·segment·indirect draw | 활성 Mud/WaterFilm/Lava의 공통 표면에 `AccumulationOverlaySides.cpp`와 coverage shader |

## Render Height와 Geometry Cache

Solver의 `AccumulationHeight`는 지원하는 적층 State의 기여를 합친 공통 높이다. State driven geometry가 켜져 있으면 Solver는 이 높이로 동적 위치·법선·오목도·transfer weight를 만들고 다음 step이 사용한다. [`FTexelGeometryPreview::Record`](../../../../../Source/Rendering/Overlay/Geometry/TexelGeometryPreview.cpp)는 해당 높이를 읽어 화면용 top geometry cache를 준비한다. 선택된 smoothing 옵션이 있으면 [`FHeightFieldSmoothing::Record`](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.cpp)가 표시용 geometry buffer를 만들고, [`FAccumulationOverlaySides::Record`](../../../../../Source/Rendering/Overlay/AccumulationOverlaySides.cpp)가 경계 옆면·coverage·draw command를 준비한다. `LitHeightDisplayScale`과 이 smoothing은 Solver 버퍼에 쓰지 않는다.

## Coverage와 Indirect Command

Computed geometry와 State coverage로 top/side의 표시 여부를 정하고 indirect draw 자료를 준비한다. 소스의 실제 variant와 dispatch 조건은 `TexelGeometryPreview.cpp`, `AccumulationOverlaySides.cpp`와 shader manifest를 함께 본다.
