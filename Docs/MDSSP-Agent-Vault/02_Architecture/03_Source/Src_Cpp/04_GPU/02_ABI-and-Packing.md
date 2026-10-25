# Surface GPU ABI · CPU Packing

CPU record가 GLSL에서 읽는 byte layout과 정확히 대응하는지 확인하는 소스 지도다. Buffer offset·stride와 binding 불일치는 shader 단독 수정으로 해결되지 않는다.

**연결 문서**
- [[02_Architecture/02_Contracts/04_GPU-Layout|GPU Layout Contract]]
- [[02_Architecture/03_Source/Src_Shaders/04_Debug-and-Shared|Shader Shared Include]]

## Surface GPU ABI와 CPU packing

| 파일 | 선언·핵심 코드 | 역할 |
|---|---|---|
| [SurfaceGPUAbi.h](../../../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h) | `TSurfaceGPU*` 레코드 | CPU에서 upload할 정렬·크기·필드 순서 계약 |
| [SurfaceGPUDescriptorBinding.h](../../../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h) | descriptor binding enum | C++ descriptor writer와 GLSL binding index가 공유하는 binding 이름 |
| [SurfaceGPUResourceLayout.h](../../../../../../Source/SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h) | upload payload, sparse layout, pack 함수 선언 | GPU에 보낼 공유 Geometry/Profile과 instance metadata payload API |
| [SharedGeometryPacking.cpp](../../../../../../Source/SurfaceState/GPU/Packing/SharedGeometryPacking.cpp) | `PackSharedSurfaceGeometry` | CPU static texel geometry/neighbour를 ABI record 배열로 변환 |
| [ProfilePacking.cpp](../../../../../../Source/SurfaceState/GPU/Packing/ProfilePacking.cpp) | `PackSurfaceProfiles`, state value indexing | registry profile/channel 배열의 GPU layout 생성 |
| [SparseMetadataLayout.cpp](../../../../../../Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp) | `GetSurfaceSparseMetadataLayout` | texel·workgroup 수에 따른 sparse metadata buffer byte/word 구간 계산 |
| [TransferWeightPacking.cpp](../../../../../../Source/SurfaceState/GPU/Packing/TransferWeightPacking.cpp) | `BuildSurfaceGPUWorldTexelAreas`, `BuildSurfaceGPUTransferWeights` | transform된 texel 면적과 neighbor transfer weight를 instance upload용 배열로 계산 |

## 실제 ABI 크기·Descriptor 번호 확인

- [SurfaceGPUAbi.h](../../../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUAbi.h)의 `static_assert`: `TSurfaceGPUVec4 = 16B`, `TSurfaceGPUGeometryScalar = 16B`, `TSurfaceGPUNeighborIndices = 32B`, `TSurfaceGPUProfileParameters = 48B`, `TSurfaceSolverPushConstants = 128B`.
- [SurfaceGPUDescriptorBinding.h](../../../../../../Source/SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h)는 0부터 22까지 23개 storage buffer binding을 순서대로 선언한다. 예: `CurrentState = 8`, `NextState = 9`, `OutgoingFluxScale = 10`, `InputDelta = 11`, `TransferWeights = 14`, `RawOutgoing = 15`, `DynamicGeometry = 20`, `AccumulationHeights = 21`, `DynamicConcavityWeights = 22`.
- [DescriptorResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp): 같은 layout으로 AB/BA 두 descriptor set을 할당하며, Current/Next에서만 State A/B source buffer를 맞바꾼다. 나머지 shared/profile/instance 자원은 두 set에서 참조한다.
- [SparseMetadataLayout.cpp](../../../../../../Source/SurfaceState/GPU/Packing/SparseMetadataLayout.cpp)의 sparse metadata word/indirect command offset은 [SparseSchedule.glsl](../../../../../../Shaders/Simulation/Solver/SparseSchedule.glsl)의 인덱싱과 연동된다. 같은 단어를 쓴다고 ABI 일치가 보장되는 것은 아니므로 값 변경 시 양쪽을 검사한다.
- Shader쪽 저장 구조의 실제 정의는 [SurfaceTypes.glsl](../../../../../../Shaders/Shared/SurfaceState/SurfaceTypes.glsl) · [SurfaceStateData.glsl](../../../../../../Shaders/Shared/SurfaceState/SurfaceStateData.glsl)에서 확인한다.
