# Source Index

> **한 줄 요약:** PDF 원본 자료와 각 자료가 뒷받침하는 설계 주제를 찾아볼 수 있는 색인이다.

이 폴더의 PDF는 현재 설계 정리의 근거 자료다. 원본 PDF 자체는 수정하지 않는다. PDF 이후 대화에서 명시적으로 수정·확정된 설계는 Architecture / ADR 문서에 반영한다.

| 파일 | 주요 내용 |
|---|---|
| [[08_Assets/Documents/0001_Overall-Engine-Structure.pdf\|Overall-Engine-Structure.pdf]] | 엔진 모듈 구조 |
| [[08_Assets/Documents/0002_Surface-System-Data.pdf\|Surface-System-Data.pdf]] | SRProfile 파라미터, State / TempState, Shared Geometry Data |
| [[08_Assets/Documents/0003_Asset-Structure.pdf\|Asset-Structure.pdf]] | OBJ / MTL / scene / srprofile 직렬화와 연결 |
| [[08_Assets/Documents/0004_Contact-Input.pdf\|Contact-Input.pdf]] | `TSurfaceContactInput` 구조 |
| [[08_Assets/Documents/0005_Next-State-Calculation.pdf\|Next-State-Calculation.pdf]] | Input / Transport / Decay, ContactWeight, Flux, TransferWeight, 2-Pass |
| [[08_Assets/Documents/0006_Geometry-Integration.pdf\|Geometry-Integration.pdf]] | Macro / Meso Geometry, Normal Map, Accumulation Height, Rendering |
| [[08_Assets/Documents/0007_Target-Demos.pdf\|Target-Demos.pdf]] | 목표 데모 네 가지 |
| [[08_Assets/Documents/0008_2026-09-11-Meeting.pdf\|2026-09-11-Meeting.pdf]] | 9월 11일 면담 기록(참고용) |

## PDF 이후 반영된 최신 설계

- 초기 ADR 0001의 `State [0,stateCapacity] + TempState` 계약은 [[../../05_ADR/0020-State-Overcapacity-Transport|ADR 0020]]으로 변경했다. 현재 설계는 전체 State A/B에 초과량을 보존하고 Capacity를 포화 기준량으로 쓴다. `TempState`는 Solver 중간값이며 초과량 전용 저장은 추가하지 않는다. Shader 변경은 구현했고 GPU 실행 검증은 대기 중이다.
- `Saturation = State / stateCapacity`. 전달 계산에서 1 초과를 허용하며 표시 정규화와 분리한다.
- Transport는 `SaturationDrive + GeometryDrive` 구조.
- `TransferWeight = Distance × Normal × Curvature × ProfileBoundary`.
- `ProfileBoundaryWeight`는 Material 이름이 아니라 **SRProfile 경계** 기준.
- `AccumulationAmount = State × accumulationFactor`.
- Wetness는 내부 흡수 수분, SurfaceWater는 표면 위 물로 구분.
- Simulation UV mapping과 GPU Resource Layout의 4주차 구현 기본안을 작성함.
