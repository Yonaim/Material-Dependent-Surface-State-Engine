# Graphics Draw / Debug

마지막 compute→draw 읽기 경계부터 base mesh, 공통 overlay, debug UI와 검사 지점까지의 순서다.

**연결 문서**
- [[02_Architecture/00_Flows/03_Rendering/00_Render-Overview|Render Overview]]
- [[02_Architecture/02_Source/00_CPP/06_Rendering|C++ Rendering]]
- [[02_Architecture/02_Source/01_Shaders/01_Overlay/02_Overlay-Shading|Overlay Shading]]

## 실행 조건과 산출물

| 단계 | 입력 → 출력 | 실행 조건·담당 코드 |
|---|---|---|
| Raster | 원본 mesh와 준비된 overlay buffer → swapchain image | `RendererCommands.cpp`가 material, 현재 A/B State, geometry descriptor를 묶어 section별 draw |

## Draw와 확인 지점

원본 mesh는 base surface로 material descriptor를 바인딩해 그린다. Surface State가 있는 instance는 현재 A/B State descriptor와 render texture descriptor를 추가로 묶는다. Lit overlay는 공통 top mesh를 `vkCmdDrawIndexedIndirect`, side를 `vkCmdDrawIndirect`로 그린다. Heat의 base tint와 Mud/WaterFilm/Lava의 공통 top shading은 서로 다른 shader entry를 거친다. 필요한 buffer가 없는 instance는 overlay 준비와 draw를 건너뛴다.

geometry debug view에서 선택 채널만 보여 주는 Accumulation preview와 모든 지원 State를 합산하는 `TotalSimulationHeight`는 입력 범위가 다르다. 화면 색으로 State 보존이나 정확성을 판단할 때는 Inspector의 실제 State·Capacity·world area와 [[05_Experiments/01_Validation-and-Tests|검증 기록]]을 같이 확인한다. 렌더링 경로를 바꿀 때는 `RendererCommands.cpp`의 compute→draw 순서, `TexelGeometryPreview.cpp` cache 갱신 조건, 현재 A/B descriptor 및 shader binding을 함께 따라간다.
