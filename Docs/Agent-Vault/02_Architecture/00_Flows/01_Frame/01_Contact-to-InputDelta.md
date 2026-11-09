# Contact → InputDelta

Raycast/Scene contact 요청을 Solver가 읽는 Texel 입력량으로 변환한다. Contact 제출 시점과 실제 소비 시점을 구별한다.

**연결 문서**
- [[02_Architecture/02_Source/00_CPP/00_SurfaceState/03_Contact-and-State|Contact Source]]
- [[02_Architecture/00_Flows/02_Simulation/03_Gather-Pass2|Pass 2]]

## 적용 시점과 소비

Contact 제출은 State를 즉시 바꾸지 않는다. 첫 실행 step의 [`ApplyPendingContacts`](../../../../../Source/SurfaceState/Contact/SurfaceContactApplication.cpp)가 대상 instance·State·도형을 검사하고 영향을 받는 texel의 `InputDelta` 및 입력 workgroup metadata를 업로드한다. Pass 2가 delta를 소비한다. Scene initial contact는 restart에서 다시 queue에 들어간다.

## 코드의 연계 지점

`TInputSystem::PollDebugContact` → `TSurfaceStateSystem::SubmitContact` → `ApplyPendingContacts` → `InputDelta` → 첫 실행 Pass 2. `InputDelta`는 Contact 제출 즉시 Current State를 바꾸지 않는다.

## 실제 접촉 매핑과 실패 조건

`SurfaceContactApplication.cpp`는 입력의 instance/State/position/strength/shape를 검증하고, `bHasSimulationMapping`에서 hit triangle과 SimulationUV를 활용한다. 중앙 texel이 동일 triangle 소속이 아니면 지정된 검색 범위에서 대체 texel을 찾고, 실패한 contact는 경고 후 거절한다. World-space shape/plane, Texel profile의 State 지원 여부·InputFactor를 적용한 뒤 instance별 InputDelta와 sparse input group/channel metadata를 업로드한다. Pass 2가 InputDelta를 읽어 0으로 초기화하므로 반복 주입을 피한다.
