# ADR 0004 — Surface / Material / SRProfile 연결

- 상태: **Accepted**
- 근거: [[05_Assets/Documents/0003_Asset-Structure.pdf|에셋 구조]]

## Context

OBJ Surface는 `.mtl`을 통해 Render Material을 이미 알고 있으며, State 반응은 별도 `.srprofile`에 저장한다.

## Decision

- 하나의 Surface는 하나의 Render Material과 하나의 SRProfile을 사용한다.
- `.scene`의 `materialProfiles`에서 MTL Material 이름을 키로 `.srprofile`을 연결한다.
- 서로 다른 Surface가 같은 Material / SRProfile을 공유할 수 있다.
- 현재 설계에서는 Texel별 Profile ID Map을 두지 않는다.

## Consequences

Surface 내 모든 texel은 동일한 Profile을 사용한다. `ProfileBoundaryWeight`는 서로 다른 Profile을 사용하는 Surface 경계에서 의미가 있다. Simulation UV와 Surface 간 Neighbor 연결 방식은 별도 설계에서 다룬다. [[02_Architecture/0004_Assets-and-Profiles|에셋과 프로필]].
