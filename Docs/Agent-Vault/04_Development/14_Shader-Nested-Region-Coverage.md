# Shader 중첩 Region — 전수 검토 (2026-11-10)

## 범위와 판단 기준

이번 작업 시작 시 디스크의 GLSL shader 69개와 Git 추적 shader 69개가 일치함을 확인했다. 각 파일의 기존 상위 구획, 실제 함수/선언, 진입점 처리 단계와 전처리 분기·동기화 경계를 검토했다. 최초 v5.8 이관의 선택적 4개 shader 적용에서 빠진 후보를 포함한다.

[Local Numbering](../00_Engineering-Standards/00_Core/01_Local-Numbering-and-Navigation.md)의 세 가지 판단을 적용했다: 같은 상위 구획 안의 서로 다른 하위 책임이 둘 이상인지, 독립적으로 접을 탐색 가치가 있는지, 함수/타입 추출보다 구획이 적합한지. 길이나 함수 수만으로 자동 생성하지 않았다. 기존 계산·함수 배치·프로토타입·GPU 인터페이스는 그대로 두고 제목과 마커만 추가했다.

- **35개 파일에 115개 하위 Region 추가**, 기존 중첩 4개 파일 유지, 나머지 30개는 아래에 미추가 이유 기록.
- 최종 **69개 파일 / 전체 Region 248개 / 하위 Region 126개 / 중첩 파일 39개**, 최대 깊이 2.
- 최상위 3줄 제목·지역 번호는 유지한다. 하위는 `00.00` 같은 한 줄 제목과 `00_00_...` 식별자이며 둘러싼 코드의 들여쓰기를 따른다.
- `SurfaceDebugData.glsl`은 진단 record / 적층·표시 높이 / normal 복원, Debug vertex는 normal / position 변위, Debug fragment는 입력·mode 경계 / 진입점 단계 / 진단 종류별 helper로 구분했다.
- Overlay·State sampling·Geometry·Solver의 큰 helper 섹션도 실제 책임으로 묶었다. 함수마다 무조건 Region을 추가하거나 기존 하위 Region 안에 3단계를 만들지 않았다.
- shared halo staging 전후의 `barrier()`와 schedule reset의 두 barrier는 진입점에서 구획 사이에 남겼다. barrier를 내부에 가진 기존 helper를 묶은 경우에는 `Synchronize`를 제목에 명시했다. 새 구획이 함수 블록·조건부 컴파일의 한쪽 경계를 가로지르지 않는지 검사했다.

## 검증

- 시작 시점 snapshot과 최종 69개 파일의 **비주석 토큰 완전 동일**. 선언·식·분기·barrier 호출 및 순서, 문자열, include·macro·layout·binding·push constant 변경 없음. 기존 LF/CRLF 형식 유지.
- 모든 Region의 짝·부모 포함·상위 번호 연결·하위 연속 번호·한 줄 제목·들여쓰기·깊이 검사 통과. 새 구획은 원래 같은 brace scope·같은 전처리 분기에서 시작하고 끝난다.
- Debug/Release entry·variant 150개를 변경 전후 glslc로 컴파일하고 spirv-val 검사: 모두 통과, SPIR-V 150개 바이트 동일.
- 실제 설치된 Maptz 1.0.22 RegionProvider로 69개 파일을 LF/CRLF 각각 실행: **138회 통과**, 양쪽 모두 248개 영역과 default 지정 일치. root/vault 설정의 동일성도 확인. 실제 편집기의 접힘 UI를 관찰한 검증은 아니다.
- C++·계산 코드 변경이 없어 전체 C++ build/CTest 및 별도 GPU 성능·렌더 캡처는 반복하지 않았다. 직전 v5.8 검증 결과와 이번 검증 범위를 구별한다.
- [로컬 원자료](../../../Build/Validation/20261110-shader-regions/summary.md): `before/`, `inventory.json`, `changes.json`, `coverage.json`, `errors.json`, shader 전후 결과와 `provider-results.json`.

## 파일별 판단

| 파일 | 판단 | 추가 하위 수 | 최종 하위 수 | 책임/유지 사유 |
|---|---|---:|---:|---|
| `Shaders/Debug/SurfaceDebug.frag` | 추가 | 10 | 10 | Fragment Inputs and Material Controls / Diagnostic Buffers and View Mode Contract / Dispatch UV Grid and Area Views / Resolve Geometry and Simulation Eligibility / Dispatch Solver Data Views / Screen Footprint and Basic View Helpers / Flux and Surface Geometry Views / Topology and Transfer Views / Accumulation Height Views / State Heatmap Evaluation |
| `Shaders/Debug/SurfaceDebug.vert` | 추가 | 2 | 2 | Evaluate Meso and Final Normals / Apply Preview Position Displacement |
| `Shaders/Debug/SurfaceDebugData.glsl` | 추가 | 3 | 3 | Accumulation Diagnostic Record / Evaluate Accumulation and Display Height / Reconstruct Final Surface Normal |
| `Shaders/Debug/TexelInspector.comp` | 추가 | 2 | 2 | Capture Accumulation and Normal Diagnostics / Capture Reaction Integration Diagnostics |
| `Shaders/Debug/Views/SurfaceDebugColors.glsl` | 추가 | 2 | 2 | Grid and Area Visualization / State Palettes and Relief Lighting |
| `Shaders/Rendering/Overlay/Coverage/OverlayCoverageSmoothing.comp` | 추가 | 2 | 2 | Resolve Coverage Tiles and Synchronize Shared Halo / Filter and Publish Vertex Coverage |
| `Shaders/Rendering/Overlay/Coverage/OverlayOccupancyScan.comp` | 추가 | 6 | 6 | Resolve Candidate Tiles and Scan Changes / Evaluate State and Coverage Change Signals / Publish Dirty Tile Schedules / Coverage Scheduling and Dispatch Layout / Append Height and Normal Candidates / Advance Rebuilt State Baseline |
| `Shaders/Rendering/Overlay/Coverage/OverlaySideCoverage.comp` | 추가 | 3 | 3 | Mesh Topology and Draw Record Types / Coverage and Indirect Draw Outputs / Computed Geometry Inputs |
| `Shaders/Rendering/Overlay/Coverage/OverlayStateSampling.glsl` | 추가 | 2 | 2 | Profile Bindings and Capacity Access / State Mapping and Saturation Sampling |
| `Shaders/Rendering/Overlay/Geometry/OverlayDrawReset.comp` | 추가하지 않음 | 0 | 0 | 하나의 indirect draw-count 초기화 작업이며 각 command 분기는 짧다. |
| `Shaders/Rendering/Overlay/Geometry/OverlaySide.vert` | 추가 | 2 | 2 | Resolve Segment and Reject Invalid Geometry / Construct World Frame and Emit Side Vertex |
| `Shaders/Rendering/Overlay/Geometry/OverlaySideBoundary.comp` | 추가 | 4 | 4 | Resolve Coverage Cutoff and Compact Top Triangles / Emit Coverage Boundary Segments / Interpolate Geometry and Write Segment Records / Reconstruct and Smooth Neighboring Sides |
| `Shaders/Rendering/Overlay/Geometry/OverlayTop.vert` | 추가 | 2 | 2 | Interpolate Displacement and Surface Normal / Publish World Space Top Vertex |
| `Shaders/Rendering/Overlay/Geometry/OverlayTopCommand.comp` | 추가하지 않음 | 0 | 0 | 단일 top draw command 생성 커널로, 진입점은 짧은 count 게시 작업이다. |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometry.comp` | 추가 | 5 | 5 | Preserve Inactive Tile Baseline / Evaluate and Publish Active Height and Normal / Resolve and Synchronize Sparse Texels / Evaluate Accumulation and Cached Display Height / Reconstruct Seam Aware Surface Normals |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometry.vert` | 추가하지 않음 | 0 | 0 | TexelMeshVertex 구현을 선택하는 include wrapper다. |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryBaseline.comp` | 추가 | 2 | 2 | Accumulate Neighbor Normal Equations / Solve Tangent Plane Normal |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryData.glsl` | 추가하지 않음 | 0 | 0 | 하나의 vec4 GPU record 정의만 담는다. |
| `Shaders/Rendering/Overlay/Geometry/TexelGeometryHeight.comp` | 추가 | 2 | 2 | Locate Sparse Height Tiles / Synchronize and Resolve Height Invocations |
| `Shaders/Rendering/Overlay/Geometry/TexelMeshVertex.glsl` | 추가 | 2 | 2 | Interpolate Texel Height and Normal / Transform and Publish Surface Vertex |
| `Shaders/Rendering/Overlay/Shading/OverlayAppearance.glsl` | 추가 | 2 | 2 | Procedural Mud Noise and Micro Bump / Water Film Reflection and Transparency |
| `Shaders/Rendering/Overlay/Shading/OverlayCombined.frag` | 추가하지 않음 | 0 | 0 | compile-time macro와 공통 fragment include를 선택하는 wrapper다. |
| `Shaders/Rendering/Overlay/Shading/OverlayLava.frag` | 추가하지 않음 | 0 | 0 | Lava macro와 공통 fragment include를 선택하는 wrapper다. |
| `Shaders/Rendering/Overlay/Shading/OverlayMud.frag` | 추가하지 않음 | 0 | 0 | 공통 overlay fragment include를 선택하는 wrapper다. |
| `Shaders/Rendering/Overlay/Shading/OverlaySurface.glsl` | 추가 | 5 | 5 | Evaluate Combined Material States / Resolve Combined Overlay Normal / Composite Accumulated Material Layers / Filter Pixel Space Overlay Normals / Sample Seam Partner States and Visibility |
| `Shaders/Rendering/Scene/Gizmo.frag` | 추가하지 않음 | 0 | 0 | 입력 색을 출력하는 단일 문장이다. |
| `Shaders/Rendering/Scene/Gizmo.vert` | 추가하지 않음 | 0 | 0 | 짧은 hover 강조·정점 변환 경로이며 각 부분을 독립 접을 이점이 작다. |
| `Shaders/Rendering/Scene/StaticMesh.frag` | 추가 | 2 | 2 | Reconstruct Material Normal and Albedo / Dispatch Diagnostic Modes and Surface Lighting |
| `Shaders/Rendering/Scene/StaticMesh.vert` | 추가하지 않음 | 0 | 0 | 하나의 vertex 변환/attribute 전달 경로로, 계산 본문이 짧다. |
| `Shaders/Rendering/Scene/WorldReference.frag` | 추가하지 않음 | 0 | 0 | grid edge fade와 axis fallback을 표현하는 짧은 단일 외관 작업이다. |
| `Shaders/Rendering/Scene/WorldReference.vert` | 추가하지 않음 | 0 | 0 | 3개 attribute/position 대입으로 끝나는 짧은 vertex 전달 경로다. |
| `Shaders/Rendering/Surface/Lighting/BaseSurfaceLit.frag` | 추가하지 않음 | 0 | 0 | base-surface macro를 지정하는 공통 fragment wrapper다. |
| `Shaders/Rendering/Surface/Lighting/Effects/AbsorbedWetness.glsl` | 추가하지 않음 | 0 | 0 | 하나의 짧은 stain 색/roughness 계산이다. |
| `Shaders/Rendering/Surface/Lighting/Effects/BurnScorched.glsl` | 추가하지 않음 | 0 | 0 | burn/scorched의 짧은 색·roughness helper 두 개로 이미 탐색 부담이 작다. |
| `Shaders/Rendering/Surface/Lighting/Effects/Heat.glsl` | 추가하지 않음 | 0 | 0 | 하나의 짧은 heat 색상 보간 helper다. |
| `Shaders/Rendering/Surface/Lighting/Effects/Lava.glsl` | 추가하지 않음 | 0 | 0 | 하나의 Lava 외관을 구성하는 짧은 amount/color/emission helper 묶음이다. |
| `Shaders/Rendering/Surface/Lighting/Effects/Mud.glsl` | 추가하지 않음 | 0 | 0 | 하나의 짧은 색상·roughness 보간 helper다. |
| `Shaders/Rendering/Surface/Lighting/Effects/WaterFilm.glsl` | 추가하지 않음 | 0 | 0 | coverage와 외관 helper가 짧으며 기존 함수 경계와 빈 줄로 구분된다. |
| `Shaders/Rendering/Surface/Lighting/Lighting.glsl` | 기존 중첩 유지 | 0 | 3 | 기존 하위 책임 구획·깊이·번호·컴파일 경계를 재확인했다. |
| `Shaders/Rendering/Surface/Lighting/MaterialParameters.glsl` | 추가하지 않음 | 0 | 0 | 하나의 고정 material ABI block과 짧은 color-ramp accessor다. 필드별 접기를 강제하지 않는다. |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.frag` | 추가하지 않음 | 0 | 0 | 공통 SurfaceLit fragment include를 선택하는 wrapper다. |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.glsl` | 기존 중첩 유지 | 0 | 3 | 기존 하위 책임 구획·깊이·번호·컴파일 경계를 재확인했다. |
| `Shaders/Rendering/Surface/Lighting/SurfaceLit.vert` | 추가하지 않음 | 0 | 0 | 짧은 normal/tangent 및 world/clip 좌표 전달 작업이다. |
| `Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.frag` | 추가하지 않음 | 0 | 0 | TEXEL_LIT macro를 지정하는 공통 fragment wrapper다. |
| `Shaders/Rendering/Surface/Lighting/TexelSurfaceLit.vert` | 추가하지 않음 | 0 | 0 | 공통 texel vertex include를 선택하는 wrapper다. |
| `Shaders/Rendering/Surface/Smoothing/HeightFieldSmoothing.comp` | 추가 | 5 | 5 | Stage Shared Height Halo / Accumulate Same Region Neighborhood / Recompose and Publish Smoothed Geometry / Resolve and Synchronize Sparse Tiles / Evaluate Profile Cavity Fill |
| `Shaders/Rendering/Surface/StateTexture/RenderStateSampling.glsl` | 추가 | 2 | 2 | Resolve Sampling Anchor and Interior Fast Path / Filter Chart and Profile Boundary Samples |
| `Shaders/Rendering/Surface/StateTexture/RenderStateTexture.comp` | 추가하지 않음 | 0 | 0 | State를 RGBA texture 한 texel로 변환하는 단일 커널이며 guard와 한 출력으로 읽힌다. |
| `Shaders/Rendering/Surface/StateTexture/RenderStateTextureSmoothing.comp` | 추가 | 4 | 4 | Stage Shared State Halo / Filter and Publish Shared Halo Samples / Filter Direct Five By Five Kernel / Filter and Publish Separable Axis |
| `Shaders/Rendering/Surface/StateTexture/StateSampling.glsl` | 추가 | 4 | 4 | Resolve Valid Surface Sampling Anchor / Read and Interpolate Scalar State / Smooth Scalar State Within Surface Boundaries / Read and Interpolate Multiple State Channels |
| `Shaders/Shared/SurfaceState/SurfaceStateData.glsl` | 추가 | 3 | 3 | Texel Mapping and Static Geometry / Profile Tables and SoA Access / Current State and Display Geometry Inputs |
| `Shaders/Shared/SurfaceState/SurfaceTypes.glsl` | 추가하지 않음 | 0 | 0 | 서로 짧은 고정 GPU ABI record 두 개다. 구조체 내부나 타입마다 접기를 강제하지 않는다. |
| `Shaders/Simulation/Geometry/SurfaceAccumulationHeight.comp` | 추가 | 2 | 2 | Publish Height and Handle Geometry Update Modes / Detect Dirty Geometry and Schedule Dependencies |
| `Shaders/Simulation/Geometry/SurfaceDynamicGeometryUpdate.comp` | 추가 | 2 | 2 | Evaluate and Sanitize Displaced Geometry / Measure World Space Neighbor Distance |
| `Shaders/Simulation/Geometry/SurfaceDynamicWeightsUpdate.comp` | 추가 | 3 | 3 | Read Dynamic Local and World Geometry / Evaluate Dynamic Concavity Weight / Evaluate Directional Transfer Weight |
| `Shaders/Simulation/Input/SurfaceInput.comp` | 추가하지 않음 | 0 | 0 | 실행 코드가 없는 placeholder다. |
| `Shaders/Simulation/Scheduling/SurfaceSparseScheduleReset.comp` | 추가 | 6 | 6 | Reset Destination Schedules and Snapshot Inputs / Merge CPU Contact Activity / Solver Schedule Address Layout / CPU Input Activation Layout / Accumulation and Geometry Schedule Layout / Reset Commands and Append Current Workgroups |
| `Shaders/Simulation/Solver/FluxCache.glsl` | 추가 | 2 | 2 | Address and Read Packed Flux Cache / Validate Solver Numeric Values |
| `Shaders/Simulation/Solver/GeometryDrive.glsl` | 추가 | 2 | 2 | Prepare Shared Source Geometry Context / Evaluate Neighbor Geometry Drive |
| `Shaders/Simulation/Solver/ReactionBindings.glsl` | 추가하지 않음 | 0 | 0 | Reaction descriptor 두 개와 매우 짧은 flag/address accessor로 구성된다. |
| `Shaders/Simulation/Solver/SolverBindings.glsl` | 추가 | 5 | 5 | Static Texel Topology and Geometry / Profile Tables and SoA Access / Current Next State and Flux Storage / Dynamic Geometry and Accumulation Storage / Step Parameters and Execution Mode Flags |
| `Shaders/Simulation/Solver/SparseSchedule.glsl` | 추가 | 5 | 5 | Persistent Solver Schedule Layout / Input Accumulation and Geometry Layout / Resolve Active Workgroups and Channels / Publish Next Solver Activity / Publish Accumulation Workgroups |
| `Shaders/Simulation/Solver/StateFinalize.glsl` | 추가하지 않음 | 0 | 0 | accumulation contribution 변경 여부를 판정하는 단일 predicate다. |
| `Shaders/Simulation/Solver/StateMath.glsl` | 추가 | 4 | 4 | Texel Topology and Transfer Weight Access / Evaluate Accumulated Geometry Height / Evaluate Effective Position Normal and Concavity / Evaluate Profile Capacity Saturation and Decay |
| `Shaders/Simulation/Solver/SurfaceReaction.comp` | 기존 중첩 유지 | 0 | 3 | 기존 하위 책임 구획·깊이·번호·컴파일 경계를 재확인했다. |
| `Shaders/Simulation/Solver/SurfaceSolverCommon.glsl` | 추가하지 않음 | 0 | 0 | 의존 include만 모은 진입 wrapper다. |
| `Shaders/Simulation/Solver/SurfaceSolverPass1.comp` | 추가 | 4 | 4 | Resolve Source Texel and Shared Profile Inputs / Evaluate and Publish Channel Outgoing Flux / Accumulate Source Outgoing Flux / Evaluate Directional Neighbor Flux |
| `Shaders/Simulation/Solver/SurfaceSolverPass2.comp` | 기존 중첩 유지 | 0 | 2 | 기존 하위 책임 구획·깊이·번호·컴파일 경계를 재확인했다. |
| `Shaders/Simulation/Solver/Transport.glsl` | 추가 | 2 | 2 | Evaluate Prepared Transport and Cavity Retention / Prepare Required Neighbor Transport Inputs |
