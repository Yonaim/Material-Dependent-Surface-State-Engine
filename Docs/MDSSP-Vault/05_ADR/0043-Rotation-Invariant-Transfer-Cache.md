# ADR 0043 — 순수 회전에 불변인 TransferWeight 캐시

> **한 줄 요약:** 정적 형상과 고정 크기의 순수 회전은 TransferWeight·월드 텍셀 면적을 바꾸지 않으므로 CPU 재계산과 GPU 버퍼 갱신을 생략한다.

- 분류: **Simulation**
- Status: **Accepted — 구현 전**
- Date: 2026-10-01
- 관련 문서: [[0015-Geometry-Driven-Transport|ADR 0015 — GeometryDrive]], [[0017-Solver-Transfer-Cache|ADR 0017 — TransferWeight 캐시]], [[0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034 — 시간 간격]]

## Context

현재 캐시 dirty 판정은 instance model 행렬의 3×3 선형 성분을 비교한다. 회전이 조금만 변해도 Graphics queue idle을 기다린 다음 CPU가 전체 TransferWeight와 월드 텍셀 면적을 재계산하고 기존 host-visible GPU buffer에 기록한다. 버퍼를 새로 할당하는 경로는 아니다.

TransferWeight는 이웃 거리 비율, 법선 내적, 선택적 mesh-local 곡률 감쇠, Profile 경계의 곱이다. 같은 정적 형상에 동일한 크기를 적용한 뒤 강체 회전하면 거리·법선 내적·곡률·면적은 유지된다. 반면 이웃 간 월드 높이 차와 표면에 투영한 중력 방향은 회전할 때 변하며, 이미 Solver가 매 step 현재 model 행렬과 World Gravity로 GPU에서 계산한다.

## Decision

1. 정적 Geometry, Profile ID 배치, TransferWeight 설정과 크기가 같으면 순수 translation·rotation은 host TransferWeight cache 및 월드 텍셀 면적 buffer를 무효화하지 않는다. 현재 TTransform의 회전과 크기를 분리해 판정한다. 일반 선형 변환으로 확장할 때는 회전에 불변인 길이 metric을 사용한다.
2. 크기 또는 형상 변경, 이웃·Profile ID 배치 변경, Weight 규칙·토글 변경은 해당 캐시를 갱신한다. 적층 feedback으로 유효 형상이 변하면 기존 동적 Geometry 경로가 다음 Solver step의 가중치를 갱신한다.
3. GPU Solver에는 최신 model 선형 행렬, normal matrix와 World Gravity를 매 step 전달한다. GeometryDrive의 월드 높이 차와 중력 방향을 정적 TransferWeight buffer에 bake하지 않는다.
4. GPU queue 대기는 실제 host buffer 갱신이 필요한 경우에만 수행한다. 회전만으로는 CPU의 BuildSurfaceGPUTransferWeights·BuildSurfaceGPUWorldTexelAreas, buffer Upload 및 해당 queue idle을 호출하지 않는다.
5. Auto substepping의 CPU 허용 시간 간격 계산은 별개의 문제다. 현재 식에는 회전별 월드 높이 차가 있으므로 이 ADR의 캐시 판정 변경만으로 시간 상한의 정확성이 보장되지는 않는다. 기본 OFF인 Auto 모드의 정책은 별도 검증한다.

## Alternatives Considered

- **모든 3×3 transform 변경에 캐시 재생성:** 현재 구현이다. 안전하지만 회전 불변 데이터까지 다시 계산·업로드하고 GPU를 기다린다.
- **중력 방향 값을 각도별 사전 계산해 TransferWeight 갱신에 사용:** 중력값은 TransferWeight의 입력이 아니며, 각도별 값을 같은 GPU buffer에 다시 쓰면 동기화 문제가 남는다. 별도 성능 실험은 [[../03_Planning/03_Future-Plans/0001_Angle-Sampled-Gravity-Cache|향후 검토]]에 둔다.
- **실제 형상·크기·가중치 의존성만 dirty 판정:** 채택한다.

## Consequences

- 순수 회전 프레임에서 host 캐시 재생성·buffer Upload·이를 위한 queue idle이 사라진다. 실제 절감 시간은 GPU 대기·CPU 계산·Upload를 따로 계측해야 한다.
- 회전해도 TransferWeight와 텍셀 면적이 같다는 결과를 균일·비균일 고정 크기, 여러 법선과 CurvatureWeight ON/OFF에서 비교한다. 동시에 GeometryDrive의 중력 이동 방향은 회전에 따라 변해야 한다.
- 크기 또는 동적 형상이 달라지는 경우의 buffer 갱신과 동기화는 유지한다. Auto substepping 상한을 회전 중 사용할 때는 이전 방향의 값만 그대로 재사용하지 않는다.
- ADR 0017의 초기 구현이 허용한 모든 선형 transform 변경 dirty 정책을 순수 회전에 한해 대체한다. ADR 0015의 instance별 GPU 중력 계산 계약은 유지한다.

## Related

- [[0015-Geometry-Driven-Transport|ADR 0015 — GeometryDrive]]
- [[0016-Transport-Transfer-Weights|ADR 0016 — TransferWeight]]
- [[0017-Solver-Transfer-Cache|ADR 0017 — 캐시 수명]]
- [[0034-Fixed-Timestep-and-Auto-Substepping|ADR 0034 — 시간 간격]]
- [[../03_Planning/03_Future-Plans/0001_Angle-Sampled-Gravity-Cache|각도별 중력값 캐시 후속 검토]]
