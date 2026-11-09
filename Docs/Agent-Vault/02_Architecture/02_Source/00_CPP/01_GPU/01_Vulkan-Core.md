# Vulkan Core · Pass Wrappers

Surface 특화 계산 모델이 아니라 Vulkan 객체 생성·수명·Compute/Graphics pass abstraction을 담당한다.

**연결 문서**
- [[02_Architecture/02_Source/00_CPP/01_GPU/03_Resources-and-Descriptors|Resource Lifecycle]]

## Vulkan device와 명령 객체

| 파일 | 선언·핵심 코드 | 역할 |
|---|---|---|
| [VulkanInstance.h](../../../../../../Source/GPU/Vulkan/VulkanInstance.h) | `TVulkanInstance` | Vulkan instance와 validation layer 객체 선언 |
| [VulkanInstance.cpp](../../../../../../Source/GPU/Vulkan/VulkanInstance.cpp) | instance 생성, `ValidateLayers` | instance extension/layer 확인 후 VkInstance 소유 |
| [VulkanDevice.h](../../../../../../Source/GPU/Vulkan/VulkanDevice.h) | `TVulkanDevice` | physical/logical device 선택·접근 API |
| [VulkanDevice.cpp](../../../../../../Source/GPU/Vulkan/VulkanDevice.cpp) | physical device suitability·logical device 생성 | queue·feature·extension 조건으로 장치 선택 및 device 수명 관리 |
| [VulkanQueue.h](../../../../../../Source/GPU/Vulkan/VulkanQueue.h) | `TQueueFamilyIndices`, `TVulkanQueue` | queue handle 및 family index 형식 |
| [VulkanQueue.cpp](../../../../../../Source/GPU/Vulkan/VulkanQueue.cpp) | graphics/compute queue 선택·획득 | device가 사용할 command queue를 준비 |
| [VulkanContext.h](../../../../../../Source/GPU/Vulkan/VulkanContext.h) | `TVulkanContext` | instance/device/queue/swapchain 사용을 한 context에서 제공 |
| [VulkanContext.cpp](../../../../../../Source/GPU/Vulkan/VulkanContext.cpp) | context 구성·정리 | 창 표면과 Vulkan 객체 초기화 순서를 결합 |
| [VulkanCommand.h](../../../../../../Source/GPU/Vulkan/Command/VulkanCommand.h) | `TVulkanCommand` | command pool/buffer 할당·제출·single-time API |
| [VulkanCommand.cpp](../../../../../../Source/GPU/Vulkan/Command/VulkanCommand.cpp) | command buffer begin/end/submit | CPU 작업을 Vulkan queue에 제출하는 공통 유틸리티 |
| [Swapchain.h](../../../../../../Source/GPU/Vulkan/Swapchain/Swapchain.h) | `TSwapchain` | swapchain image·format·extent lifecycle API |
| [Swapchain.cpp](../../../../../../Source/GPU/Vulkan/Swapchain/Swapchain.cpp) | create/recreate/acquire/present | window surface 크기·지원 format에 맞춰 표시 image 구성 |

## Buffer·image·graphics pass

| 파일 | 선언·핵심 코드 | 역할 |
|---|---|---|
| [GPUBuffer.h](../../../../../../Source/GPU/Vulkan/Resource/GPUBuffer.h) | `TGPUBuffer`, Upload/Download | Vulkan buffer/memory 소유 및 CPU↔GPU data 전송 API |
| [GPUBuffer.cpp](../../../../../../Source/GPU/Vulkan/Resource/GPUBuffer.cpp) | buffer/memory allocate·map | 요청 usage·memory property에 맞춘 buffer 생성과 수명 관리 |
| [GPUImage.h](../../../../../../Source/GPU/Vulkan/Resource/GPUImage.h) | `TGPUImage` | VkImage와 device memory wrapper |
| [GPUImage.cpp](../../../../../../Source/GPU/Vulkan/Resource/GPUImage.cpp) | image create/recreate/reset | format·extent·usage에 맞춰 image를 생성·교체 |
| [GPUImageView.h](../../../../../../Source/GPU/Vulkan/Resource/GPUImageView.h) | `TGPUImageView` | image view wrapper |
| [GPUImageView.cpp](../../../../../../Source/GPU/Vulkan/Resource/GPUImageView.cpp) | view create/recreate/reset | image의 format·subresource range를 shader/render pass에 연결 |
| [GPUSampler.h](../../../../../../Source/GPU/Vulkan/Resource/GPUSampler.h) | `TGPUSampler` | texture sampling state wrapper |
| [GPUSampler.cpp](../../../../../../Source/GPU/Vulkan/Resource/GPUSampler.cpp) | sampler 생성·destroy | filter/address mode를 Vulkan sampler로 구성 |
| [GPUResourceMemoryStats.h](../../../../../../Source/GPU/Vulkan/Resource/GPUResourceMemoryStats.h) | memory category·snapshot·`TGPUResourceMemoryStats` | 앱이 추적하는 buffer/image allocation 및 peak 값 집계 |
| [GraphicsPipeline.h](../../../../../../Source/GPU/Vulkan/Pipeline/GraphicsPipeline.h) | shader stage/pipeline config, `TGraphicsPipeline` | graphics pipeline 입력 형식과 lifecycle API |
| [GraphicsPipeline.cpp](../../../../../../Source/GPU/Vulkan/Pipeline/GraphicsPipeline.cpp) | SPIR-V 읽기·VkPipeline 생성 | shader stages·vertex input·render pass 정보를 graphics pipeline으로 변환 |
| [RenderPass.h](../../../../../../Source/GPU/Vulkan/Render/RenderPass.h) | `TRenderPass` | attachment와 subpass lifecycle API |
| [RenderPass.cpp](../../../../../../Source/GPU/Vulkan/Render/RenderPass.cpp) | render pass 생성 | color/depth attachment 동작을 Vulkan render pass로 지정 |
| [Framebuffer.h](../../../../../../Source/GPU/Vulkan/Render/Framebuffer.h) | `TFramebuffer` | render pass에 연결된 image view 집합 API |
| [Framebuffer.cpp](../../../../../../Source/GPU/Vulkan/Render/Framebuffer.cpp) | framebuffer create/recreate/reset | swapchain image와 render pass attachment를 묶음 |
| [ComputePass.h](../../../../../../Source/GPU/Pass/ComputePass.h) | `TComputePass` | compute shader·descriptor·pipeline을 한 pass로 제공 |
| [ComputePass.cpp](../../../../../../Source/GPU/Pass/ComputePass.cpp) | compute pipeline 생성·dispatch 바인딩 | Simulation/Rendering compute 작업의 공통 pass wrapper |
| [GraphicsPass.h](../../../../../../Source/GPU/Pass/GraphicsPass.h) | `TGraphicsPass::Begin/End` | render pass begin/end API |
| [GraphicsPass.cpp](../../../../../../Source/GPU/Pass/GraphicsPass.cpp) | viewport/scissor·render pass 기록 | graphics command buffer의 공통 pass 시작·종료 처리 |
