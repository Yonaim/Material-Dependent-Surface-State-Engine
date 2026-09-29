# ADR 0018 — Normal Map 기반 Virtual Meso Geometry 복원

> **한 줄 요약:** Normal Map의 slope를 texel graph에서 적분해 Virtual Height와 곡률 파생값을 생성한다.

- 분류: **Simulation**
- 상태: **채택**
- 날짜: 2026-09-27
- 관련 문서: [[0019-Optional-Curvature-Transfer-Weight|ADR 0019 — CurvatureWeight]]
- 범위: Week-05 Branch 2.3

## Context

Normal Map의 tangent-space 방향을 `NormalWeight`에 직접 사용하는 것만으로는 GeometryDrive의 높이차와 실제 형상 거리, cavity retention, 높이 기반 디버그 표시를 만들 수 없다. Normal Map에는 noise나 bake 오차가 있을 수 있어, 국소 기울기를 단일 height field로 정확히 적분할 수 있다는 보장도 없다.

## Decision

1. 각 Simulation texel은 자신의 barycentric UV로 Normal Map을 sample한다. mesh-local로 변환된 texel normal과 기존 Simulation neighbor graph가 적분 입력이다. UV seam은 mapping graph가 연결한 이웃 쌍에 한해 높이 field를 공유한다.
2. 이웃 `i → j`의 위치 차이를 각 endpoint의 macro tangent plane에 투영한다. `n_i`는 macro normal, `m_i`는 mesh-local sampled Normal Map normal, `t_ij`는 투영된 단위 이웃 방향이다.

   ```text
   slope_i(i→j) = -dot(m_i, t_ij) / dot(m_i, n_i)
   d_ij = 0.5 × (slope_i × length_i + slope_j × length_j)
   ```

   방향이 반대인 `j → i`에는 반대 부호의 height delta를 사용한다. 이웃 길이는 mesh-local 길이에서 오므로 texel UV 크기나 Simulation resolution에 종속되지 않는다. 별도 `β_meso`는 곱하지 않는다.
3. 연결 graph에서 다음 목적함수를 최소화한다.

   ```text
   minimize Σ_edges ((h_j - h_i) - d_ij)²
   ```

   component별 한 texel을 내부 gauge로 pin해 Laplacian의 상수 nullspace를 제거한다. 해를 구한 뒤 각 component의 평균 높이를 0으로 이동해, cavity depth가 임의의 pinned texel에 좌우되지 않도록 한다. 끊긴 chart는 각각 별도 기준을 가진다.
4. CPU 전처리는 Jacobi preconditioned conjugate gradient(PCG)를 사용한다. 최대 128회 반복하고 상대 residual 제곱 기준 `1e-8` 이하에서 멈춘다. 최대 반복 뒤에도 남은 오차는 가장 가까운 least-squares height field로 둔다. 별도 임계값으로 Surface를 거부하지 않으며, 방향별 edge mismatch의 relative RMS residual을 AssetManager 로그에 기록한다.
5. invalid sample, `dot(m,n) ≤ 0.05`, 유한하지 않은 입력은 적분 active set에서 제외하고 해당 texel 높이를 0으로 둔다. 이웃 데이터가 부족하거나 곡면 fit이 특이한 texel은 curvature를 0으로 두고, normal은 sampled Normal Map normal로 fallback한다.
6. 적분 높이의 local tangent 좌표 이웃을 weighted quadratic least-squares fit해 `MesoNormal`, signed mean curvature `H`, Gaussian curvature `K`를 계산한다. `H` 단위는 1/mesh-local length, `K`는 1/(mesh-local length²)다. `ConcavityWeight = clamp(H × mean projected neighbor spacing, 0, 1)`로 두며 양의 H를 bowl/cavity, 평탄·볼록을 0으로 취급한다. Gaussian curvature는 형상 분석 데이터로 보존하되 현재 Decay 입력이나 Transport에 직접 연결하지 않는다.
7. Curvature와 Concavity는 별도 값이다. `CurvatureWeight`의 기본값은 1.0으로 유지한다. 초기 구현은 고정값만 제공했으며 현재 선택적 비교 모드는 [[0019-Optional-Curvature-Transfer-Weight|ADR 0019]]를 따른다. NormalWeight가 유효 normal 차이를 반영하므로 곡률 전달 감쇠를 추가하면 형상 방향 효과를 중복할 수 있다. ConcavityWeight만 Decay의 cavity retention에 사용한다.
8. shared CPU Geometry는 높이, mean/Gaussian curvature, ConcavityWeight와 MesoNormal을 보유한다. GPU GeometryScalar는 네 float(16 B)로 확장되고, MesoNormal은 별도 `vec4` buffer(binding 17)에 저장한다. TransferWeight cache의 NormalWeight 입력은 MesoNormal, fallback으로 TransferNormal, 최종 fallback으로 macro Normal을 사용한다. height는 기존 GeometryDrive 및 DistanceWeight 경로에 즉시 반영된다. DirectionDrive도 기본 ON에서 binding 17의 MesoNormal을 inverse-transpose로 변환해 사용한다. 비교용 UI `DirectionDrive: MesoNormal`을 OFF로 두면 macro normal을 사용한다.
9. Virtual Height 디버그 모드는 부호가 다른 값을 파랑/짙은 중립/주황색으로 표시한다. 초기 Virtual Height Offset은 원본 render vertex UV의 Simulation texel 높이를 읽어 변위했고 메시 정점 밀도로 세부가 제한되었다. 현재 Meso Color/Offset은 [[0036-Texel-Geometry-Preview|ADR 0036]]의 texel 연결면을 사용한다. State Heatmap의 relief shading은 `MesoNormal`을 사용하며 Heatmap 팔레트는 유지한다.

## Alternatives Considered

| 대안 | 장점 | 선택하지 않은 이유 |
|---|---|---|
| 경로 누적 적분 | 구현이 단순하고 빠름 | 폐곡선 drift와 경로 의존성이 생긴다. |
| UV-space `-nx/nz`, `-ny/nz` 후 height scale | 평면 grid에서 간단 | UV metric과 mesh scale을 따로 추적해야 하고, seam·왜곡 UV에서 물리 길이와 어긋난다. |
| 정규화 근사 높이 | 비용이 작고 실패 입력도 표현 가능 | mesh 길이 단위와 height 의미를 보장하지 않는다. |
| 잔차가 큰 Surface 전체 거부 | 잘못 복원된 형상을 숨길 수 있음 | 현재 normal map에 적합한 임계값의 근거가 없다. residual을 관찰하고 least-squares 결과를 유지한다. |
| Mean curvature만 저장 | Decay에 필요한 간단한 scalar | Gaussian curvature는 안장형/국소 형상 구분에 별도 정보를 주므로 형상 데이터로 함께 보존한다. 다만 당장 Solver에 연결하지 않는다. |

## Consequences

- Normal Map/UV/tangent/mesh 변경은 shared Virtual Meso Geometry와 instance별 TransferWeight cache를 무효화한다.
- GPU 공유 형상 payload는 texel당 기존 80 B에서 104 B가 된다. 추가분은 mean/Gaussian curvature를 포함한 GeometryScalar 8 B와 MesoNormal 16 B다.
- Virtual Height를 이용한 displacement는 기존 render mesh 정점을 옮기는 경로다. 조밀한 texel relief를 재현하려면 render mesh도 충분히 세밀하거나 후속 tessellation/displacement 경로가 필요하다.
- 매핑 graph의 UV seam 연결이 없거나 normal sample이 불안정한 영역은 높이 0 / fallback normal로 나타난다. 해당 상태와 relative edge residual은 전처리 로그에서 확인한다.

## Validation Evidence

- 평탄 Normal Map은 0 높이, macro와 일치하는 MesoNormal, 0 curvature/concavity를 만들어야 한다.
- 적분 가능한 ramp / bowl / dome fixture는 높이 기울기와 부호, mean/Gaussian curvature 정의를 확인해야 한다.
- noisy/non-integrable fixture는 finite least-squares 높이와 상대 edge residual을 확인해야 한다.
- UV seam neighbor와 disconnected chart는 연결/비연결 기준이 유지되는지 검사해야 한다.
- GPU pack의 크기·offset, shader vertex/fragment descriptor binding, Virtual Height/Offset 및 State Heatmap relief 표시를 확인해야 한다.
