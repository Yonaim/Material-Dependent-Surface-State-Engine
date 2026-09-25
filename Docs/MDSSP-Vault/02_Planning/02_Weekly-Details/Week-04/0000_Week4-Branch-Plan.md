# 4주차 구현 브랜치 계획

상태: **임시 작업 계획 / 커밋하지 않음**

이 문서들은 4주차 구현을 작은 검증 단위로 나누기 위한 임시 상세 계획이다. 공식 설계의 기준은 `03_Architecture/`와 `05_Development/Notes/` 문서다.

## 브랜치 순서

```text
main
 └─ feat/surface-data-contract
     ↓ merge
 └─ feat/simulation-mapping
     ↓ merge
 └─ feat/shared-geometry-build
     ↓ merge
 └─ feat/surface-gpu-resources
     ↓ merge
 └─ feat/surface-solver-2pass
     ↓ merge
 └─ feat/surface-input-integration
```

| 순서 | 브랜치 | 결과물 |
|---:|---|---|
| 1 | `feat/surface-data-contract` | CPU 자료형, 소유권, Profile/Surface 연결 계약 |
| 2 | `feat/simulation-mapping` | UV rasterization, ValidMask, neighbor, seam mapping |
| 3 | `feat/shared-geometry-build` | Solver가 읽을 정적 Geometry field |
| 4 | `feat/surface-gpu-resources` | Shared/Instance SSBO와 descriptor |
| 5 | `feat/surface-solver-2pass` | Pass 1/2, barrier, ping-pong |
| 6 | `feat/surface-input-integration` | Raycast부터 State 디버그 표시까지의 연결 |

## 추가 설계 결정의 후속 작업 배정

| 결정·미완료 작업 | 담당 브랜치 | 범위 |
|---|---|---|
| Profile Distribution authoring 형식 및 로더 | `feat/shared-geometry-build` | 입력 형식 선택, 파싱·검증, UV texel별 `ProfileIndex` 생성 |
| `.Surface` cache miss/stale 처리의 end-to-end 연결 | `feat/shared-geometry-build` | Scene/Asset 경로에서 cache 확인, 필요 시 Mapping→Build→Save, 결과 등록. Binary v1 Save/Load 자체는 `feat/simulation-mapping`에 구현됨 |
| Registry 크기를 State instance와 GPU 리소스에 전달 | `feat/surface-gpu-resources` | Registry 수명/참조와 channel count를 instance 생성 및 resource 크기에 연결하고 dynamic layout 결정 |
| 임의 개수 State를 처리하는 Solver | `feat/surface-solver-2pass` | 하드코딩된 State 이름·개수 제거, Registry channel count로 처리 및 테스트 |
| Profile에 정의되지 않은 Registry State의 처리 규칙 | `feat/surface-solver-2pass` 및 `feat/surface-input-integration` | 지원 여부 표현은 Branch 4가 제공하고, Solver/Contact에서의 동작을 각각 정해 테스트 |
| Contact 입력의 State 선택 및 UI | `feat/surface-input-integration` | `StateId`를 `ChannelIndex`로 해석해 입력을 기록하고 UI를 Registry에서 구성 |
| Normal Map으로 Meso/Curvature 생성하는 알고리즘 | Week-08 experiment 이후 별도 구현 branch 결정 | Week-08에 후보와 품질·비용을 비교하고 승인된 방법만 후속 계획에 배정. 4주차는 기본값 0 유지 |

## 운영 원칙

- 뒤 브랜치를 미리 만들지 않는다. 앞 브랜치를 `main`에 병합한 뒤 다음 브랜치를 만든다.
- 브랜치 하나는 독립적으로 빌드되고, 최소 하나의 확인 가능한 결과를 남겨야 한다.
- 자료구조 계약은 Registry 기반 dynamic State를 기준으로 한다. GPU의 구체적인 channel memory layout은 4번 브랜치에서 결정하며, 이후 변경 시 ADR와 영향 문서를 갱신한다.
- GPU 단계 전까지 CPU 결과를 충분히 검증한다. GPU에서 mapping 오류와 solver 오류를 동시에 디버깅하지 않는다.
- 자동 UV unwrap, 완전한 Normal Map 적분, 동적 Accumulation geometry는 4주차 최소 완료 조건에서 제외한다.

## 4주차 최소 완료 조건

준비된 테스트 OBJ 하나에서 다음 흐름이 끝까지 동작하면 최소 성공으로 본다.

```text
OBJ UV
→ Mesh-to-Texel Mapping
→ Shared Geometry
→ State A/B + TempAlpha
→ 2-Pass Solver
→ Contact Input
→ Debug Visualization
```

## 공통 검증 명령

각 브랜치에서 최소한 다음을 실행한다.

```bash
cmake --build Build
```

CPU test target을 추가한 뒤에는 다음도 실행한다.

```bash
ctest --test-dir Build --output-on-failure
```

Vulkan 관련 브랜치는 validation layer를 켠 Debug build로 한 번 이상 실행한다.

## 문서 목록

- [[02_Planning/02_Weekly-Details/Week-04/0001_Branch-Surface-Data-Contract|1. Surface Data Contract]]
- [[02_Planning/02_Weekly-Details/Week-04/0002_Branch-Simulation-Mapping|2. Simulation Mapping]]
- [[02_Planning/02_Weekly-Details/Week-04/0003_Branch-Shared-Geometry-Build|3. Shared Geometry Build]]
- [[02_Planning/02_Weekly-Details/Week-04/0004_Branch-Surface-GPU-Resources|4. Surface GPU Resources]]
- [[02_Planning/02_Weekly-Details/Week-04/0005_Branch-Surface-Solver-2Pass|5. Surface Solver 2-Pass]]
- [[02_Planning/02_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|6. Surface Input Integration]]
