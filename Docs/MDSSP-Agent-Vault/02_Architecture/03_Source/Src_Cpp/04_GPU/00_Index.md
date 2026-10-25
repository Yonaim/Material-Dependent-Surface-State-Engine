# GPU C++ Source — Index

일반 Vulkan wrapper와 Surface 전용 GPU ABI·Resource lifetime을 혼동하지 않도록 분리한다. 공통 필드 의미는 [[02_Architecture/02_Contracts/04_GPU-Layout|GPU Layout 계약]]에서 확인한다.

| 질문 | 문서 |
|---|---|
| Vulkan device, swapchain, buffer/image, pass 생성 | [[02_Architecture/03_Source/Src_Cpp/04_GPU/01_Vulkan-Core|Vulkan Core]] |
| CPU→GPU packed record, alignment, descriptor binding | [[02_Architecture/03_Source/Src_Cpp/04_GPU/02_ABI-and-Packing|ABI & Packing]] |
| 공유 Geometry vs instance State, descriptor·retirement | [[02_Architecture/03_Source/Src_Cpp/04_GPU/03_Resources-and-Descriptors|Resources & Descriptors]] |

ABI 변경 시에는 C++ record, GLSL include, packer, descriptor, shader variant, fixture를 함께 검토한다.
