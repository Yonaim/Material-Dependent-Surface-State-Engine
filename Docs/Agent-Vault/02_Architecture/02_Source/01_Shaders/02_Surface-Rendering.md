# Surface Rendering Shaders

이 파일들은 Solver state/geometry를 화면에 sampling·shading한다. 표시용 filtering과 material effect는 GPU simulation buffer로 기록되지 않는다. 실제 pass 기록은 [Rendering C++](../00_CPP/06_Rendering.md), 계산 값은 [[02_Architecture/01_Contracts/04_Solver|Solver Contract]]를 참고한다.

## State texture와 sampling

| 파일 | 주요 코드 | 입력→출력/연결 |
|---|---|---|
| [StateSampling.glsl](../../../../../Shaders/Rendering/Surface/StateTexture/StateSampling.glsl) | `FindSurfaceSamplingAnchor`, `SampleStateSaturation(s)` | UV와 valid chart texel을 연결하고 State/Profile로부터 saturation을 보간; state texture 변환과 surface fragment가 공유 |
| [RenderStateTexture.comp](../../../../../Shaders/Rendering/Surface/StateTexture/RenderStateTexture.comp) | `main`, `image2DArray RenderStates` | State buffer의 선택 channel을 RGBA16F 2D array image로 변환 |
| [RenderStateTextureSmoothing.comp](../../../../../Shaders/Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp) | `SmoothingMode`, `SameRegion`, `main` | raw state texture를 chart 경계에 맞춰 direct 또는 separable mode로 smoothing |
| [RenderStateSampling.glsl](../../../../../Shaders/Rendering/Surface/StateTexture/RenderStateSampling.glsl) | `SampleRenderStates`, boundary flags | 준비된 render texture를 UV에서 샘플하고 seam/chart 경계에서 이웃 State를 보정 |
| [HeightFieldSmoothing.comp](../../../../../Shaders/Rendering/Surface/Smoothing/HeightFieldSmoothing.comp) | `ResolveCandidateTile`, `ResolveTexel`, `main` | computed overlay height/normal buffer의 선택 tile을 화면용으로 smoothing; CPU는 variant·tile list를 공급 |

## Surface material lighting

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [SurfaceLit.vert](../../../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.vert) | vertex `main` | source mesh 위치·normal·tangent·UV를 world/view 공간으로 넘김 |
| [SurfaceLit.frag](../../../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.frag) | manifest fragment entry | `SurfaceLit.glsl` 공통 표면 shading을 일반 surface mesh에 적용 |
| [BaseSurfaceLit.frag](../../../../../Shaders/Rendering/Surface/Lighting/BaseSurfaceLit.frag) | manifest fragment entry | accumulation overlay와 분리된 base surface 표시 |
| [TexelSurfaceLit.vert](../../../../../Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.vert) | `TexelMeshVertex.glsl` vertex path | texel mesh sample/position을 읽어 Simulation texel 단위 render surface 생성 |
| [TexelSurfaceLit.frag](../../../../../Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.frag) | manifest fragment entry | texel mesh fragment에 공통 Surface State material shading 적용 |
| [SurfaceLit.glsl](../../../../../Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl) | shared fragment `main` | base color·normal·State sample을 조합해 표면 lighting과 state effect 적용 |
| [MaterialParameters.glsl](../../../../../Shaders/Rendering/Surface/Lighting/MaterialParameters.glsl) | material uniform block | renderer/debug material option과 texture/render mode를 fragment stages에 공유 |
| [Lighting.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Lighting.glsl) | specular lobe·surface lighting helpers | material normal/roughness에 따른 공통 diffuse/specular 계산 |
| [Effects/AbsorbedWetness.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl) | wetness stain coverage | wetness State를 흡수/얼룩 appearance로 변환 |
| [Effects/Heat.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Effects/Heat.glsl) | heat tint/emission term | Heat channel의 표시 강도를 계산 |
| [Effects/Lava.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Effects/Lava.glsl) | `LavaVisualAmount`, `LavaColor`, `LavaEmission` | Lava coverage를 색 ramp와 발광량으로 변환 |
| [Effects/Mud.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Effects/Mud.glsl) | mud material blend | Mud coverage에 따라 base material을 demo mud appearance로 혼합 |
| [Effects/WaterFilm.glsl](../../../../../Shaders/Rendering/Surface/Lighting/Effects/WaterFilm.glsl) | `VisibleWaterCoverage` | 연속 WaterFilm State를 물방울 형태의 화면 coverage로 변환 |

## Scene helper shaders

| 파일 | 주요 코드 | 역할 |
|---|---|---|
| [Scene/StaticMesh.vert](../../../../../Shaders/Rendering/Scene/StaticMesh.vert) | vertex `main` | scene mesh 위치·normal·tangent를 raster pipeline에 전달 |
| [Scene/StaticMesh.frag](../../../../../Shaders/Rendering/Scene/StaticMesh.frag) | material sampling·lighting | Surface State overlay가 없는 scene mesh를 재질 조명으로 표시 |
| [Scene/Gizmo.vert](../../../../../Shaders/Rendering/Scene/Gizmo.vert) | gizmo transform/color output | axis gizmo 정점을 clip space로 옮기고 선택 축을 강조 |
| [Scene/Gizmo.frag](../../../../../Shaders/Rendering/Scene/Gizmo.frag) | color output | gizmo vertex color를 fragment color로 출력 |
| [Scene/WorldReference.vert](../../../../../Shaders/Rendering/Scene/WorldReference.vert) | grid/axis vertex output | world reference grid·axis의 위치, color, edge coordinate 전달 |
| [Scene/WorldReference.frag](../../../../../Shaders/Rendering/Scene/WorldReference.frag) | edge fade | grid 선분 바깥을 fade하고 axis 색/alpha 출력 |

실제 entry point와 compile-time material variants는 현재 저장소의 `CMake/ShaderManifest.cmake`에서 확인한다. Demo effect의 시각 효과를 범용 simulation material law로 해석하지 않는다.

## C++ 렌더 옵션에 따른 실제 분기

[FRenderer::RecordCommandBuffer](../../../../../Source/Rendering/Core/RendererCommands.cpp)는 Lit viewport가 있고 demo effects가 켜져 있을 때만 `RenderStateTexture->Record`를 호출한다. Texture 경로 사용 여부는 `bRenderStateTextureSamplingEnabled || bPrecomputeCoverage`이며 `bPrecomputeCoverage`는 coverage smoothing 및 precompute 옵션이 모두 켜져야 한다. 이 분기는 shader 구조가 아니라 C++ record 경계에서 결정된다.

[FRenderStateTexture::Record](../../../../../Source/Rendering/Surface/RenderStateTexture.cpp)는 State buffer를 표시용 이미지로 변환한다. [FHeightFieldSmoothing::Record](../../../../../Source/Rendering/Surface/HeightFieldSmoothing.cpp)는 geometry 표시용 smoothing이고 Solver의 State·DynamicGeometry buffer를 교체하지 않는다.

## Shader 진입점의 읽기 순서

- `StaticMesh.frag`의 `main()`은 `ReconstructMaterialNormal` → `EvaluateStaticMeshColor` 순서로 노멀 복원과 조명을 구분한다.
- `HeightFieldSmoothing.comp`의 `main()`은 공유 Halo 구성과 필수 `barrier()`를 먼저 수행한 뒤 `SmoothResolvedTexel`에 texel별 유효성·보간·결과 기록을 위임한다. **공유 메모리 barrier는 함수 추출 과정에서 순서를 바꾸면 안 된다.**
- `TexelMeshVertex.glsl`은 `SampleDeformedMeshNormal`에서 높이·노멀을 보간하고 `PublishTexelMeshVertex`가 world/clip-space 출력을 기록한다.
- `OverlayTop.vert`·`OverlaySide.vert`의 `main()`은 segment/vertex 데이터를 판단한 뒤 vertex 출력을 작성한다.

Region 중첩은 기능별 탐색을 위한 것이며 `#region` 번호 자체가 추상화 레벨을 뜻하지 않는다.
