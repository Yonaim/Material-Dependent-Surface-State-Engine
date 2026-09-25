# Surface Mapping 테스트 사례

상태: **통과** · 대상 브랜치: `feat/simulation-mapping`

OBJ 원본 topology 보존, UV rasterization, chart-local 이웃과 seam 연결 결과를 검증한다. 공통 원칙은 [[0000_Testing-Guide|Testing Guide]]를 따른다.

## 테스트 함수 구성

| 테스트 함수 | 입력 방식 | 검증 범위 |
|---|---|---|
| `TestOBJTopologyPreservation` | UV seam OBJ fixture | render vertex 분리 이후에도 원본 position·UV index와 Surface ID 보존 |
| `TestSingleTriangleRasterization` | 단일 triangle fixture | Valid texel 생성과 barycentric 범위 |
| `TestRegularChartNeighbors` | seam 없는 quad fixture | 공유 UV edge와 chart-local neighbor |
| `TestSeamNeighbors` | UV seam quad fixture | 서로 다른 chart 사이의 양방향 seam neighbor |
| `TestDisconnectedTopology` | 분리된 두 quad fixture | UV와 공간상 가까운 별도 topology의 연결 방지 |
| `TestDeterministicResult` | 동일 fixture를 두 번 변환 | Triangle, Chart, barycentric과 neighbor 결과 일치 |
| `TestMultipleSurfaceRanges` | C++ 입력 직접 구성 | 서로 다른 해상도의 연속 Surface texel range |
| `TestInvalidFixtures` | 오류 OBJ fixture | UV·overlap·non-manifold 입력 거부 |
| `TestInvariantValidation` | 정상 결과를 의도적으로 훼손 | 단방향 neighbor 탐지 |

## 정상 fixture

| 파일 | 목적 | 기대 결과 |
|---|---|---|
| `SingleTriangle.obj` | 기본 rasterization | 한 개 이상의 Valid texel과 정상 barycentric 생성 |
| `QuadNoSeam.obj` | 같은 chart의 triangle 두 개 | 공유 edge를 가로지르는 regular neighbor 생성 |
| `QuadSeam.obj` | 같은 원본 edge, 서로 다른 UV edge | 원본 position topology를 이용한 seam neighbor 생성 |
| `DisconnectedQuads.obj` | 공간상 가깝지만 원본 position이 다른 두 quad | 서로 다른 chart 사이 neighbor 없음 |

## 오류 fixture

| 파일 | 오류 조건 | 기대 결과 |
|---|---|---|
| `DegenerateUV.obj` | UV triangle 면적 0 | mapping 생성 거부 |
| `Overlap.obj` | 서로 다른 triangle이 같은 UV 영역 점유 | 소유 Surface·Triangle·texel을 포함한 overlap 오류 |
| `NonManifold.obj` | 하나의 원본 edge에 triangle 세 개 연결 | non-manifold 오류 |
| `MissingUV.obj` | OBJ face에 `vt` index 없음 | Simulation UV 누락 오류 |

## 실행 및 완료 확인

| 항목 | 기준 |
|---|---|
| 테스트 target | `MDSS_SurfaceMappingTests` |
| CTest 이름 | `MDSS_SurfaceMapping` |
| 실행 명령 | `ctest --test-dir <build-dir> --output-on-failure` |
| GPU 의존성 | Vulkan device나 GPU resource 초기화 없음 |
| 완료 기준 | 정상·오류 fixture와 mapping 불변조건 검증이 모두 통과 |

GPU buffer upload, descriptor와 Solver 연결은 후속 브랜치에서 다룬다.
