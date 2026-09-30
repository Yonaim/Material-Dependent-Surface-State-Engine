# 문서 정책

> **한 줄 요약:** 이 볼트는 현재 확정된 설계, 구현 기본안, 아직 정해야 하는 항목을 구분한다.

이 볼트는 **현재 확정된 설계**, **구현 기본안**, **아직 정해야 하는 항목**을 구분한다. 설계 문서에 적혀 있다는 사실만으로 구현 완료를 의미하지 않는다.

## 상태 표기

- **확정**: 현재 설계에서 의미와 역할이 정해진 내용.
- **설계 기본안**: 구현 방향은 정했지만 성능·세부 구현 검증이 남은 내용.
- **검토 필요**: 알고리즘, 저장 방식, 수치 범위 등이 아직 확정되지 않은 내용.

실제 구현 완료 여부는 코드와 실험 결과를 확인한 뒤 [[TODO|TODO]]에 반영한다.

## 문서 책임

- `01_Project-Policy/`: 공식 용어, 문서 규칙, 프로젝트 전반에 적용되는 원칙.
- `02_Research/`: 외부 연구·기술 근거의 쉬운 요약, 상세 설명과 프로젝트 적용 검토.
- `03_Planning/`: 프로젝트 전체 계획, 전체 주차 목표 요약, 주차별 상세 구현 계획.
- `Flow-Maps/`: 입력부터 결과까지 여러 시스템을 통과하는 처리 순서와 구현 경계.
- `04_Architecture/`: 시스템의 논리 구조, 데이터 의미, 시스템 동작 규칙과 정의 수식.
- `05_ADR/`: 중요한 설계 결정을 선택한 이유와 결과. ADR은 [[05_ADR/README|주제별 분류와 색인]]을 기준으로 Architecture, Assets, Rendering, Simulation으로 나눈다.
- `06_Development/`: 구현 순서와 구체적인 구현 방식, 미검증 구현안, 실험·디버깅 기록.
- `07_Testing/`: 공통 테스트 구성 기준, 사례 작성 원칙과 회귀 검증 문서.
- `08_Assets/Documents/`: 설계의 근거가 된 원본 PDF.

주차별 계획은 [[03_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]]과 [[03_Planning/00_Project-Overview/0001_Roadmap|Roadmap]]을 기준으로 한다. 실제 완료 여부와 당장 남은 작업은 [[TODO|TODO]]에서 관리한다.

같은 내용을 여러 문서에 복제하지 않는다. 예를 들어 **SRProfile 파라미터의 의미와 범위는 [[04_Architecture/0002_Surface-State|표면 상태와 데이터 구조]]에서만 정의**하고, [[04_Architecture/0003_Assets-and-Profiles|에셋과 프로필]]에서는 파일 직렬화와 연결 관계만 다룬다.

시스템이 무엇을 의미하고 어떤 규칙으로 동작하는지는 Architecture에 둔다. Flow Map은 여러 기준 문서와 구현 단계를 하나의 처리 순서로 연결하며 정의나 수식을 새로 만들지 않는다. 특정 주차의 임시 선택, GPU 자원 배치, 세부 실행 순서, 아직 검증이 필요한 구현 방식은 Development에 둔다. 구현 방식 중 중요한 대안 선택과 그 근거를 기록해야 하면 ADR을 작성한다. 미정이라는 이유만으로 시스템 동작의 의미나 수식 정의를 Development로 옮기지는 않는다.

## 파일명 규칙

- 정식 문서는 디렉터리별로 `0000_이름.md` 형식의 네 자리 번호를 붙인다. `06_Development/Notes/`의 임시 메모는 번호 없이 주제 이름으로 작성한다.
- Overview, Guide, Index처럼 폴더의 시작점은 `0000`이다.
- ADR은 문서 ID를 보존하기 위해 `0001-이름.md` 형식을 사용한다.
- 주차별 목표 요약은 `01_Weekly-Overview/Week-01.md`부터 `Week-16.md`에 둔다.
- 주차별 구현 상세는 `02_Weekly-Details/Week-XX/` 아래에 둔다.
- `Flow-Maps/` 내부 문서는 `0000_Overview.md`에서 시작해 권장 읽기 순서대로 번호를 붙인다.
- 번호는 권장 읽기·구현 순서를 나타내며 문서 제목에는 포함하지 않는다. 임시 메모에 정식 문서로 남길 내용이 생기면 담당 문서로 옮기고 메모를 정리한다.

## 출처와 최신성

`08_Assets/Documents/`의 설계 PDF는 검토·확정된 내용을 판본 단위로 보존하는 기준 문서다. 같은 판본을 작업 메모처럼 수시 수정하지 않지만, 설계가 바뀌어 새 기준이 확정되면 PDF도 개정해 발행일·개정 정보와 함께 갱신한다. Markdown은 세부 정의와 현재 구현·검증 상태를 갱신한다. 둘 사이에 차이가 생기면 Source Index에 보완 항목을 적고, 문서 내용을 일치시킨다. 면담·회의록은 당시 기록으로 보존하고 후속 결정을 현재 설계 문서에 반영한다. 자료별 역할은 [[08_Assets/Documents/0000_Source-Index|Source Index]]를 따른다.

특히 다음 변경은 현재 설계와 PDF 보완 검토에 반영한다.

- `Overflow` 초과량 모델 폐기. Capacity 초과량은 State에 포함하고 Solver 중간값은 목적별 scratch buffer로 둔다.
- 상태별 `stateCapacity`와 파생값 `Saturation` 사용.
- Transport를 `SaturationDrive`와 `GeometryDrive`로 분리.
- `TransferWeight`를 Distance / Normal / Curvature / Profile Boundary로 구성.
- 적층량은 `State × accumulationFactor`에서 계산.
- 표면 위 물(`SurfaceWater`)과 내부 흡수 수분(`Wetness`)의 의미를 구분.

원본 자료 목록은 [[08_Assets/Documents/0000_Source-Index|Source Index]]에서 확인한다.
