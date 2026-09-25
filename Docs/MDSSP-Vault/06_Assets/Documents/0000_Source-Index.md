# Source Index

이 폴더의 PDF는 현재 설계 정리의 근거 자료다. 원본 PDF 자체는 수정하지 않는다. PDF 이후 대화에서 명시적으로 수정·확정된 설계는 Architecture / ADR 문서에 반영한다.

| 파일 | 주요 내용 |
|---|---|
| [[06_Assets/Documents/0001_Overall-Engine-Structure.pdf\|Overall-Engine-Structure.pdf]] | 엔진 모듈 구조 |
| [[06_Assets/Documents/0002_Surface-System-Data.pdf\|Surface-System-Data.pdf]] | SRProfile 파라미터, State / TempState, Shared Geometry Data |
| [[06_Assets/Documents/0003_Asset-Structure.pdf\|Asset-Structure.pdf]] | OBJ / MTL / scene / srprofile 직렬화와 연결 |
| [[06_Assets/Documents/0004_Contact-Input.pdf\|Contact-Input.pdf]] | `SurfaceContactInput` 구조 |
| [[06_Assets/Documents/0005_Next-State-Calculation.pdf\|Next-State-Calculation.pdf]] | Input / Transport / Decay, ContactWeight, Flux, TransferWeight, 2-Pass |
| [[06_Assets/Documents/0006_Geometry-Integration.pdf\|Geometry-Integration.pdf]] | Macro / Meso Geometry, Normal Map, Accumulation Height, Rendering |
| [[06_Assets/Documents/0007_Target-Demos.pdf\|Target-Demos.pdf]] | 목표 데모 네 가지 |
| [[06_Assets/Documents/0008_2026-09-11-Meeting.pdf\|2026-09-11-Meeting.pdf]] | 9월 11일 면담 기록(참고용) |

## PDF 이후 반영된 최신 설계

- `Overflow` 초과량 의미 폐기 → `State [0,stateCapacity] + TempState`.
- `Saturation = State / stateCapacity`.
- Transport는 `SaturationDrive + GeometryDrive` 구조.
- `TransferWeight = Distance × Normal × Curvature × ProfileBoundary`.
- `ProfileBoundaryWeight`는 Material 이름이 아니라 **SRProfile 경계** 기준.
- `AccumulationAmount = State × accumulationFactor`.
- Wetness는 내부 흡수 수분, SurfaceWater는 표면 위 물로 구분.
- Simulation UV mapping과 GPU Resource Layout의 4주차 구현 기본안을 작성함.
