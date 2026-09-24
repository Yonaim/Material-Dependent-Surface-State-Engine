# 모듈과 책임

상태: **설계** · 근거: [[05_Assets/Documents/0001_Overall-Engine-Structure.pdf|전체 엔진 구조]]

| 구성요소 | 책임 |
|---|---|
| `VulkanContext` | Instance / Device, Queue / Command, GPU Resource 기반 관리 |
| `Renderer` | Swapchain, RenderPass / Pipeline, Framebuffer / RenderContext |
| `Scene` | Camera와 `StaticMeshInstance[]` 관리 |
| `StaticMeshInstance` | Transform, `MeshAssetHandle`, `SurfaceStateHandle` 보유 |
| `AssetManager` | Mesh, Texture, Material, SurfaceResponseProfile Asset 관리 |
| `InputSystem` | Mouse / Keyboard, Raycaster 및 Contact Input 생성 |
| `DebugUI` | 상태·형상·Solver 결과 디버깅 |
| `SurfaceStateSystem` | 공유 형상 데이터, 인스턴스 상태, 입력, Solver, 형상 갱신, DebugData 관리 |

`SurfaceStateSystem`의 내부 구성은 다음과 같다.

```text
SurfaceStateSystem
├── SharedSurfaceGeometryData[]
├── SurfaceInstanceStateData[]
├── SurfaceInput
├── SurfaceStateSolver
├── SurfaceGeometryUpdate
└── DebugData
```

실제 C++ 클래스 분할과 Vulkan 리소스 소유권은 구현 단계에서 조정할 수 있다. 이 문서는 **논리적 책임**을 정의한다.
