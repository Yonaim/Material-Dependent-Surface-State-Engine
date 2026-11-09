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

## C++ 타입 이름과 직렬화 호환성

2026-11-09 Unreal-Inspired 전환으로 CPU 일반 타입은 `F`, enum은 `E`를 사용한다. GLSL 타입명은 해당 셰이더 프로필의 기존 이름을 유지한다. 예를 들어 CPU `FSurfaceGPUProfileParameters` ↔ GLSL `FSurfaceGPUProfileParameters`는 byte layout으로 대응하며 이름이 같아야 하는 계약이 아니다. 바인딩 번호·필드 순서·alignment/stride·push constant 범위는 변경하지 않았다. C++ public 타입명이 바뀌므로 소비자는 새 헤더로 다시 빌드한다.

Scene 직렬화의 `"type": "TScene"`, 기존 JSON 키·로그/예외 문자열·파일 경로는 호환성을 위해 유지한다. C++ 클래스는 `FScene`이다. `std::array<bool, ...>` 같은 컨테이너는 scalar bool 변수가 아니므로 `LogLevelFilters`, `Enabled` 같은 기존 컬렉션 이름을 유지한다.
