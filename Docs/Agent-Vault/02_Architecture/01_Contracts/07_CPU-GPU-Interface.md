# CPU–GPU Interface Navigation

> 선언 원문을 복제하지 않는 **연결 지도**. 실제 Binding·Layout은 아래 소스 및 셰이더가 계약의 직접 근거다.

## Source of Truth
- CPU Descriptor·ABI: `Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h`, `Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h`, `Source/SurfaceState/GPU/Resources/`, `Source/SurfaceState/Solver/SurfaceStateSolver.cpp`.
- GLSL 리소스 선언: `Shaders/Simulation/Solver/SolverBindings.glsl`, `Shaders/Simulation/Solver/SurfaceSolverCommon.glsl` 및 각각의 `.comp/.vert/.frag`.
- 계층별 데이터 의미·패킹: [[02_Architecture/01_Contracts/05_GPU-Layout|GPU Layout]].
- GPU 수명·실행 시퀀스: [[02_Architecture/00_Flows/02_Simulation/00_Step-Overview|Simulation Steps]].

## 변경 시 반드시 동기화할 항목
1. `set`/`binding`, descriptor type, stage flags, array lengths 및 required features.
2. `std430`/`std140`, 정렬·패딩, push constant 오프셋·범위·stage flags.
3. Shader 읽기·쓰기와 CPU barrier / pipeline stage / queue transfer의 실제 대응.
4. 관련 셰이더와 CPU 양쪽 선언을 확인한 뒤 컴파일·Reflection·테스트 결과를 기록. 검증하지 않았다면 미검증으로 표시.

> 자동 이관 시 선언을 반복하지 않았고 이 파일의 항목별 ABI 검증은 수행하지 않았다.
