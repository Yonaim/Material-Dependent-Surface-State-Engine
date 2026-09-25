# 테스트 구성 가이드

이 문서는 테스트 코드와 입력 fixture의 역할, 테스트 사례를 문서화하는 기준을 정의한다. 특정 주차의 완료 여부는 해당 주차 계획 문서에서 관리하고, 이 문서에는 반복해서 적용할 테스트 구조를 둔다.

## 테스트 코드와 fixture의 역할

| 구성 요소 | 담당 역할 | 사용 예 |
|---|---|---|
| 테스트 전용 C++ 코드 | 객체 구성, 함수 호출, 결과 판정 | 음수 Rate 거부, 빈 Surface 거부 |
| JSON fixture | 실제 파일 입력·역직렬화 경로 검증 | 잘못된 JSON 자료형, 알 수 없는 State 이름 |
| CMake test target | 테스트 실행 파일 빌드 및 CTest 등록 | CPU 단위 테스트 실행 |

JSON parser의 파일 처리 자체를 확인하는 사례에는 fixture를 쓴다. 값 범위나 자료구조 불변조건은 테스트 코드에서 객체를 직접 구성하면 fixture가 불필요하게 늘어나는 것을 막을 수 있다.

## 테스트 함수 구성 예시

테스트 함수는 동작 영역별로 나누고 테스트 전용 C++ 파일은 처음부터 과도하게 분할하지 않는다. 현재 규모에서는 한 파일 안에서 아래처럼 독립적인 테스트 함수를 두는 구성을 기본으로 한다.

| 테스트 함수 | 입력 방식 | 검증 예 |
|---|---|---|
| `TestProfileValidation` | C++ 객체 직접 구성 | 음수 Rate 거부, Factor `0`·`1` 허용 |
| `TestProfileJsonLoading` | JSON fixture를 실제 loader로 로드 | 정상 로드, 알 수 없는 State 및 자료형 오류 거부 |
| `TestSharedGeometryValidation` | C++ 객체 직접 구성 | 빈 Surface 거부, sentinel ID 예약 및 충돌 거부 |
| `TestSurfaceStateMapping` | C++ 객체 직접 구성 | Profile 연결과 상태 배열 크기 검증 |

## 테스트 사례 기록 형식

| 검증 대상 | 입력·조건 | 기대 결과 | 테스트 방식 |
|---|---|---|---|
| Rate 하한 | `DecayRate = -0.1F` | Validation 오류 | C++ 객체 직접 검증 |
| Factor 경계 | Factor `0.0F`, `1.0F` | 유효한 값으로 수락 | C++ 객체 직접 검증 |
| JSON 자료형 | 숫자 필드에 문자열 입력 | 필드 경로를 포함한 오류 | 오류 fixture 로드 |
| 알 수 없는 State | 지원하지 않는 State 이름 | 명시적 로드 오류 | 오류 fixture 로드 |
| 빈 Surface | Surface 목록이 비어 있음 | 계약 위반 오류 | C++ 객체 직접 검증 |
| sentinel 충돌 | 예약 sentinel을 실제 ID로 사용 | 계약 위반 오류 | C++ 객체 직접 검증 |

주차별 테스트 계획은 해당 작업의 계획 문서에 둔다. 이 가이드의 공통 원칙이나 표를 복제하지 말고 필요하면 이 문서를 링크한다.

## Fixture 관리

- fixture는 `Tests/Fixtures/`에 두고 검증 의도가 드러나는 이름을 사용한다. 예: `Valid.SRProfile`, `UnknownState.SRProfile`.
- 정상 입력과 실패 입력을 각각 최소 단위로 구성한다. 하나의 fixture에 여러 오류를 섞지 않는다.
- 테스트가 실행 당시 작업 디렉터리에 의존하지 않도록 CMake가 fixture 경로를 전달하게 한다.
- fixture는 실제 parser/loader를 통해 읽고, 객체 직접 검증을 대신하는 용도로 쓰지 않는다.

## GPU 및 통합 테스트 경계

| 검증 대상 | 적절한 단계 |
|---|---|
| CPU domain validation, JSON parsing, 상태·Profile 연결 | CPU 단위 테스트 |
| CPU/GPU 배치 및 pack 순서 | GPU Resource 구현 후 layout 단위 테스트 |
| Vulkan descriptor, barrier, resource lifetime | Vulkan 통합 테스트와 validation layer |

GPU resource가 아직 존재하지 않는 단계에서 GPU 배치 검증을 완료로 표시하지 않는다.
