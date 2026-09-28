# ADR 0017 — Solver Transfer 캐시와 유출 합계 재사용

> **한 줄 요약:** 인스턴스별 TransferWeight 캐시와 Pass 1 RawOutgoing 합계 재사용을 채택한다.

- Status: **Accepted**
- Date: 2026-09-27
- 구현 상태: **구현 및 GPU 기능 검증 완료 · 실제 Scene 성능 개선은 미확정**

## Context

TransferWeight 구현은 Position/Normal/Neighbor에서 가중치를 즉시 계산한다. rawFlux 호출마다 양 endpoint의 평균 이웃 거리를 다시 순회하며, Pass 1의 유출 합계를 버려 Pass 2에서 다시 계산한다. 동일한 방향의 flux는 Pass 1과 Pass 2 양 endpoint에서 총 세 번 평가될 수 있다. 데모 화면에서 FPS 8.7, frame time 114.71 ms, Solver GPU 122.42 ms가 관측되었다. 이는 단일 관측이며 두 시간 표시는 같은 표본임이 보장되지 않아 정확한 점유율이나 개선 배수의 기준값으로 사용하지 않는다.

## Decision

1. 인스턴스별·이웃 슬롯별 TransferWeight 캐시를 채택한다. DistanceWeight, NormalWeight, 기본 중립·선택적 사전 계산 CurvatureWeight([[0019-Optional-Curvature-Transfer-Weight|ADR 0019]])와 ProfileBoundaryWeight의 곱을 저장하고 rawFlux에서 읽는다. 계산식과 무효 Geometry의 가중치 0 규칙은 ADR 0016을 유지한다.
2. 텍셀·State 채널별 RawOutgoing 합계를 Pass 1에서 저장하고 Pass 2가 재사용한다. OutgoingFluxScale은 기존대로 별도 저장한다.
3. 간선별 Raw flux 버퍼는 이번 구현에서 채택하지 않는다. 1·2 적용 후 GPU 시간과 메모리 비용을 측정해 후속 결정한다.
4. TransferWeight cache는 최신 유효 표면 형상을 사용한다. 유효 위치는 `BasePosition + BaseNormal × (MesoVirtualHeight + AccumulationHeight)`이며 instance transform을 적용한다. 현재 upload는 MesoVirtualHeight만 포함하고, AccumulationHeight는 future dynamic geometry input이다. NormalWeight에는 같은 유효 형상에서 갱신된 normal을 사용하고, instance transform의 inverse-transpose를 적용한다. GeometryDrive의 높이와 방향도 이 최신 형상을 사용한다. 이는 ADR 0003의 적층 형상 반영 결정을 따른다.
5. 준비 단계는 월드 위치·법선, 텍셀별 평균 유효 이웃 거리, 간선별 가중치 순서로 계산한다. 평균은 텍셀당 한 번만 계산한다. inverse-transpose normal matrix는 인스턴스 변환 갱신 시 한 번 산출한다. 준비 단계의 CPU/GPU 배치는 구현에서 확정하고 기록한다.
6. Meso/Accumulation 형상 갱신이 매 Solver step 바뀌면 그 갱신 뒤 TransferWeight cache를 한 번 재생성한다. 형상 revision이 그대로면 재사용한다. 캐시 준비 완료 및 필요한 barrier 이전에는 Solver를 실행하지 않는다. 기존 2-Pass 제한, 입력 소비 시점, State A/B 전환, Registry 기반 채널 구조를 유지한다.

| 방안 | 선택 | 연산 감소 | payload 추가 메모리 | 캐시 갱신 조건 |
|---|---|---|---|---|
| 간선별 TransferWeight | 채택 | 캐시 유효 시 평균 이웃 거리의 매-step 순회 제거 | 인스턴스당 48 MiB | 최초 생성, 유효 위치/normal revision, 현재 MesoVirtualHeight 및 향후 AccumulationHeight 변경, 이웃/Surface/Profile 배치 변경, 인스턴스 선형 변환 변경, weight 규칙 변경 |
| 텍셀별 RawOutgoing 합계 | 채택 | rawFlux 호출 상한 24→16회/텍셀·채널, 약 33% 감소 | 인스턴스당 채널당 6 MiB | 매 Solver step의 Pass 1에서 덮어쓰기 |
| 간선별 Raw flux | 보류 | 단독 채택 시 호출 상한 24→8회/텍셀·채널, 약 67% 감소 | 인스턴스당 채널당 48 MiB 및 역방향 슬롯 조회 정보 | 매 Solver step의 Pass 1에서 덮어쓰기 |

메모리 가정: 6 Surface × 512×512 = 1,572,864 texel, 8 이웃 슬롯, 각 원소 float32 4 byte, 원소별 추가 padding 없음, 모든 슬롯을 할당한다. TransferWeight는 채널 독립이며 인스턴스 변환에 의존하므로 인스턴스별 소유다. 합계는 texel-major 인덱스 `texel × ChannelCount + channel`을 사용한다. 버퍼 정렬·할당 overhead와 준비용 scratch는 위 payload에 포함하지 않는다. 호출 감소와 실행 시간 감소는 같은 비율이 아니다.

## Alternatives Considered

- 기존 즉시 계산 유지: 추가 메모리가 없지만 중첩 이웃 순회와 Pass 간 중복이 유지된다.
- TransferWeight만 저장: 큰 Geometry 중복은 제거하지만 Pass 2의 유출 합계 재계산이 남는다. 작은 합계 버퍼도 함께 채택한다.
- 간선별 Raw flux 저장: 재계산을 더 줄이지만 채널별 큰 버퍼와 역방향 슬롯 조회, 추가 bandwidth가 필요하다. 측정 후 검토한다. 이 방안은 RawOutgoing 재사용에 대한 추가 67% 감소가 아니라 기존 24회 대비 총 67% 감소다.
- base Geometry를 고정해 캐시: ADR 0003이 정한 적층 형상의 다음 Simulation 반영을 누락하므로 채택하지 않는다.

## Consequences

- steady-state에서 중첩 평균 거리 순회를 제거하고 Pass 2의 rawFlux 호출 상한을 16회에서 8회로 줄인다.
- 인스턴스마다 메모리가 늘며 캐시 무효화와 GPU 접근 동기화가 필요하다.
- 채널 지원/StateCapacity/전달률 변경은 TransferWeight 자체를 바꾸지 않는다. 지원 여부는 rawFlux 경계에서 검사한다. Profile ID 배치 변경은 캐시를 갱신한다.
- 순수 translation은 거리·법선 가중치를 바꾸지 않아 재생성이 필요 없다. 초기 구현은 모든 transform 변경을 dirty로 처리하는 보수적인 정책도 허용하며 구현 문서에 기록한다.
- 현재 MesoVirtualHeight와 향후 AccumulationHeight 변경은 유효 표면 형상을 바꾸므로 TransferWeight 캐시를 무효화한다. 중력 변경은 TransferWeight에는 영향을 주지 않으며 GeometryDrive에서 최신 값을 사용한다.
- Geometry 생성 경로가 제공하는 유효 normal을 사용한다. 형상 갱신과 normal 산출 방식은 동적 Geometry 계약을 따른다. 다음 Solver가 base normal로 되돌아가지 않게 준비 단계의 입력 revision을 연결한다.
- 최적화 전후 상태·유입·유출·alpha를 허용 오차 안에서 비교하고 동일 환경의 GPU 시간을 반복 측정한다.

## Related

- [[0016-Transport-Transfer-Weights|ADR 0016 — TransferWeight 계산식]]
- [[0015-Geometry-Driven-Transport|ADR 0015 — GeometryDrive]]
- [[0003-Dynamic-Accumulation-Geometry|ADR 0003 — Dynamic Accumulation Geometry]]
- [[../04_Architecture/0007_Surface-Solver-Cache|Surface Solver Cache]]
- [[../03_Planning/02_Weekly-Details/Week-05/0002_01_Branch-Solver-Transfer-Cache|Branch 2.1 — Solver Transfer Cache]]
