# Contract Register

> 상세 계약을 모아 찾는 인덱스다. 계약 본문은 각 하위 문서에 두며, 요구·테스트 링크는 탐색용으로만 연결한다.

| 계약 ID / 이름 | 필수 보장·경계 | 실제 코드 / 상세 문서 | 관련 요구·검증 |
|---|---|---|---|
| State & Profile | State는 texel별 총량이며 Capacity는 저장 상한이 아닌 포화 기준이다. Profile v5와 상태 registry의 데이터 의미를 확인한다. | [State & Profile](01_State-and-Profile.md), [Assets & Profiles](02_Assets-and-Profiles.md); `Source/SurfaceState/Types/`, `Shaders/Shared/SurfaceState/` | FR-002, FR-004; `Tests/SurfaceStateContractTests.cpp` |
| Geometry | Simulation UV graph, seam topology, 정적 공유 geometry와 instance 동적 geometry의 경계를 지킨다. | [Geometry](03_Geometry.md); `Source/SurfaceState/Mapping/`, `Source/SurfaceState/Geometry/`, `Shaders/Simulation/Geometry/` | FR-001, FR-003; `Tests/SurfaceMappingTests.cpp`, `Tests/SharedSurfaceGeometryTests.cpp` |
| Solver | Pass 순서, Flux/Gather/Reaction 실행 조건, State A/B와 step 간 barrier를 보존한다. | [Solver](04_Solver.md); `Source/SurfaceState/Solver/`, `Shaders/Simulation/Solver/` | FR-002; `Tests/SimulationTransportTests.cpp` |
| GPU Layout | C++ packing, descriptor/binding, push constants와 GLSL layout 및 자원 수명을 함께 맞춘다. | [GPU Layout](05_GPU-Layout.md); `Source/SurfaceState/GPU/`, `Shaders/Shared/SurfaceState/` | NFR-001; `Tests/SurfaceGPUResourceTests.cpp` |
| Reactions | Profile v5 `consumes`/`produces`, 공급량 제한과 Reaction Pass의 결과 규칙을 지킨다. | [Reactions](06_Reactions.md); `Source/SurfaceState/Types/SurfaceReactions.cpp`, `Shaders/Simulation/Solver/SurfaceReaction.comp` | FR-004; `Tests/SurfaceStateContractTests.cpp`, `Tests/SimulationTransportTests.cpp` |
| CPU–GPU Interface | CPU 자원 설정·dispatch·barrier와 GLSL 단계/리소스 사용의 연결을 확인한다. | [CPU–GPU Interface](07_CPU-GPU-Interface.md); `Source/SurfaceState/GPU/`, `Source/SurfaceState/Solver/`, `Shaders/` | NFR-001; 관련 GPU/solver 테스트 및 Validation 기록 |

## 우선 확인할 계약

- 입력·출력 타입, 데이터 단위·범위, 오류·예외와 반환 의미
- 상태 변화, 순서 보장, 소유권·수명 및 스레드/동기화 경계
- 직렬화 형식, 파일·네트워크 프로토콜, 공개 API/ABI 호환성
- **GLSL/CPU–GPU 경계(셰이더 사용 시):** Descriptor Set/Binding ↔ Host Descriptor 설정, SSBO/UBO의 `std140`/`std430` 등 레이아웃, Push Constant 범위·멤버 오프셋, Vertex Input/출력 Location, 패스 사이 리소스 사용과 Host 동기화

## 셰이더 계약의 정본과 문서 책임

- **GLSL 소스:** 실제 `layout` 선언·접근 제한·Workgroup 정보 및 선언만으로 보이지 않는 CPU 대응/데이터 의미 주석을 유지한다.
- **CPU 소스:** Descriptor Set Layout/Write, Pipeline Layout, Push Constant 전달 및 API 배리어 설정의 실제 구현을 유지한다.
- **Contract 문서:** 셰이더 단계·CPU 호출자·리소스 생성/갱신/소비 관계, 패스 간 전환·동기화, 링크·검증 결과를 **상위 수준에서** 연결한다. 실제 GLSL/C++ 선언을 통째로 중복하지 않는다.
- 셰이더를 사용하는 프로젝트에서 관계가 복잡해지면 [`CPU–GPU Contract 문서 템플릿`](../../00_Engineering-Standards/04_Docs_Templates/06_Docs_Template-CPU-GPU-Contract.md)을 복사하여 **실제 모듈/패스별** 계약 문서를 만든다. 사용하지 않는 프로젝트에는 빈 문서를 강제하지 않는다.
- 인터페이스 변경 시 GLSL·CPU·Contract·검증 기록을 **한 작업 안에서** 함께 확인한다. Reflection/Offset 검사와 API Validation을 사용할 수 있으면 실제 결과만 기록한다.

변경 시 호출자/소비자, Doxygen 주석, 관련 테스트를 함께 확인한다. 코드와 계약이 다르면 **의도된 보장과 관찰된 구현을 구분**해 기록한다.

계약의 성격별 하위 디렉터리와 인덱스 구성은 [Architecture Organization](../../00_Engineering-Standards/00_Core/02_Architecture-Organization.md)을 따른다.
