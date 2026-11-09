# Shader Source Index

이 문서는 shader 파일을 **entry point와 include dependency**로 나눠 찾는다. entry point 목록과 compile variant의 기준은 [ShaderManifest.cmake](../../../../../CMake/ShaderManifest.cmake)다. CMake 등록 `.comp/.vert/.frag`는 SPIR-V를 만들며, `.glsl`은 다른 shader에서 include한다.

| 문서 | 범위 | 주요 코드 경로 |
|---|---|---|
| [[02_Architecture/02_Source/01_Shaders/00_Simulation/00_Index|Simulation]] | state transport, dynamic geometry, sparse scheduling | `Shaders/Simulation/` |
| [[02_Architecture/02_Source/01_Shaders/02_Surface-Rendering|Surface Rendering]] | state texture, material lighting, scene mesh rendering | `Shaders/Rendering/Surface/`, `Rendering/Scene/` |
| [[02_Architecture/02_Source/01_Shaders/01_Overlay/00_Index|Overlay]] | texel geometry·coverage·top/side mesh·overlay shading | `Shaders/Rendering/Overlay/` |
| [[02_Architecture/02_Source/01_Shaders/04_Debug-and-Shared|Debug and Shared Data]] | debug visualization, shared CPU/GPU structures, shader-only data helpers | `Shaders/Debug/`, `Shaders/Shared/` |

## CPU 연결점

- Simulation pass dispatch와 barriers: [Solver C++](../00_CPP/05_Solver.md), [Simulation pipeline](../../00_Flows/00_Main-Flow.md).
- Rendering/overlay dispatch와 draw: [Rendering C++](../00_CPP/06_Rendering.md), [Rendering pipeline](../../00_Flows/03_Rendering/00_Render-Overview.md).
- CPU↔GLSL record layout: [GPU Resources](../00_CPP/01_GPU/00_Index.md), [GPU Layout contract](../../01_Contracts/05_GPU-Layout.md).

각 하위 페이지는 파일별 목적, 입력/출력, 다음 소비자를 설명한다. variant macro와 SPIR-V 등록 변경은 manifest 및 관련 C++ pipeline 생성 코드와 함께 확인한다.


## 세부 탐색

- [[02_Architecture/02_Source/01_Shaders/00_Simulation/00_Index|Simulation Compute Source]]
- [[02_Architecture/02_Source/01_Shaders/01_Overlay/00_Index|Overlay Shader Source]]
- [[02_Architecture/02_Source/00_Source-Map|CPU↔Shader 연결·증상별 탐색]]

## C++ pipeline variant 대조

[FSurfaceStateSolver 생성자](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)에는 Solver Pass1·Pass2 각각 11개 GPU pipeline variant(5 raw modes × 2 weight modes + 1 runtime flag mode), Reaction pipeline 하나와 DynamicWeights 3개 variant, SparseScheduleReset/AccumulationHeight/DynamicGeometry pipeline 생성이 명시되어 있다. [SurfaceInput.comp](../../../../../Shaders/Simulation/Input/SurfaceInput.comp)는 이 생성자에서 pipeline이 구성되지 않는다.

실제 SPIR-V variant 생성 범위는 [ShaderManifest.cmake](../../../../../CMake/ShaderManifest.cmake)와 CMake 빌드 설정을 함께 확인한다.


## 소스 탐색과 Region folding

GLSL의 번호가 있는 기존 섹션에는 `// #region 00_Resource_Interface` / `// #endregion` 형식의 주석 마커를 사용한다. 형식의 정본은 [Local Numbering](../../../00_Engineering-Standards/00_Core/01_Local-Numbering-and-Navigation.md)과 [GLSL 규칙](../../../00_Engineering-Standards/01_Languages/02_GLSL.md)이다. 저장소 루트 또는 볼트의 VS Code 설정에서 glsl 언어 모드와 Custom Folding 공급자를 사용한다. 사용자 지정 MDSS 정책에 따라 작은 include wrapper/helper/placeholder에도 마커를 적용한다. 현재 69개 파일·122개 영역이며, GLSL 언어 프로필의 일반적인 작은 파일 예외보다 Project Brief의 설정을 우선한다. CRLF 대응 정규식과 기본 접기 지정은 볼트 README를 따른다. 접기 마커 변경만으로 계산·바인딩·pass/barrier 순서가 바뀌지 않는다.

## 5.5 전체 적용 대조

나머지 StaticMesh/HeightFieldSmoothing/RenderStateTextureSmoothing/SparseScheduleReset/AccumulationHeight/DynamicWeights entry도 helper prototype → main → Detailed Implementation으로 배치했다. 기존 shared include와 단일 pass의 함수/자원 경계는 유지한다. 모든 69개를 검토했으며 layout/CPU 대응·수식·compile-time variant·derivative/barrier 참여는 변경하지 않았다.
