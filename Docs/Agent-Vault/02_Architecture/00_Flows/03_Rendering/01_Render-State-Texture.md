# Render State Texture / Inspector

현재 State A/B와 Registry의 demo channel을 화면 표시에 적합한 Texture·Snapshot으로 변환하는 경로다. Solver의 State 저장량 자체는 바꾸지 않는다.

**연결 문서**
- [[02_Architecture/00_Flows/03_Rendering/02_Texel-Overlay-Preparation|Overlay]]
- [[02_Architecture/02_Source/01_Shaders/02_Surface-Rendering|Surface Shader]]
- [[02_Architecture/02_Source/01_Shaders/04_Debug-and-Shared|Debug Shader]]

## 실행 조건과 산출물

| 단계 | 입력 → 출력 | 실행 조건·담당 코드 |
|---|---|---|
| Inspector | 선택 instance/texel/channel의 현재 buffer → snapshot | 선택값과 State ID가 유효할 때 `TexelInspector.cpp`, `TexelInspector.comp` |
| State texture | 현재 State·Profile → demo channel의 RGBA16F image array | Lit view와 demo effect가 있을 때 `RenderStateTexture.cpp`, `RenderStateTexture.comp` |

## State → Texture

[`TRenderStateTexture::Record`](../../../../../Source/Rendering/Surface/RenderStateTexture.cpp)는 Registry에서 Heat·Mud·WaterFilm·Lava 채널을 찾아 buffer State를 image로 변환한다. `RenderStateTextureSmoothing.comp`는 설정에 따라 chart/region 경계를 고려한 표시용 texture를 만든다. `StateSampling.glsl`과 `RenderStateSampling.glsl`은 UV와 seam에서 이 texture의 sampling을 해석한다. 직접 buffer sampling과 texture sampling은 렌더링 경로의 선택이며 저장된 State 양을 바꾸지 않는다.

## 확인

Texel Inspector는 선택한 texel의 실제 State·Capacity·world area를 조회한다. 렌더 heatmap의 [0,1] 범위 제한과 시뮬레이션 saturation을 혼동하지 않는다.
