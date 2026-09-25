# Week-01 — 전체 구조 설계

상태: **계획** · 상위 계획: [[02_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]

## 목표

Vulkan 렌더링 엔진과 Surface State System의 책임·연결 관계를 정하고, 첫 구현 범위를 Static Mesh로 한정한다.

## 작업

- VulkanContext, Renderer, Scene, AssetManager, InputSystem, DebugUI의 역할과 관계를 정리한다.
- SurfaceStateSystem이 geometry, instance state, input, solver, geometry update를 어떻게 연결하는지 정의한다.
- 프로젝트의 대상 Mesh와 Material, Static Mesh 범위를 확인한다.
- 전체 데이터 흐름과 주요 외부 입력 경로를 도식화한다.

## 산출물

- 엔진 모듈 구조도와 Surface State System 구성도.
- 프로젝트 범위 및 구현 순서를 설명하는 1주차 기록.

## 참고

- [[03_Architecture/0000_Overview|전체 엔진 구조]]
- [[03_Architecture/0001_Engine-Structure|엔진 모듈과 데이터 흐름]]
- [[07_Assets/Documents/0001_Overall-Engine-Structure.pdf|전체 엔진 구조 원본]]
