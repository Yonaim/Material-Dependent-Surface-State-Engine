# Surface Data Contract 테스트 사례

상태: **통과** · 대상 브랜치: `feat/surface-data-contract`

브랜치 1에서 정의한 CPU 자료형, `.SRProfile` loader, Geometry와 Instance State 계약의 구체적인 테스트 사례를 관리한다. 공통 원칙은 [[0000_Testing-Guide|Testing Guide]]를 따른다.

## 테스트 함수 구성

| 테스트 함수 | 입력 방식 | 검증 범위 |
|---|---|---|
| `TestStateChannelContract` | enum과 문자열 직접 전달 | 네 채널 순서, 이름 변환, 잘못된 enum과 미지원 이름 거부 |
| `TestProfileValidationAndLoading` | C++ Profile 객체와 JSON fixture | 값 검증, JSON parsing, State transition |
| `TestGeometryAndInstanceDataContract` | C++ 객체 직접 구성 | Surface 범위, sentinel, 해상도, Profile 연결, 초기 State |
| `TestContactInputType` | `SurfaceContactInput` 직접 구성 | 입력 필드와 기본값 |

## 검증 사례

| 검증 대상 | 입력·조건 | 기대 결과 | 테스트 방식 | 상태 |
|---|---|---|---|---|
| State 채널 계약 | 네 채널 enum·문자열 순회 | `Wetness`, `Heat`, `Burn`, `Mud` 순서와 이름 유지 | C++ 직접 검증 | 구현됨 |
| 정상 Profile | `Valid.SRProfile` 로드 | 채널별 값과 `Heat → Burn` 전이 변환 | 정상 fixture 로드 | 구현됨 |
| 필수 State 누락 | `MissingState.SRProfile` | 필수 `states` 항목 누락 오류 | 오류 fixture 로드 | 구현됨 |
| Capacity 하한 | `StateCapacity = 0` | 양수 조건 위반으로 거부 | C++ 객체 직접 검증 | 구현됨 |
| 음수 Rate | `DecayRate = -0.1F` | 음수 값 거부 | C++ 객체 직접 검증 | 추가됨 |
| Factor 경계 | Retention/Fill에 각각 `0`과 `1` 입력 | 경계값 허용 | C++ 객체 직접 검증 | 추가됨 |
| Factor 범위 초과 | Retention 또는 Fill에 `1.1F` 입력 | `[0, 1]` 위반으로 거부 | C++ 객체 직접 검증 | 추가됨 |
| 자기 전이 | `Heat → Heat` 입력 | Source와 Target이 달라야 한다는 오류 | C++ 객체 직접 검증 | 구현됨 |
| 알 수 없는 JSON State | Transition `source`에 `snow` 입력 | JSON key 경로를 포함한 알 수 없는 채널 오류 | `UnknownState.SRProfile` 로드 | 추가됨 |
| JSON 자료형 오류 | 숫자 `decayRate`에 문자열 입력 | JSON 경로를 포함한 숫자 자료형 오류 | `WrongFieldType.SRProfile` 로드 | 추가됨 |
| 필수 parameter 누락 | `states.wetness.inputFactor` 생략 | 누락된 JSON key 경로 오류 | `MissingParameter.SRProfile` 로드 | 추가됨 |
| Surface 해상도 | Width 또는 Height가 `0` | 생성 거부 | C++ 객체 직접 검증 | 구현됨 |
| 빈 Surface 목록 | 빈 정의 목록 전달 | 최소 하나의 Surface가 필요하다는 오류 | C++ 생성자 검증 | 추가됨 |
| Sentinel 충돌 | `InvalidSurfaceID`를 실제 Surface ID로 전달 | 예약값 사용 오류 | C++ 생성자 검증 | 추가됨 |
| 비연속 Surface ID | 첫 Surface ID가 `1` | ID가 0부터 연속이어야 한다는 오류 | C++ 생성자 검증 | 구현됨 |
| Profile index sentinel | `InvalidSurfaceProfileIndex`를 연결 | 예약 index 오류 | C++ 생성자 검증 | 구현됨 |
| Surface/Profile 개수 불일치 | Surface 2개에 Profile index 1개 전달 | Surface마다 Profile index 하나를 요구하는 오류 | C++ 생성자 검증 | 추가됨 |
| 알 수 없는 Surface 조회 | 존재하지 않는 ID 조회 | `out_of_range` 오류 | C++ 객체 직접 검증 | 구현됨 |

## Fixture 목록

| 파일 | 목적 |
|---|---|
| `Valid.SRProfile` | 네 채널과 `Heat → Burn`을 포함한 정상 Profile |
| `MissingState.SRProfile` | 필수 State 데이터 누락 거부 |
| `UnknownState.SRProfile` | Transition에서 미지원 State 이름 거부 |
| `WrongFieldType.SRProfile` | 숫자 필드에 문자열이 입력된 경우 거부 |
| `MissingParameter.SRProfile` | 필수 parameter 누락 거부 |

## 실행 및 완료 확인

| 항목 | 기준 |
|---|---|
| 실행 명령 | `ctest --test-dir <build-dir> --output-on-failure` |
| 실행 조건 | CPU contract test는 Vulkan 초기화 없이 실행 |
| 완료 기준 | 모든 사례가 구현되고 테스트 실행이 성공 |

GPU memory packing, descriptor와 barrier 검증은 GPU Resource 단계에서 별도로 다룬다.
