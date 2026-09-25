# 구현 구조 개요

상태: **현재 C++ 코드 기준** · 상위 아키텍처: [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]]

클래스와 구조체의 관계를 실제 데이터 흐름에 따라 정리한다. 상세 관계는 파일 파싱부터 Surface Instance State 생성까지 하나의 문서에서 이어서 설명한다.

## 상세 문서

| 문서 | 범위 |
|---|---|---|
| [[0001_Asset-and-Surface-Data-Flow|Asset과 Surface 데이터 흐름]] | 원본·설정·Runtime 생성 데이터의 역할과 수명, OBJ/MTL/Texture/SRProfile 파싱, Mapping, Shared Geometry, Instance State 관계 |

## 관계 표기

| 관계 | 의미 |
|---|---|
| 상속 | 파생 클래스가 기반 클래스의 인터페이스·공통 상태를 이어받음 |
| 소유 | 멤버 객체 또는 컨테이너가 대상 객체의 수명을 관리 |
| 공유 소유 | `std::shared_ptr`로 여러 소유 주체가 수명을 공동 관리 |
| 비소유 참조 | 참조·포인터·handle로 접근하지만 대상 수명을 소유하지 않음 |
| 호출/변환 | 함수 호출이나 값 변환으로 데이터를 전달. 소유 관계를 뜻하지 않음 |
| 헤더 의존 | 컴파일을 위해 타입 선언을 include. 런타임 포함·소유와 다름 |

## 모듈 위치

| 모듈 | 주요 역할 | 코드 위치 |
|---|---|---|
| `Application` | 창과 하위 시스템 수명, 메인 루프 조정 | `Source/Application/` |
| `AssetManager` | 에셋 로드 조정, 등록 및 handle 조회 | `Source/AssetManager/` |
| `DebugUI` | ImGui 기반 설정·로그 UI | `Source/DebugUI/` |
| `InputSystem` | 입력 이벤트와 Raycast. 현재 placeholder | `Source/InputSystem/` |
| `Logger` | 로그 기록과 history 조회 | `Source/Logger/` |
| `Renderer` | Swapchain 기반 장면 렌더링 | `Source/Renderer/` |
| `Scene` | Camera와 정적 Mesh Instance 자료형 | `Source/Scene/` |
| `SurfaceStateSystem` | Mapping 및 Surface State 자료형. Solver/System은 placeholder | `Source/SurfaceStateSystem/` |
| `VulkanContext` | Vulkan instance/device/queue/command와 GPU 자원 기반 | `Source/VulkanContext/` |

모듈별 책임은 [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]]을 기준으로 한다. 개별 관계 문서는 해당 기능에서 실제로 연결되는 모듈을 함께 설명한다.

## 갱신 기준

| 변경 | 갱신 대상 |
|---|---|
| 모듈 책임 또는 큰 데이터 단계 변경 | [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]] |
| Parser, Asset, Mapping, Shared Geometry, Instance State 관계 변경 | [[0001_Asset-and-Surface-Data-Flow|Asset과 Surface 데이터 흐름]] |
