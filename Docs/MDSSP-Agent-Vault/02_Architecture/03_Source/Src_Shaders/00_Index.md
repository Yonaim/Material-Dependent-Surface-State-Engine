# Shader Source Index

이 문서는 shader 파일을 **entry point와 include dependency**로 나눠 찾는다. entry point 목록과 compile variant의 기준은 `CMake/ShaderManifest.cmake` (이번 ZIP에 미포함)다. CMake 등록 `.comp/.vert/.frag`는 SPIR-V를 만들며, `.glsl`은 다른 shader에서 include한다.

| 문서 | 범위 | 주요 코드 경로 |
|---|---|---|
| [[02_Architecture/03_Source/Src_Shaders/01_Simulation/00_Index|Simulation]] | state transport, dynamic geometry, sparse scheduling | `Shaders/Simulation/` |
| [[02_Architecture/03_Source/Src_Shaders/02_Surface-Rendering|Surface Rendering]] | state texture, material lighting, scene mesh rendering | `Shaders/Rendering/Surface/`, `Rendering/Scene/` |
| [[02_Architecture/03_Source/Src_Shaders/03_Overlay/00_Index|Overlay]] | texel geometry·coverage·top/side mesh·overlay shading | `Shaders/Rendering/Overlay/` |
| [[02_Architecture/03_Source/Src_Shaders/04_Debug-and-Shared|Debug and Shared Data]] | debug visualization, shared CPU/GPU structures, shader-only data helpers | `Shaders/Debug/`, `Shaders/Shared/` |

## CPU 연결점

- Simulation pass dispatch와 barriers: [Solver C++](../Src_Cpp/05_Solver.md), [Simulation pipeline](../../01_Flows/00_Index.md).
- Rendering/overlay dispatch와 draw: [Rendering C++](../Src_Cpp/06_Rendering.md), [Rendering pipeline](../../01_Flows/04_Rendering/00_Render-Overview.md).
- CPU↔GLSL record layout: [GPU Resources](../Src_Cpp/04_GPU/00_Index.md), [GPU Layout contract](../../02_Contracts/04_GPU-Layout.md).

각 하위 페이지는 파일별 목적, 입력/출력, 다음 소비자를 설명한다. variant macro와 SPIR-V 등록 변경은 manifest 및 관련 C++ pipeline 생성 코드와 함께 확인한다.


## 세부 탐색

- [[02_Architecture/03_Source/Src_Shaders/01_Simulation/00_Index|Simulation Compute Source]]
- [[02_Architecture/03_Source/Src_Shaders/03_Overlay/00_Index|Overlay Shader Source]]
- [[02_Architecture/03_Source/00_Index|CPU↔Shader 연결·증상별 탐색]]

## C++ pipeline variant 대조

[TSurfaceStateSolver 생성자](../../../../../Source/SurfaceState/Solver/SurfaceStateSolver.cpp)에는 Solver Pass1·Pass2 각각 11개 GPU pipeline variant(5 raw modes × 2 weight modes + 1 runtime flag mode)와 DynamicWeights 3개 variant, SparseScheduleReset/AccumulationHeight/DynamicGeometry pipeline 생성이 명시되어 있다. [SurfaceInput.comp](../../../../../Shaders/Simulation/Input/SurfaceInput.comp)는 이 생성자에서 pipeline이 구성되지 않는다.

**범위 제한:** 원본 `Shaders.zip`에는 CMake shader manifest와 compiler 설정이 없다. 실제 SPIR-V 배포 목록, include path, specialization build flags까지 확인했다고 기재하지 않는다.
