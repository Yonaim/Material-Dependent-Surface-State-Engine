# ADR 0009 — Texel별 Profile Index Map

- 상태: **Accepted**
- 날짜: 2026-09-26
- 관련 결정: [[0004-Asset-Profile-Mapping]], [[0008-Runtime-Surface-Preprocessing]]

## Context

Runtime Surface 전처리는 texel별 Profile 배치를 만든다. Profile Map을 압축해 인접 texel의 반복 인덱스를 줄일 수 있지만, Solver는 texel 및 이웃 texel의 Profile 파라미터를 자주 조회한다. 압축 표현은 조회 과정이나 분기 비용을 추가할 수 있다.

## Decision

- 각 유효 texel은 자신이 사용할 SRProfile 테이블 항목의 `ProfileIndex` 하나를 가진다.
- Profile Map은 texel 수와 같은 원소 수의 dense map으로 표현한다. 각 원소는 `uint32` Profile index다.
- 여러 인접 texel이 같은 Profile을 사용해도 각 texel에 index를 각각 저장한다. 중복 index 압축은 기본안에 적용하지 않는다.
- Profile 반응 파라미터와 Transition은 texel에 복제하지 않고 별도 Profile 데이터에 둔다. `(ProfileIndex, ChannelIndex)`로 해당 State 파라미터를 조회한다.
- Runtime Profile Map은 해당 Geometry/Profile Distribution 조합을 쓰는 instance 간 공유한다. `.Surface` 파일이나 persistent cache에는 저장하지 않는다.
- Invalid texel은 `TexelSurfaceIndex`의 예약 sentinel로 판정한다. invalid texel의 Profile index는 조회하지 않는다.
- 현재 authoring 입력은 Surface/Material 할당마다 Profile 하나를 지정하고, Runtime builder가 이를 각 valid texel로 확장한다. Surface 내부의 texel별 Profile authoring은 이 표현을 사용할 수 있는 후속 기능이며 이번 구현 범위에는 포함하지 않는다.

## Rationale

Dense map은 임의 texel 및 이웃 texel에서 인덱스를 일정한 주소 계산으로 바로 읽을 수 있고, CPU/GPU 자료구조와 검증 규칙을 단순하게 유지한다. Map의 압축은 측정된 메모리 병목이 있을 때 다시 검토한다.

## Alternatives Considered

### 1. Tile 또는 run 단위 Profile index 압축

같은 Profile을 쓰는 인접 texel의 중복 인덱스를 줄일 수 있다. 다만 경계 타일의 세부 map, 추가 분기 또는 구간 탐색이 필요할 수 있고, texel별 직접 조회보다 접근 과정이 복잡해진다. 현재 기본안으로는 선택하지 않는다.

### 2. 각 texel에 Profile 파라미터 전체 저장

조회가 직접적이지만 같은 Profile을 쓰는 texel마다 파라미터를 복제해 저장 공간과 업로드량이 커진다. 인덱스와 별도 Profile 테이블을 사용한다.

### 3. Dense texel별 `ProfileIndex` map — 선택

texel당 4-byte index를 저장한다. 인접 texel에서 Profile이 반복되어도 조회가 단순하고 예측 가능하다. 압축 여부는 실제 Profile Map 크기와 GPU 측정 결과를 얻은 뒤 판단한다.

## Consequences

- Profile Map의 공간 비용은 texel당 4 bytes다. 512×512 map은 약 1 MiB이며, 실제 할당 정렬은 포함하지 않은 값이다.
- Solver는 현재 texel과 이웃 texel의 Profile index를 dense map에서 직접 읽을 수 있다.
- Dense map의 texel별 저장 방식과 authoring 입력의 세분화 정도는 별개다. 현재 입력은 Surface 단위 Profile 할당이며, 더 세밀한 Profile Distribution 입력은 후속 계약에서 다룬다.
- 압축이 필요해지면 source/CPU 표현을 압축하고 GPU에 펼치는 방식과 GPU 조회용 압축 표현을 각각 성능 측정해 결정한다.

## Related

- [[03_Architecture/0003_Assets-and-Profiles|Assets and Profiles]]
- [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[04_ADR/0008-Runtime-Surface-Preprocessing|ADR 0008 — Runtime Surface 전처리]]
