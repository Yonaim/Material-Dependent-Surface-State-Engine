# 문서 정책

이 볼트는 **현재 확정된 설계**, **구현 기본안**, **아직 정해야 하는 항목**을 구분한다. 설계 문서에 적혀 있다는 사실만으로 구현 완료를 의미하지 않는다.

## 상태 표기

- **확정**: 현재 설계에서 의미와 역할이 정해진 내용.
- **설계 기본안**: 구현 방향은 정했지만 성능·세부 구현 검증이 남은 내용.
- **검토 필요**: 알고리즘, 저장 방식, 수치 범위 등이 아직 확정되지 않은 내용.

실제 구현 완료 여부는 코드와 실험 결과를 확인한 뒤 [[TODO|TODO]]에 반영한다.

## 문서 책임

- `01_Project-Policy/`: 공식 용어와 문서 규칙.
- `02_Architecture/`: 시스템의 논리 구조, 데이터 의미, 시스템 정의 수식.
- `03_ADR/`: 중요한 설계 결정을 선택한 이유와 결과.
- `04_Development/`: 구현 순서, GPU 패스, 실험·디버깅 등 구현 세부.
- `05_Assets/Documents/`: 설계의 근거가 된 원본 PDF.

같은 내용을 여러 문서에 복제하지 않는다. 예를 들어 **SRProfile 파라미터의 의미와 범위는 [[02_Architecture/Surface-State|표면 상태와 데이터 구조]]에서만 정의**하고, [[02_Architecture/Assets-and-Profiles|에셋과 프로필]]에서는 파일 직렬화와 연결 관계만 다룬다.

## 출처와 최신성

`05_Assets/Documents/`의 PDF는 설계 근거 자료다. 이후 대화에서 명시적으로 수정·확정된 설계는 PDF의 이전 표현보다 우선한다. 특히 다음 변경은 현재 설계에 반영한다.

- `Overflow` 초과량 모델 폐기, `State + TempState` 사용.
- 상태별 `stateCapacity`와 파생값 `Saturation` 사용.
- Transport를 `SaturationDrive`와 `GeometryDrive`로 분리.
- `TransferWeight`를 Distance / Normal / Curvature / Profile Boundary로 구성.
- 적층량은 `State × accumulationFactor`에서 계산.
- 표면 위 물(`SurfaceWater`)과 내부 흡수 수분(`Wetness`)의 의미를 구분.

원본 자료 목록은 [[05_Assets/Documents/Source-Index|Source Index]]에서 확인한다.
