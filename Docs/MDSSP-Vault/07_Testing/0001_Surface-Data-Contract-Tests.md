# Surface Data Contract 테스트 사례

> **한 줄 요약:** CPU 자료형, `.SRProfile` loader, State Registry, Surface geometry/Profile map 및 Runtime preprocessing 계약의 테스트 사례를 관리한다.

상태: **통과** · 대상 브랜치: `feat/surface-data-contract`, `feat/simulation-mapping`

CPU 자료형, `.SRProfile` loader, State Registry, Surface geometry/Profile map 및 Runtime preprocessing 계약의 테스트 사례를 관리한다. 공통 원칙은 [[0000_Testing-Guide|Testing Guide]]를 따른다.

## 테스트 함수 구성

| 테스트 함수 | 입력 방식 | 검증 범위 |
|---|---|---|
| `TestProfileAndRegistry` | Profile 객체와 JSON fixture | 임의 State 이름, 정규화, Registry ID, Transition 참조 및 재현성 |
| `TestTransferFactorValidation` | C++ Profile 객체 | 두 Factor의 `[0,1]` 경계값 허용 및 음수·상한 초과·NaN/Inf 거부 |
| `TestGeometryAndInstanceData` | C++ 객체 직접 구성 | Surface 범위, sentinel, texel Profile map, 동적 State 채널 |
| `TestContactInputType` | `TSurfaceContactInput` 직접 구성 | Registry `TStateId`와 입력 기본값 |
| `TestSurfacePreprocessing` | Mapping/Profile Distribution 입력 | Profile map 구성, deterministic build 및 같은 입력의 결과 공유 |

## 검증 사례

| 대상 | 입력·조건 | 기대 결과 | 방식 |
|---|---|---|---|
| 정규화 전달 계수 | version 2의 두 TransferFactor | Factor를 그대로 로드하고 Solver 기준 속도는 적용하지 않음 | `Valid.SRProfile` |
| 이전 schema | version 1의 실제 Rate 필드 | version 2 요구 오류 | `LegacyRates.SRProfile` |
| 범위 밖 전달 계수 | version 2에서 GeometryTransferFactor=50 | 필드 경로와 `[0,1]` 오류 | `InvalidTransferFactor.SRProfile` |
| Factor 범위 | 각 Factor의 0, 0.5, 1 및 음수·1 초과·NaN/Inf | 정상 범위 허용, 나머지 거부 | C++ 직접 검증 |
| 부분 State 정의 | Profile에 임의 State key 하나만 선언 | 고정된 채널 집합 없이 로드 | 정상 JSON fixture |
| 이름 정규화 | 대소문자와 앞뒤 ASCII whitespace 차이 | 같은 canonical name; 구두점과 내부 공백은 보존 | 직접 검증 및 fixture |
| Registry 구성 | 둘 이상의 Profile에 서로 다른 State 선언 | 전체 State union에 deterministic ID 배정 | C++ 직접 검증 |
| ID 재현성 | 동일 Profile 집합을 다른 순서로 제공 | bytewise 이름 정렬에 따라 같은 ID | C++ 직접 검증 |
| 미지원 State slot | 한 Profile에만 있는 State를 다른 Profile에서 조회 | `optional` empty로 미지원 표시 | C++ 직접 검증 |
| Transition endpoint | 등록 State를 가리키는 source/target | runtime `TStateId`로 변환 | C++ 직접 검증 |
| 알 수 없는 Transition endpoint | 전체 Profile 집합에 없는 State 참조 | Registry 생성 오류 | `UnknownState.SRProfile` |
| 잘못된 JSON 자료형 | 숫자 필드에 문자열 입력 | JSON 경로를 포함한 오류 | `WrongFieldType.SRProfile` |
| 필수 parameter 누락 | State parameter 하나 생략 | 누락된 필드 경로를 표시 | `MissingParameter.SRProfile` |
| parameter 범위 | 음수 Rate, Capacity 0, Factor 범위 초과 | 검증 오류 | C++ 직접 검증 |
| Factor 경계 | Factor에 `0`과 `1` 입력 | 허용 | C++ 직접 검증 |
| 자기 전이 | 정규화 후 source와 target이 같음 | 거부 | C++ 직접 검증 |
| Geometry sentinel | 예약 Surface ID를 실제 ID로 입력 | 거부 | C++ 직접 검증 |
| 빈 Surface 목록 | 빈 정의 목록 전달 | 거부 | 생성자 검증 |
| Texel Profile map | valid/invalid texel에 정상 index/sentinel 설정 | count·sentinel·ProfileCount 검사 통과 | C++ 직접 검증 |
| 잘못된 Profile index | index가 `ProfileCount` 이상 | Runtime Surface build 거부 | C++ 직접 검증 |
| Runtime build 재현성 | 같은 입력으로 전처리 반복 | geometry 및 Profile map 동일 | CPU builder |
| Runtime 공유 | 동일 Mesh/Profile Distribution을 쓰는 복수 instance | 동일한 전처리 결과 참조 | Runtime Asset/Scene 연결 |
| 전처리 입력 변경 | Mesh UV 또는 Profile Distribution 변경 | 새 입력으로 Runtime 결과 재생성 | C++ 직접 검증 |

## Fixture 목록

| 파일 | 목적 |
|---|---|
| `LegacyRates.SRProfile` | version 1의 Rate schema를 거부 |
| `InvalidTransferFactor.SRProfile` | version 2의 정규화 계수 범위 위반을 거부 |
| `Valid.SRProfile` | 정규화할 이름과 Transition을 포함하는 다중 State Profile |
| `MissingState.SRProfile` | 임의의 단일 State만 지원하는 부분 Profile |
| `UnknownState.SRProfile` | 존재하지 않는 Transition endpoint를 Registry에서 거부 |
| `WrongFieldType.SRProfile` | 숫자 필드가 잘못된 JSON 자료형인 경우 |
| `MissingParameter.SRProfile` | 필수 parameter가 누락된 경우 |

## 실행 및 완료 확인

| 항목 | 기준 |
|---|---|
| 실행 명령 | `ctest --test-dir <build-dir> --output-on-failure` |
| 실행 조건 | CPU contract test는 Vulkan 초기화 없이 실행 |
| 완료 기준 | 모든 contract test가 성공 |

GPU memory packing, descriptor와 barrier 검증은 GPU Resource 단계에서 별도로 다룬다.

## 면적·시간 계약 확장

`MDSS_SimulationTransport`는 15·30·60·120 FPS clock, 반복 한도·backlog·pause·배속 및 128·256·512의 비균일 scale 면적 합을 검사한다. `MDSS_SurfaceGPUResource`는 면적 환산 Capacity·Decay, 상한 없는 Geometry mobility, SaturationDrive OFF, 한 command buffer의 반복·입력 한 번 소비와 단일 이웃의 공간 스케일을 cache ON/OFF에서 검사한다. `MDSS_SceneResources`는 실제 접촉 입력의 면적 환산을 확인한다. 전체 결과와 아직 실시하지 않은 비교 범위는 [[../06_Development/Experiments/0006_Area-and-Timestep-Regression|면적·시간 회귀 검증]]을 따른다.

Geometry 기준값 6000은 `MDSS_SurfaceGPUResource`에서 기본 Factor 0.5의 128·256·512 국소 이동률과 cache ON/OFF를 검사한다. `MDSS_SceneResources`에서는 Factor 변경 및 Geometry OFF에 따른 안전 시간 간격 변경을 검사한다. 조건과 결과는 [[../06_Development/Experiments/0007_Geometry-Rate-Recalibration|Geometry 재보정 검증]]을 따른다.

`MDSS_SimulationTransport`는 Fixed·Auto 네 조합과 기본값 ON·OFF를 검사한다. 작은 Transport 상한에서도 Auto OFF의 고정 dt=1/60초가 유지되는지, Auto ON이 전체 고정 구간 대기·마지막 짧은 substep·반복 한도 이후 재개·전환·Reset에서 시간을 보존하는지 확인한다. 현재 정책은 [[../05_ADR/0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034]]다.
