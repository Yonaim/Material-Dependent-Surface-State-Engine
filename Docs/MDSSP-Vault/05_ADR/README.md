# Architecture Decision Records

> **한 줄 요약:** 결정의 주제에 따라 ADR을 분류하고, 번호는 전체 ADR에서 고유하게 유지한다.

각 ADR의 번호는 문서 ID다. ADR을 다른 분류로 옮기더라도 번호와 파일명은 유지한다.

## Simulation

상태 모델, 표면 형상과 이동 Solver의 결정을 둔다.

- [[05_ADR/Simulation/0001-Capacity-and-Saturation|0001 — Capacity와 Saturation]]
- [[05_ADR/Simulation/0002-Transport-Drive-and-Weight|0002 — Transport Drive와 TransferWeight]]
- [[05_ADR/Simulation/0003-Dynamic-Accumulation-Geometry|0003 — Accumulation Height와 동적 형상]]
- [[05_ADR/Simulation/0015-Geometry-Driven-Transport|0015 — Geometry-Driven Transport]]
- [[05_ADR/Simulation/0016-Transport-Transfer-Weights|0016 — Transport TransferWeight]]
- [[05_ADR/Simulation/0017-Solver-Transfer-Cache|0017 — Solver Transfer 캐시]]
- [[05_ADR/Simulation/0018-Normal-Map-Meso-Geometry|0018 — Normal Map 기반 Virtual Meso Geometry]]
- [[05_ADR/Simulation/0019-Optional-Curvature-Transfer-Weight|0019 — 선택적 CurvatureWeight]]
- [[05_ADR/Simulation/0020-State-Overcapacity-Transport|0020 — State 초과량 Transport]]

## Rendering

최종 표면 형상과 Normal Map을 렌더링에 반영하는 결정을 둔다.

- [[05_ADR/Rendering/0021-Accumulation-Height-and-Normal-Map|0021 — Accumulation Height와 Normal Map 렌더링]]

## Architecture

공통 GPU 자료 배치, 상태 Registry, 동기화와 접촉 입력 API 결정을 둔다.

- [[05_ADR/Architecture/0005-Per-Texel-GPU-Data-Layout|0005 — Per-Texel GPU Data Layout]]
- [[05_ADR/Architecture/0006-Dynamic-State-Registry|0006 — Dynamic State Registry]]
- [[05_ADR/Architecture/0010-Dynamic-State-GPU-Buffer-Layout|0010 — Dynamic State GPU Buffer Layout]]
- [[05_ADR/Architecture/0011-GPU-Resource-Initialization-and-ABI|0011 — GPU Resource 초기화와 ABI]]
- [[05_ADR/Architecture/0013-InputDelta-Host-Upload-Synchronization|0013 — InputDelta Host Upload 동기화]]
- [[05_ADR/Architecture/0014-Surface-Contact-Target-API|0014 — Surface Contact Target API]]

## Assets

Material·Profile 연결, 전처리와 Scene의 Profile Distribution 참조 결정을 둔다.

- [[05_ADR/Assets/0004-Asset-Profile-Mapping|0004 — Surface와 Profile 연결]]
- [[05_ADR/Assets/0007-Surface-Preprocessed-Asset|0007 — 정적 Surface 전처리 에셋]]
- [[05_ADR/Assets/0008-Runtime-Surface-Preprocessing|0008 — Runtime Surface 전처리]]
- [[05_ADR/Assets/0009-Texel-Profile-Index-Map|0009 — Texel별 Profile Index Map]]
- [[05_ADR/Assets/0012-Scene-Profile-Distribution-Reference|0012 — Scene별 Profile Distribution 참조]]
- [[05_ADR/Assets/0026-Resolution-Surface-Cache|0026 — 해상도별 Surface 전처리 캐시]]
