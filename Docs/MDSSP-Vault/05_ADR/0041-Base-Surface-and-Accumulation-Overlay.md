# ADR 0041 — 원본 표면과 적층 Overlay 분리 렌더링

> **한 줄 요약:** 원본 Mesh를 바닥으로 유지하고 기존 texel 연결면을 적층의 윗면으로 재사용하며, 적층 영역의 경계에 옆면을 생성한다.

- 분류: **Rendering**
- 상태: **Accepted (1차 렌더 경로 코드 반영, 실행 검증 대기)**
- 날짜: 2026-09-30
- 관련 문서: [[0037-Texel-Grid-and-Demo-Lit-Effects|ADR 0037 — 데모 Lit]], [[0038-Source-Topology-Seam-Stitching|ADR 0038 — texel 연결면]], [[0039-State-Thickness-Per-Amount|ADR 0039 — State별 적층 두께]], [[0003-Dynamic-Accumulation-Geometry|ADR 0003 — 동적 적층 형상]]

## Context

현재 Lit 경로는 원본 triangle을 texel 중심으로 세분한 연결면을 만든 뒤, 계산된 높이만큼 그 연결면을 변위해 표면처럼 그린다. 원본 Mesh를 같은 위치의 바닥으로 함께 그리지 않는다. Mud는 불투명한 피복이라 이 방식의 한계가 덜 드러나지만, WaterFilm의 아래쪽 원본 표면까지 변위된 것처럼 보이면 투명한 층의 두께를 자연스럽게 표현하기 어렵다. 현재 `WaterFilm.glsl`도 표면의 색과 roughness를 바꾸는 데모 효과이며 실제 투명 적층 패스는 아니다.

이미 생성하는 texel 연결면은 원본 topology·UV seam을 보존하고 중앙 texel의 높이를 실루엣에 반영한다 ([[0038-Source-Topology-Seam-Stitching|ADR 0038]]). 이 형상을 적층 윗면으로 재사용할 수 있다. State별 Capacity 제한량, cavity 배분과 `thicknessPerAmount`를 이용한 높이 계산 및 다음 Solver step의 동적 형상 피드백은 별도의 시뮬레이션 계약이다 ([[0039-State-Thickness-Per-Amount|ADR 0039]], [[0003-Dynamic-Accumulation-Geometry|ADR 0003]]).

## Decision

1. **Base Surface:** 원본 Mesh를 원래 위치와 Render Material로 렌더링한다. `Wetness`처럼 별도 두께가 없는 외관 반응은 이 패스에 적용한다. Base Surface의 정점을 적층 높이만큼 올리지 않는다.
2. **Overlay 윗면:** 현재 texel 연결면의 정적 topology와 인스턴스별 GPU 높이 출력을 적층 윗면에 재사용한다. 해당 State를 지원하는 Surface에 적층 패스를 제출하고, State가 존재하는 영역만 보이게 한다. 이 변경은 렌더링용 draw 경로이며 Solver의 높이 수식·State·형상 피드백을 수정하지 않는다.
3. **Overlay 옆면:** 적층이 있는 영역과 없는 영역 사이의 경계에 옆면을 만든다. Mesh의 바깥쪽 경계뿐 아니라 Surface 한가운데 생긴 Mud 섬의 둘레도 포함한다. 경계의 각 위치에는 원본 표면의 아래 점과 적층 윗면의 위 점이 있다. 이웃한 경계 위치들의 두 점 쌍을 이어 삼각형 두 개로 옆면 한 조각을 만든다. 따라서 모든 내부 texel 정점을 두 배로 만들 필요는 없다. 윗면과 옆면의 법선이 다르면 같은 위치의 렌더 정점 속성을 분리한다.
4. **같은 경계 사용:** 윗면에서 적층이 끝나는 위치와 옆면의 위쪽 경계는 같은 State/coverage 기준과 높이 샘플을 사용한다. 현재 연결면의 삼각형 내부에서 경계가 지나가면 두 결과가 같은 교차점을 사용하도록 윗면을 잘라 낸다. 윗면은 fragment에서만 버리고 옆면은 별도의 대략적인 위치에 두는 방식은 경계 틈이나 겹침을 만들 수 있다. 첫 구현은 텍셀 해상도의 경계여도 되며, 부드러운 윤곽 보간은 후속 품질 개선으로 둔다.
5. **바닥면:** Overlay 전용 바닥 삼각형은 만들지 않는다. 원본 Mesh가 보이는 바닥이다. 바닥 삼각형을 같은 위치에 다시 그리면 중복된 depth 면이 된다. 이 결정은 독립된 폐합 체적 메시를 만드는 요구를 포함하지 않는다.
6. **State별 재질:** Mud와 WaterFilm은 위의 형상 경로를 공유하지만 재질·패스는 구분한다. Mud는 피복된 부분을 불투명하게 그리고 depth를 기록한다. WaterFilm은 Base와 불투명 적층 뒤에 투명 패스로 그려 아래 표면을 보이게 한다. 현재의 `waterfilm`은 로드된 `.SRProfile`의 `states`에서 Registry가 구성하는 데모 State key이며 고정 채널 번호로 취급하지 않는다. `SurfaceWater`를 별도 State로 도입하는 결정은 이 ADR의 범위가 아니다.
7. **GPU 갱신:** State와 높이가 GPU에서 매 step 변하므로 적층 경계 판정과 옆면 형상은 인스턴스별 GPU 작업으로 갱신한다. 정적 texel 연결면과 원본 topology는 공유한다. 실제 생성 buffer, compact/indirect draw 방식과 갱신 빈도는 구현 단계에서 정한다. CPU가 매 프레임 Surface 전체를 다시 삼각분할하는 경로는 기본안으로 두지 않는다.

## 후속 전환 계획: Tessellation

첫 단계는 위의 texel 연결면과 동적 옆면으로 완성한다. 이후 시각 품질과 비용을 측정한 뒤, Overlay 윗면에 Tessellation을 적용해 생성된 정점의 UV에서 높이를 보간하는 경로로 전환한다. 이때 **원본 Mesh 자체를 Base 패스에서 올리는 것이 아니다**. 원본 Mesh의 해당 Surface topology를 Overlay 패스의 입력 patch로 재사용하거나 현재 Overlay topology를 세분화한다. Base 패스는 원래 위치에 남는다.

`Surface`를 선택한다는 것은 그 Surface에 적층 렌더링이 가능한지를 정한다는 뜻이다. 같은 Surface 안의 일부 texel에만 적층이 있다면, 그 부분의 윗면 경계와 옆면은 여전히 State 분포에서 결정해야 한다. Tessellation은 윗면 삼각형을 더 촘촘하게 만들지만 적층 영역의 경계나 옆면을 자동 생성하지 않는다. Overlay 윗면과 옆면은 같은 UV 위치와 높이 보간 규칙을 써야 한다. Tessellation의 patch 구성, State 높이 필드의 샘플링 자원 및 성능은 별도 구현 결정으로 남긴다.

## Alternatives Considered

- **단일 변위 연결면을 최종 표면으로 사용:** 현재 데모 경로다. 원본 표면이 적층 아래에 남지 않아 WaterFilm의 투명층 표현에 맞지 않는다.
- **윗면만 분리하고 옆면은 생략:** 원본 바닥은 제자리에 보이지만 Surface 가운데 생긴 Mud 피복의 두께가 측면에서 드러나지 않는다.
- **Overlay 바닥면까지 추가:** 원본 Mesh와 같은 위치의 렌더 면을 중복한다. 현재의 표면 적층 렌더링에는 필요하지 않다.
- **처음부터 Tessellation으로 윗면 생성:** 최종 품질 개선 후보지만 경계·옆면 생성과 Base/Overlay 분리 문제를 대신 해결하지는 않는다. 현재 texel 연결면을 활용한 첫 단계를 먼저 구현한다.

## Consequences

- 원본 Mesh와 적층 윗면을 각각 그리므로 적층이 증가해도 아래 표면은 원래 위치에 남는다. Mud와 WaterFilm은 공통 Overlay geometry 경로를 사용할 수 있다.
- 현재 Lit의 단일 texel 연결면 대체 draw 및 `WaterFilm.glsl`의 색·roughness 변경만으로는 이 결정을 구현한 것이 아니다. Base draw, Overlay 윗면·옆면, State별 depth/blend 및 실제 투명 재질 경로가 필요하다.
- 같은 텍셀에 Mud와 WaterFilm이 공존하면 층별 윗면 높이와 순서를 정의해야 한다. ADR 0039의 총 높이는 물리 재질별 layer 순서를 정하지 않으므로, 겹침 정책은 별도 결정으로 남긴다.
- Virtual Meso cavity가 원본 Macro Mesh 표면 아래에 있으면 Base의 depth가 그 안의 Overlay를 가릴 수 있다. 실제 기하 홈과 가상 홈을 구분해 렌더링 정책을 검토한다. 단순히 모든 Overlay를 앞으로 밀어 올리면 cavity 높이의 의미가 달라진다.
- 1차 경로는 기존 texel 연결면을 Overlay 윗면으로 그린다. Compute Shader가 연결 삼각형의 State 경계와 실제 열린 topology 경계에서 옆면 구간을 계산하고, Vertex Shader가 구간마다 삼각형 두 개를 그린다. 원본 Mesh는 별도 Base 패스로 그린다. MDSS 실행파일 빌드까지 확인했으며, 실행 화면·시각·성능 검증은 남아 있다.
- 렌더링 품질 실험 옵션 `Height-field smoothing`은 기본 OFF다. ON이면 선택 State의 texel 적층 높이를 `1 2 1 / 2 4 2 / 1 2 1`의 3×3 가우시안 가중치로 필터링하고, 유효 이웃의 가중치 합으로 정규화한다. 같은 Surface·UV chart·Profile에서 해당 State가 표시되는 texel만 섞는다. Meso 높이는 원래 texel 값을 유지하고, 필터 결과는 Overlay 윗면과 옆면에 함께 사용한다. State 저장량, Solver 높이 계산과 형상 피드백은 바꾸지 않는다. 이 필터는 높이 변화의 급격함을 줄이지만 연결 삼각형의 평면성을 제거하지는 않는다.
- 경계와 옆면의 GPU 생성 비용, UV seam 연속성, 윗면과 옆면의 접합 및 투명 패스의 겹침은 구현 후 검증한다. Mud와 WaterFilm 동시 적층 높이의 물리적 순서는 이 ADR에서 정하지 않는다.

## Related

- [[0036-Texel-Geometry-Preview|ADR 0036 — Compute texel 표시 형상]]
- [[0037-Texel-Grid-and-Demo-Lit-Effects|ADR 0037 — 데모 Lit 효과]]
- [[0038-Source-Topology-Seam-Stitching|ADR 0038 — 원본 topology 기반 seam 봉합]]
- [[0039-State-Thickness-Per-Amount|ADR 0039 — State별 적층 두께]]
- [[0028-Accumulation-Height-and-Normal-Map|ADR 0028 — 높이와 Normal Map]]
- [[../04_Architecture/0009_Rendering|Surface State Rendering]]
