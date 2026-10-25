# GPU Resource Lifetime · Descriptor

Shared Geometry·Profile Table·Instance State를 각각 누가 소유하며 언제 재할당·retire하는지 다룬다.

**연결 문서**
- [[02_Architecture/03_Source/Src_Cpp/04_GPU/02_ABI-and-Packing|ABI/Packing]]
- [[02_Architecture/01_Flows/02_Frame/00_Frame-and-Step-Scheduling|Frame Flow]]

## Surface descriptor와 resource lifecycle

| 파일 | 선언·핵심 코드 | 역할 |
|---|---|---|
| [SurfaceGPUResources.h](../../../../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResources.h) | Surface GPU handle·resource 공통 형식 | Runtime surface와 GPU resource 사이의 ID/handle 표현 |
| [SurfaceGPUBufferPool.h](../../../../../../Source/SurfaceState/GPU/Resources/SurfaceGPUBufferPool.h) | `TSurfaceGPUBufferPool::Release/Clear` | 교체된 buffer를 보관·재사용해 잦은 재할당을 줄이는 pool |
| [ResourceBufferHelpers.h](../../../../../../Source/SurfaceState/GPU/Resources/ResourceBufferHelpers.h) | buffer build/upload helpers | surface resource builder의 반복 upload·allocation 절차 공통화 |
| [DescriptorResources.h](../../../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.h) | `TSurfaceStateDescriptorResources` | solver·geometry·render용 descriptor set/layout 수명 선언 |
| [DescriptorResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp) | descriptor layout/pool/set 생성·update | ABI binding에 맞춰 GPU buffer/image를 shader descriptor에 연결 |
| [InstanceResources.h](../../../../../../Source/SurfaceState/GPU/Resources/InstanceResources.h) | `TSurfaceInstanceGPUResources`, scratch format | instance별 State A/B, InputDelta, scratch, 동적 geometry buffer 상태 |
| [InstanceResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/InstanceResources.cpp) | `ConfigureScratchStorage`, area/weight update, retirement | instance 자원 할당·용량 변경·입력 업로드·buffer retirement |
| [ProfileResources.h](../../../../../../Source/SurfaceState/GPU/Resources/ProfileResources.h) | `TSurfaceProfileGPUResources` | Scene Registry에 대응하는 GPU profile table 소유 |
| [ProfileResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/ProfileResources.cpp) | profile buffer packing/upload | Registry 변경분을 GPU table에 반영하고 이전 buffer를 retire |
| [SharedGeometryResources.h](../../../../../../Source/SurfaceState/GPU/Resources/SharedGeometryResources.h) | `TSurfaceSharedGeometryGPUResources`, mesh variants | 공유 static Geometry·texel mesh·resolution variant 소유 |
| [SharedGeometryResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/SharedGeometryResources.cpp) | geometry/mesh upload·variant trim | 동일 Runtime Geometry의 GPU buffer와 필요 시점에 만든 render mesh cache 관리 |
| [SurfaceGPUResourceManager.h](../../../../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.h) | `TSurfaceGPUResourceManager` | Scene·surface·instance 자원을 조합하는 상위 manager API |
| [SurfaceGPUResourceManager.cpp](../../../../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp) | ensure/update/release, scratch mode, model transform updates | static geometry 공유와 instance resources 분리, descriptor 재구성 및 GPU memory accounting 조정 |

ABI를 수정하면 `SurfaceGPUAbi.h`, `SurfaceTypes.glsl`/`SurfaceStateData.glsl`, descriptor 생성, packer, GPU fixture를 함께 따라간다. 대조할 테스트 경로 후보는 `Tests/SurfaceGPUResourceTests.cpp`, `Tests/SurfaceStateContractTests.cpp`다(이번 ZIP 미포함).

## 공유 자원과 instance 자원의 경계

`TSurfaceGPUResourceManager`는 Scene instance index에서 runtime surface를 찾는다. 동일 runtime surface의 static mapping·geometry와 texel render mesh variant는 `TSurfaceSharedGeometryGPUResources`가 공유한다. `TSurfaceInstanceGPUResources`는 State A/B, `InputDelta`, RawOutgoing/scale, world area, transfer weight, accumulation height, dynamic geometry와 scratch를 instance마다 소유한다. `TSurfaceProfileGPUResources`는 Scene Registry의 profile table을 GPU에 올린다. `DescriptorResources.cpp`가 이 세 범주의 buffer를 solver·geometry·render descriptor binding으로 연결한다.

| 바뀌는 원인 | 재계산·교체 경로 | 실행 전 확인 |
|---|---|---|
| Scene이나 resolution 변경 | `SurfaceGPUResourceManager.cpp`의 공유 geometry/instance ensure·release | texel 수, variant cache, descriptor 재구성 |
| instance transform 변경 | `NeedsTransferWeightCacheUpdate` → `UpdateTransferWeightCache` | world area와 정적 transfer weight의 재업로드 |
| profile parameter 수정 | `UpdateProfileParameters` → `ProfileResources.cpp` | registry index와 table buffer의 일치 |
| FP16 scratch·weight 설정 | `ConfigureScratchStorage` → `InstanceResources.cpp` | buffer 크기·packing과 shader variant 일치 |
| A/B step 종료 | `AdvanceCurrentState` | 다음 step과 renderer가 새 current descriptor를 선택 |

`SharedGeometryPacking.cpp`는 CPU texel/neighbor topology를, `ProfilePacking.cpp`는 profile/channel을 ABI record로 바꾼다. `TransferWeightPacking.cpp`는 instance transform이 반영된 world area·neighbor weight를 만든다. `SparseMetadataLayout.cpp`의 word offset은 shader의 schedule·indirect command 주소와 같아야 한다. C++ 구조체에 필드를 추가할 때는 byte size뿐 아니라 field 순서, 정렬, descriptor binding, shader에서 읽는 stride를 함께 확인한다.

`Source/GPU/`의 Vulkan wrapper는 Surface 계산의 의미를 정의하지 않는다. `TGPUBuffer`와 `TGPUImage`가 메모리 수명을, `TComputePass`와 pipeline wrapper가 shader/binding 생성을, `TRenderContext`가 frame fence를 담당한다. Surface 자원 교체 시에는 이전 command buffer가 아직 buffer를 읽고 있을 수 있으므로 `InstanceResources.cpp`/`ProfileResources.cpp`의 retirement 및 descriptor 갱신 순서를 확인한다. 데이터 소비 순서는 [[02_Architecture/01_Flows/00_Index|Simulation]]과 [[02_Architecture/01_Flows/04_Rendering/00_Render-Overview|Rendering]]에 있다.

## 자원 교체·캐시 무효화 체크리스트 (코드 검증)

| 변경 원인 | CPU에서 실제 검사할 함수 | 부작용/확인할 것 |
|---|---|---|
| Instance Transform 변경 | `TSurfaceGPUResourceManager::NeedsTransferWeightCacheUpdate` → `UpdateTransferWeightCache` | CPU packed world area/transfer weight 및 dynamic geometry force-full 조건. `TSurfaceStateSystem::RecordStep`은 갱신 전 `vkQueueWaitIdle` 호출 가능 |
| FP16 Weight / Flux Cache 구성 변경 | `TSurfaceGPUResourceManager::ConfigureScratchStorage` | 새 scratch 용량·packing 및 선택한 Pass pipeline variant 동일성 |
| Profile 파라미터 변경 | `TSurfaceGPUResourceManager::UpdateProfileParameters` | GPU ProfileParameters 갱신, 관련 geometry/weight 재빌드 플래그 |
| Solver step 완료 | `TSurfaceGPUResourceManager::AdvanceCurrentState` | AB/BA 선택이 다음 step과 Renderer의 현재 상태 읽기에 일치해야 함 |
| Scene/해상도 변경 | resource manager 재구성·descriptor 교체 | static geometry 공유 key와 Instance index가 기존보다 달라질 수 있음 |

근거: [SurfaceGPUResourceManager.cpp](../../../../../../Source/SurfaceState/GPU/Resources/SurfaceGPUResourceManager.cpp) · [SurfaceStepRecording.cpp](../../../../../../Source/SurfaceState/Solver/SurfaceStepRecording.cpp) · [DescriptorResources.cpp](../../../../../../Source/SurfaceState/GPU/Resources/DescriptorResources.cpp). 리소스 수명은 GPU fence 및 retirement 완료 여부에 종속된다. 소스의 정적 읽기만으로 실제 GPU race 부재를 보장하지는 않는다.
