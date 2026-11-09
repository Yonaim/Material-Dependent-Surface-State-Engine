# Simulation Shader Source — Index

Compute entry와 include-only GLSL을 구별한다. 두-pass Solver / 전송·스케줄 / Geometry feedback의 세 단위로 원본 shader 목록을 분리했다.

| 파악할 것 | 문서 |
|---|---|
| Pass 1/2, InputDelta 소비, placeholder | [[02_Architecture/02_Source/01_Shaders/00_Simulation/01_Solver-Passes|Solver Passes]] |
| Transport math·Flux Cache·Sparse scheduling shared include | [[02_Architecture/02_Source/01_Shaders/00_Simulation/02_Transport-and-Scheduling|Transport & Scheduling]] |
| Height, dynamic normal, directional weights | [[02_Architecture/02_Source/01_Shaders/00_Simulation/03_Dynamic-Geometry|Dynamic Geometry]] |

Compile-time entry와 variants는 `CMake/ShaderManifest.cmake` 등 실제 빌드 manifest에서 확인해야 하지만 이번 첨부에는 없으므로 컴파일 variant 전체를 검증하지 못했다. `.glsl` include는 독립적인 SPIR-V entry가 아니다.
