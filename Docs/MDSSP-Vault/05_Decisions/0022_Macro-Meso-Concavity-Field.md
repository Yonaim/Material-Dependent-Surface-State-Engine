# Decision 0022 — Macro Mesh와 Normal Map을 반영한 텍셀 오목도

> **한 줄 요약:** Mesh와 Normal Map에서 계산한 오목도를 Decay와 방향별 이동 억제에서 함께 사용한다.

- 분류: **Simulation**
- Status: **Implemented**
- Date: 2026-10-01
- 관련 문서: [[05_Decisions/0003_Normal-Map-Meso-Geometry|Decision 0003 — Normal Map Meso Geometry]], [[03_Architecture/0004_Surface-Geometry|Surface Geometry]], [[05_Decisions/0023_Directional-Cavity-Transport-Retention|Decision 0023 — 홈 이탈 억제]]

## 쉽게 읽기

Mesh의 큰 굴곡과 Normal Map의 작은 굴곡을 합친 표면에서 오목도를 계산한다. 같은 오목도 값을 자연 감소와 방향별 이동 억제에 제공한다.

## Context — 왜 필요했나

이 결정을 내리기 전에는 TSurfaceGeometryScalar가 texel마다 MesoVirtualHeight, ConcavityWeight, MesoMeanCurvature, MesoGaussianCurvature를 보유한다. 그러나 ConcavityWeight의 생성은 유효한 Normal Map transfer normal이 있는 texel만 대상으로 하며, 복원한 MesoVirtualHeight를 국소 이차 곡면에 맞춘 signed mean curvature H에 이웃 간격을 곱해 0–1로 제한한다. Gaussian curvature K는 저장하지만 오목도 판정에는 쓰지 않는다. Normal Map이 없는 Bunny·Mountain의 Macro Mesh 굴곡은 이 값에 들어가지 않아 Decay의 cavity retention도 그 굴곡을 보지 못한다.

또한 H의 절댓값을 쓰는 선택적 CurvatureWeight는 오목함·볼록함을 구분하지 않는 대칭적 이동 감쇠다. 홈에서 물질이 머무는 효과에 사용할 signed concavity와는 역할이 다르다.

## Decision — 무엇을 정했나

1. 별도 2D 리소스를 기본으로 추가하지 않는다. 기존 Surface별 simulation UV 텍셀 배열과 GPU GeometryScalar의 ConcavityWeight를 통합 오목도 입력으로 사용한다. 유효 texel은 Normal Map 유무와 관계없이 Macro Mesh 위치·법선으로 오목도를 평가한다.
2. Normal Map이 있으면 복원된 Virtual Meso 형상을 Macro Mesh 위에 한 번 합친 유효 위치·방향에서 오목도를 도출한다. Macro와 Meso의 평균곡률을 단순 합산하지 않는다. Normal Map이 없는 경우에는 Macro 형상만으로 0이 아닌 오목도를 만들 수 있어야 한다.
3. signed mean curvature H와 Gaussian curvature K를 함께 검토하고, 필요한 주곡률은 H±sqrt(max(H²−K, 0))에서 유도한다. 길이 단위가 다른 곡률을 직접 비교하지 않고 H·주곡률에는 local 이웃 간격, K에는 그 간격의 제곱을 사용해 무차원으로 만든다. 그릇·홈·볼록부·안장형을 구분한 뒤 최종 ConcavityWeight를 유한한 0–1 값으로 제한한다. 분류 경계와 gain은 fixture 및 실제 Mesh 결과를 보고 확정한다.
4. 기존 MesoMeanCurvature·MesoGaussianCurvature의 Meso 전용 의미는 유지한다. 통합 오목도용 곡률은 별도 전처리 중간값으로 계산한다. 당시에는 선택적 절댓값 CurvatureWeight를 그대로 두었지만, 해당 항은 후속 결정 [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024]]에서 제거했다.
5. Decay는 현재처럼 texel의 ConcavityWeight와 .SRProfile의 cavityDecayProtectionFactor를 사용한다. Transport는 같은 오목도 필드를 사용하되 독립 계수와 방향 규칙을 Decision 0023에서 정한다.
6. 동적 적층 형상 갱신이 활성화되어 홈의 유효 형상이 바뀌면 오목도도 다음 Simulation에 맞게 갱신해야 한다. 기존 SurfaceDynamicWeightsUpdate.comp의 unsigned bend 값은 signed ConcavityWeight의 갱신을 대신하지 않는다. 정적 통합 전처리와 동적 갱신은 각각 검증한다.

## Implementation — 현재 구현

`BuildMesoGeometry`의 최종 단계에서 Macro 위치·법선과 복원한 Meso 높이·법선을 결합한 유효 표면의 이웃 법선 변화로 H·K 및 주곡률을 구한다. 이웃 간격을 곱한 양의 주곡률 합에서 음의 주곡률 합의 두 배를 뺀 뒤 gain 8과 `[0,1]` clamp를 적용한다. 기존 Meso 전용 H·K 필드는 그대로 둔다. `.Surface` cache의 preprocessing version은 현재 4다. 형상 생성 규칙을 바꿀 때 이 버전을 갱신한다. 적층 형상 갱신이 켜지면 `SurfaceDynamicWeightsUpdate.comp`가 변경된 texel별 signed 오목도를 계산해 instance별 float32 cache에 기록하고 Solver가 이를 읽는다. 정적 cache 값은 갱신 불가한 동적 이웃 추정의 fallback이다. 평면·그릇·긴 홈·돔·안장형 합성 Mesh 검사를 통과했다.

## Alternatives Considered — 다른 방법

- **Normal Map Meso 오목도만 유지:** 기존 구현이지만 노멀 맵 없는 Macro 홈에 대한 Decay·Transport 보정이 없다.
- **Macro와 Meso의 H 값을 단순 더하기:** 구현은 짧지만 서로 다른 국소 접평면과 샘플 간격에서 얻은 곡률의 부호·크기를 일관되게 결합하기 어렵다.
- **기존 텍셀 필드에 결합된 유효 형상의 오목도 기록:** 채택한다. 다른 소비자가 같은 형상 의미를 읽는다.

## Consequences — 결정의 영향

- Bunny·Mountain처럼 Normal Map이 없는 Mesh에서도 Decay의 오목함 보정이 나타날 수 있다. 실제 세기는 Profile의 decayRate·cavityDecayProtectionFactor와 새 오목도 분포에 따라 달라진다.
- 기존 .Surface 전처리 캐시는 오목도 생성 알고리즘 변경 시 fingerprint 또는 버전을 갱신해 다시 만들어야 한다. GPU GeometryScalar의 기존 ConcavityWeight 슬롯은 유지한다.
- Normal Map이 있는 BrickCube와 없는 Bunny·Mountain에서 동일한 부호 계약을 확인한다. 평면, 오목한 그릇, 한 방향 홈, 볼록한 돔, 안장형, UV seam 및 서로 다른 해상도를 검증한다.
- 동적 cache는 texel별 float32 한 개이며 instance 소유다. Surface 6개 × 512×512, 추가 원소 padding 없는 가정에서는 instance당 6 MiB다. cache는 dynamic geometry pass가 변경 texel과 그 이웃에 대해 다시 기록하며, 그 뒤 compute write→read barrier로 Solver에 공개된다.
- Meso 전용 ConcavityWeight 생성 규칙을 통합 필드의 최종 생성 규칙으로 대체한다. Virtual Meso Height 복원과 MesoMean/Gaussian 데이터 자체는 유지한다.

## Related — 관련 문서

- [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[05_Decisions/0003_Normal-Map-Meso-Geometry|Decision 0003 — Meso 전처리]]
- [[05_Decisions/0024_Transport-Role-Names-and-Curvature-Removal|Decision 0024 — 곡률 감쇠 제거]]
- [[05_Decisions/0023_Directional-Cavity-Transport-Retention|Decision 0023 — 방향별 홈 보유]]
- [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
