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

## 운영 원칙

- 뒤 브랜치를 미리 만들지 않는다. 앞 브랜치를 `main`에 병합한 뒤 다음 브랜치를 만든다.
- 브랜치 하나는 독립적으로 빌드되고, 최소 하나의 확인 가능한 결과를 남겨야 한다.
- 자료구조 변경은 가능한 한 1번 브랜치에서 끝낸다. 이후 브랜치에서 계약을 바꿔야 하면 먼저 이유를 기록한다.
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

- [[02_Planning/04_Weekly-Details/Week-04/0001_Branch-Surface-Data-Contract|1. Surface Data Contract]]
- [[02_Planning/04_Weekly-Details/Week-04/0002_Branch-Simulation-Mapping|2. Simulation Mapping]]
- [[02_Planning/04_Weekly-Details/Week-04/0003_Branch-Shared-Geometry-Build|3. Shared Geometry Build]]
- [[02_Planning/04_Weekly-Details/Week-04/0004_Branch-Surface-GPU-Resources|4. Surface GPU Resources]]
- [[02_Planning/04_Weekly-Details/Week-04/0005_Branch-Surface-Solver-2Pass|5. Surface Solver 2-Pass]]
- [[02_Planning/04_Weekly-Details/Week-04/0006_Branch-Surface-Input-Integration|6. Surface Input Integration]]
