# Week-03 — 기본 렌더링 엔진 구현

상태: **계획** · 상위 계획: [[02_Planning/0000_Project-Plan|Project Plan]], [[02_Planning/0001_Roadmap|Roadmap]]

## 목표

Vulkan 기반 실행 경로에서 Mesh와 Material을 로드하고 Static Mesh를 렌더링한다.

## 작업

- Vulkan instance/device/queue 및 swapchain을 구성한다.
- 기본 render pass 또는 현재 Renderer 구조에 맞는 pipeline과 framebuffer를 구성한다.
- OBJ/MTL, Texture, Scene을 읽는 Asset 경계를 연결한다.
- Static Mesh를 Scene에 배치하고 기본 Material로 렌더링한다.
- SRProfile Asset 연결을 위한 파일·자료형 경계를 마련한다.

## 산출물

- 빌드·실행 가능한 최소 렌더링 엔진.
- Static Mesh와 Material 로딩·표시 확인 및 캡처.

## 범위 메모

아직 Surface State GPU Solver를 완성하는 단계가 아니다. 안정적인 렌더링·Asset 기반을 먼저 확보한다.

## 참고

- [[03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]
- [[06_Assets/Documents/0003_Asset-Structure.pdf|Asset 구조 원본]]
