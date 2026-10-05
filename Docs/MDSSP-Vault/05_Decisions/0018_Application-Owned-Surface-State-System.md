# Decision 0018 — Application 소유 Surface State System

> **한 줄 요약:** Application이 Surface State 시스템을 소유하고 Renderer는 필요한 작업만 요청한다.

- 분류: **Architecture**
- Status: **Accepted. 구현 반영.**
- Date: 2026-09-30
- 관련 문서: [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]], [[05_Decisions/0007_Scene-State-Registry-and-Shared-Profile-Table|Decision 0007 — Scene별 State Registry와 공유 Profile GPU 테이블]]

## 쉽게 읽기

Application이 Surface State 시스템의 생성·수명을 맡고 Renderer는 참조만 보유한다. 렌더링과 시뮬레이션 실행은 함께 기록되지만 소유권은 분리한다.

## Context — 왜 필요했나

초기 구현에서는 `TRenderer`가 `TSurfaceStateSystem`을 생성하고 소유했다. 이 때문에 Contact 입력이 Renderer의 전달 메서드를 거쳐야 했고, Renderer의 생성·파괴 수명과 Surface State 및 Solver GPU 자원의 수명이 결합됐다.

`TSurfaceStateSystem`은 표면 상태, 접촉 입력, Solver와 Scene별 GPU 자원을 관리하는 시뮬레이션 시스템이다. Renderer는 같은 Vulkan command buffer에 Solver dispatch를 기록하고 결과 buffer를 렌더링에 사용하지만, 이 실행 의존성이 시뮬레이션 시스템의 소유권까지 요구하지는 않는다.

## Decision — 무엇을 정했나

- `TApplication`이 `TSurfaceStateSystem`을 소유한다. 선언 순서로 `TAssetManager`와 `TScene` 뒤, `TRenderer` 앞에 두어 의존 대상보다 먼저 파괴되지 않게 한다.
- `TRenderer`는 생성자에서 `TSurfaceStateSystem&`를 받고 비소유 참조로 유지한다.
- `TApplication`은 Debug contact를 `TSurfaceStateSystem::SubmitContact`에 직접 전달한다.
- Renderer는 프레임 command buffer에 `TSurfaceStateSystem::RecordStep`을 호출하고 현재 GPU State resource를 읽는다.
- Scene 또는 Simulation 해상도 교체에서는 Application이 소유하는 `TSurfaceStateSystem` 객체의 주소와 수명을 유지한다. 새 Scene 자원과 pipeline을 먼저 준비한 뒤 성공 시 `ReplaceSceneResources`로 내부 Scene별 GPU 자원과 Solver를 교체한다.
- 현재 Scene reload UI와 렌더 pipeline 재생성이 결합되어 있으므로 교체 트랜잭션의 조정은 Renderer에 남긴다. 이는 Surface State System의 소유권을 Renderer에 돌려주지 않는다.

## Alternatives Considered — 다른 방법

- **Renderer가 Surface State System 소유:** 초기 구현이다. command buffer와 pipeline 접근은 단순하지만 시뮬레이션 수명과 입력 경로가 렌더링에 종속된다.
- **Application이 두 시스템을 각각 소유:** 채택한다. 수명과 책임이 드러나며 Renderer는 실행에 필요한 참조만 사용한다.

## Consequences — 결정의 영향

- Renderer를 교체하거나 렌더링 책임을 분리해도 Surface State System의 소유 위치는 바뀌지 않는다.
- Contact 입력이 Renderer API를 경유하지 않는다.
- Renderer보다 Surface State System이 먼저 생성되고 나중에 파괴되어야 한다.
- Scene 자원 교체 중 Renderer가 보관한 참조가 무효화되지 않도록 시스템 객체 자체를 교체하지 않고 내부 자원만 교체한다.
- Solver가 Vulkan command buffer에 기록되는 현재 실행 방식은 유지한다.

## Related — 관련 문서

- [[03_Architecture/0000_Overview|전체 엔진 구조]]
- [[03_Architecture/0001_Engine-Structure|엔진 구조와 데이터 흐름]]
- [[03_Architecture/0010_System-Flows|Asset과 Surface 데이터 흐름]]
- [[03_Architecture/0010_System-Flows|Contact Input 흐름]]
- [[05_Decisions/0007_Scene-State-Registry-and-Shared-Profile-Table|Decision 0007 — Scene별 State Registry와 공유 Profile GPU 테이블]]
