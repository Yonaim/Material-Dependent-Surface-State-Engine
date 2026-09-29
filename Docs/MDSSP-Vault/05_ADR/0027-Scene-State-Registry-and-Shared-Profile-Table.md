# ADR 0027 — Scene별 State Registry와 공유 Profile GPU 테이블

> **한 줄 요약:** Scene별 State Registry와 Profile GPU 테이블의 구성 및 공유 범위를 정한다.

- 분류: **Architecture**
- 상태: **Accepted. 구현 및 CTest 검증 완료.**
- 날짜: 2026-09-29
- 관련 문서: [[0006-Dynamic-State-Registry|ADR 0006 — Dynamic State Registry]], [[0009-Texel-Profile-Index-Map|ADR 0009 — Texel Profile Index Map]], [[../04_Architecture/0008_Surface-GPU-Data-Layout|Surface GPU Data Layout]]

## Context

초기 구현은 AssetManager에 캐시된 모든 `.SRProfile`로 Registry를 지연 생성했다. Scene을 교체해도 이전 Profile의 State가 선택 목록과 instance State 배열에 남았으며, Registry가 재구성될 때 이전 숫자 State ID로 저장한 선택·튜닝 값의 의미가 달라질 수 있었다.

Profile GPU 테이블도 Runtime Surface Data handle별로 생성했다. 서로 다른 Mesh 또는 Map이 같은 `.SRProfile`을 참조하면 동일한 반응 파라미터가 여러 GPU 테이블에 복제됐다.

현재 범위는 Scene 전환이다. 실행 중 같은 Scene에 State 종류를 추가하는 기능과 Registry ID의 영구 보존은 요구하지 않는다.

## Decision

- 현재 Scene instance의 Runtime Surface Profile 테이블에 참조된 고유 `.SRProfile`만으로 Registry를 구성한다. State 종류는 해당 Profile의 `states` key에서 수집하며 canonical 이름 정렬에 따른 숫자 `TStateId`를 사용한다.
- Mesh·Profile 자산 캐시는 유지한다. 새 자산을 로드하는 동안 활성 Registry는 변경하지 않는다.
- Renderer는 Scene 교체 시 새 Registry를 설치한 뒤 GPU 자원을 준비한다. 준비가 실패하면 이전 Registry를 복원하고 이전 GPU 자원을 유지한다. UI도 이전 Scene으로 복원한다.
- Scene 교체 성공 시 Inject와 Heatmap의 선택을 채널 0으로 초기화한다. Registry가 비어 있으면 UI 선택은 `InvalidStateId`다. 이전 State ID에 연결된 Profile Tuning draft·override와 Solver step/reset 요청을 비운다.
- 동일 Scene의 Simulation 해상도 변경은 Profile 집합과 ID를 유지하므로 현재 선택·튜닝 값을 유지하고 State만 초기화한다.
- Profile GPU 테이블은 Scene GPU resource manager당 하나로 소유한다. 고유 Profile handle을 한 번씩 넣고 모든 simulated instance가 같은 Parameters·Supported buffer를 참조한다. Profile tuning은 이 테이블의 해당 record를 한 번 갱신한다.
- CPU Geometry와 `.Surface` 캐시의 Profile index는 Runtime-local 테이블 순서를 유지한다. GPU Geometry 업로드 시 Runtime-local index를 Scene Profile index로 변환한다. `InvalidSurfaceProfileIndex`는 그대로 보존한다. Shader의 Profile-major 조회 산식과 descriptor binding ABI는 유지한다.
- Geometry는 Runtime Surface Data handle별로 공유하고 동적 State·Solver 임시 버퍼·TransferWeight cache는 instance별로 소유한다.

## Alternatives Considered

- **모든 캐시 Profile로 Registry 생성:** 초기 구현이다. Scene과 무관한 State가 남으며 State ID에 연결된 UI·튜닝 값이 다음 Registry에 재사용될 수 있다.
- **Runtime Surface Data별 Profile GPU 테이블:** 초기 구현이다. 로컬 Profile index를 직접 사용할 수 있지만 다른 Runtime 조합 사이에서 같은 Profile 파라미터를 중복 저장한다.
- **Scene별 Registry와 단일 Profile GPU 테이블:** 채택한다. GPU 업로드에서 index 변환이 필요하지만 현재 Scene의 State 집합과 Profile 공유 범위를 명확하게 유지한다.

## Consequences

- State ID는 Registry 내부에서만 유효하며 Scene 전환 시 새로 배정될 수 있다. 이전 ID를 새 Scene의 선택·튜닝 값에 재사용하지 않는다.
- 동일 Profile을 다른 Mesh·Map에서 사용해도 GPU 파라미터와 지원 여부는 한 번 저장한다. instance의 State 값은 계속 독립적이다.
- GPU Profile 테이블의 `profileCount`는 Scene 전체의 고유 Profile 수다. Runtime-local index를 사용하는 CPU 접촉 입력과 Scene index를 사용하는 Shader 조회는 같은 Profile handle을 가리킨다.
- GPU index 변환은 `.Surface` 파일 형식과 Geometry fingerprint를 변경하지 않는다. CPU 전처리 캐시를 재사용할 수 있다.
- 단일 Profile buffer의 크기는 Scene 전체 Profile 수와 channel 수에 따라 정해지며 기존 storage-buffer range 검증을 적용한다.
- `MDSS_SceneResources` CTest는 캐시된 외부 State 제외, Scene 전환, 서로 다른 로컬 테이블 순서의 GPU 공유·주입, instance 격리, 튜닝 초기화·해상도 변경 시 유지, GPU 자원 생성 실패 복원과 빈 Scene 전환을 검증한다. Native window와 Vulkan context가 필요하며 실행 중 Vulkan validation error도 검사한다.

## Related

- [[0006-Dynamic-State-Registry|ADR 0006 — 동적 State Registry]]
- [[0009-Texel-Profile-Index-Map|ADR 0009 — Texel별 Profile Index Map]]
- [[0012-Scene-Profile-Distribution-Reference|ADR 0012 — Scene별 Profile Map 참조]]
- [[0026-Resolution-Surface-Cache|ADR 0026 — Surface 캐시]]
- [[../04_Architecture/0008_Surface-GPU-Data-Layout|GPU Data Layout]]
- [[../06_Development/Notes/0003_Surface-State-GPU-Resource|GPU Resource 구현]]
