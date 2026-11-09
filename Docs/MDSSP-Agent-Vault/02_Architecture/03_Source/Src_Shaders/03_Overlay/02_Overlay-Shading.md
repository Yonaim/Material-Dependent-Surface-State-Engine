# Overlay — Material Shading

공통 overlay top/side에 적용되는 계산으로, material appearance는 실험용 Demo 정책이며 범용 State 물성 계약이 아니다.

**연결 문서**
- [[02_Architecture/01_Flows/04_Rendering/03_Draw-and-Debug|Draw]]
- [[02_Architecture/03_Source/Src_Shaders/02_Surface-Rendering|Surface Shaders]]

## Overlay shading

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [OverlaySurface.glsl](../../../../../../Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl) | `SamplePixelOverlayNormal`, `SampleSeamPartnerStates`, `HasVisibleOverlayState`, fragment `main` | computed vertices·render state를 샘플해 seam-aware overlay material과 fragment output 구성 |
| [OverlayAppearance.glsl](../../../../../../Shaders/Rendering/Overlay/Shading/OverlayAppearance.glsl) | `MudValueNoise`, `ApplyMudMicroBump`, `ShadeWaterFilmLayer` | 물·진흙 demo appearance의 작은 패턴·normal bump 계산 |
| [OverlayCombined.frag](../../../../../../Shaders/Rendering/Overlay/Shading/OverlayCombined.frag) | combined material entry | 여러 enabled overlay State를 한 fragment path에서 shading |
| [OverlayMud.frag](../../../../../../Shaders/Rendering/Overlay/Shading/OverlayMud.frag) | mud entry/variant | Mud overlay shading entry; 공통 구현을 `OverlaySurface.glsl`에서 사용 |
| [OverlayLava.frag](../../../../../../Shaders/Rendering/Overlay/Shading/OverlayLava.frag) | lava entry/variant | Lava overlay shading entry; 공통 구현을 `OverlaySurface.glsl`에서 사용 |

`TexelGeometry.comp`, `TexelGeometryBaseline.comp`, `TexelGeometryHeight.comp`는 역할이 서로 다르다. C++에서 어느 파일을 dispatch하는지는 `TexelGeometryPreview.cpp`의 variant/pipeline 생성 및 `RendererCommands.cpp`의 pass 순서를 확인한다. 실제 compile variant 목록은 `CMake/ShaderManifest.cmake` (이번 ZIP에 미포함)에 있다.

## 실제 코드 경계

`[RendererCommands.cpp](../../../../../../Source/Rendering/Core/RendererCommands.cpp)`에서 active channel은 demo `Mud/WaterFilm/Lava` lookup으로 결정된다. Combined top surface 경로에서 fragment appearance와 coverage/geometry 준비는 다른 pass이므로 두 효과를 하나의 Solver 계산으로 설명하지 않는다. `OverlayCombined.frag`·`OverlayAppearance.glsl`는 표시의 소비자이고 source state producer가 아니다.
