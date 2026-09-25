# ADR 0004 — Surface / Material / SRProfile 연결

- 상태: **Partially Superseded by [[0007-Surface-Preprocessed-Asset]], [[0008-Runtime-Surface-Preprocessing]], and [[0009-Texel-Profile-Index-Map]]**
- 근거: [[07_Assets/Documents/0003_Asset-Structure.pdf|에셋 구조]]

## Context

OBJ Surface는 `.mtl`을 통해 Render Material을 이미 알고 있으며, State 반응은 별도 `.SRProfile`에 저장한다.

## Decision

> [!warning] 대체 범위
> Render Material과 SRProfile이 서로 다른 책임이라는 결정은 유지한다. Surface당 하나의 SRProfile만 지정하고 texel별 Profile map을 두지 않는 결정은 [[0007-Surface-Preprocessed-Asset]]에서 대체되어 texel별 Profile map을 채택했다. 그 map의 `.Surface` persistent cache 저장은 [[0008-Runtime-Surface-Preprocessing]]에서 Runtime 메모리 전처리로 대체됐다. Texel마다 dense `ProfileIndex`를 저장하는 기본 표현은 [[0009-Texel-Profile-Index-Map]]에서 정한다.

### 이전 결정 기록 — 대체됨

- 하나의 Surface는 하나의 Render Material과 하나의 SRProfile을 사용한다.
- `.Scene`의 `materialProfiles`에서 MTL Material 이름을 키로 `.SRProfile`을 연결한다.
- 서로 다른 Surface가 같은 Material / SRProfile을 공유할 수 있다.
- Texel별 Profile ID Map을 두지 않는다.

위 항목은 이 ADR 작성 당시의 결정 이력이며 현재 Profile 배치 규칙으로 사용하지 않는다.

## Consequences

Surface 내 모든 texel은 동일한 Profile을 사용한다. `ProfileBoundaryWeight`는 서로 다른 Profile을 사용하는 Surface 경계에서 의미가 있다. Simulation UV와 Surface 간 Neighbor 연결 방식은 별도 설계에서 다룬다. [[03_Architecture/0003_Assets-and-Profiles|에셋과 프로필]].

## Alternatives Considered

### 1. Mesh 전체에 SRProfile 하나만 적용

연결이 단순하고 Surface별 table이 필요 없다. 하지만 한 Mesh 안의 서로 다른 재질 표면에 다른 반응 특성을 줄 수 없다. Surface별 연결을 선택했다.

### 2. Texel마다 Profile ID 저장

세밀한 분할이 가능하지만 같은 Surface 안에서도 Profile이 달라질 수 있고, texel별 index 데이터와 관리가 추가된다. 현재는 Surface가 Profile 경계의 최소 단위이므로 사용하지 않는다.

### 3. Material 이름과 별개로 Surface index에 직접 Profile 지정

가능하지만 기존 OBJ/MTL이 제공하는 Surface별 Material 이름과 Profile 연결 설정이 분리된다. `.Scene`의 `materialProfiles`를 통해 Material 이름으로 연결하는 방식을 선택했다.
