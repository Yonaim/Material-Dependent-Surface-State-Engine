# Current Work

> 누적 변경은 14개 의미 단위 커밋으로 분리했다. 소스는 분할 전 상태를 보존했으며, push는 수행하지 않았다. 중간 커밋별 재빌드는 수행하지 않았고 최종 상태의 기존 검증 기록을 유지한다.

> **한 줄 요약:** 아직 Architecture의 정본으로 확정되지 않은 현재·후속 작업만 둔다. 완료되면 결과를 Architecture / Decisions / Validation 문서에 반영하고 여기서는 제거한다.

## Engineering Standards 정비 — 현재 실행 순서

2026-11-09 사용자 결정. 현재 상태는 `refactor/engineering-standards`에서 의미 단위로 분할 커밋해 보존한다. **기존 3번 C++·4번 GLSL 구조 정리와 5번 Unreal-Inspired 네이밍 전환 완료. Universal 5.5의 큰 클래스·파일 2단계 작업도 프로젝트 전체에 적용·검토했다. 다음은 기존 6번 문서·스타일 정리다.** 아래 순서는 그 전에 요청된 Universal 규칙 동기화·shader folding을 포함해 갱신한 것이다. 완료된 folding을 GLSL 계산 구조 리팩터링 완료로 보지 않는다.

| 순서 | 작업 | 상태 / 완료 기준 |
|---|---|---|
| 선행 A | 변경 전 최소 기준선 보존 | 완료: 전체 Debug build 성공, CTest 7 통과/7 실패/skip 0, shader 150/150. [실행 기록](../../../Build/Validation/20261109-210554-pre-refactor/summary.md) |
| 선행 B | Universal-Agent-Vault 규칙 병합·shader folding | 공통 규칙·JS/TS 선택 프로필·라우팅·언어별 folding 반영. 최초 shader 50개·84개 region 적용 후 사용자 요청으로 작은 19개도 포함해 보완 당시 69개·103개로 확장(구조 정리 당시 116개, 5.5 전체 적용 후 122개). CRLF 마커 인식·기본 자동 접기도 수정. 검증은 [실행 기록](../../../Build/Validation/20261109-211718-vault-region-folding/summary.md) 참조 |
| 1 (기존 3) | C++ 상위 흐름 정리 | 완료: LoadDemoAnimation → MainLoop → RecordCommandBuffer 책임 분리. build 성공, CTest 상태/실패 이유 기준선 동일, 파서 35개·render-only 캡처 동일. Overlay 런타임은 GPU 한도로 미검증. [근거](../../../Build/Validation/20261109-213530-cpp-top-down/summary.md) |
| 2 (기존 4) | GLSL 계산 구조 정리 | 완료: Debug·Overlay·Solver 13개 구조 정리. shader 150개 컴파일/검증·reflection 동일, build 성공, CTest 7 통과/7 실패·기준선 동일, debug GPU 출력 16개 동일. Overlay 전체·성능 미검증. [근거](../../../Build/Validation/20261109-221158-glsl-top-down/summary.md) |
| 3 (기존 5) | Unreal-Inspired 네이밍 | 완료: C++ 일반 타입/비템플릿 별칭 F·enum E·scalar bool b. 템플릿/외부 타입·GLSL 이름/직렬화 유지. 전체 build 성공, CTest 7 통과/7 실패·기준선 동일, shader 150개 동일, render-only 출력 동일. [근거](../../../Build/Validation/20261109-222214-unreal-naming/summary.md) |
| 우선 추가 | Universal 5.5 2단계 구조화 | 완료: 구현 코드 323개 + CMake 2개 검토, 구획과 실제 책임 추출을 별도 검증. 8개 값 소유 상태·frame/solver/UI/geometry/Python/Shell 분리, 유지 판단도 기록. [파일별 대조표](12_Section-and-Responsibility-Coverage.md) · [검증](../../../Build/Validation/20261109-230833-vault55-project-wide/summary.md) |
| 4 (기존 6) | 문서·스타일 정리 | 대기: Doxygen 한국어, Python/Shell 구조와 필요한 큰 책임별 folding, 포맷터 버전 확정 후 검사. JS/TS는 실제 도입 전 비활성 |
| 5 (기존 2) | 오류 처리·CPU–GPU 계약 보완 | 후순위 유지: benchmark 쓰기 오류, 버퍼 길이/인덱스/수명 계약 확인. 기존 실패의 원인은 개별 작업으로 판정하고 기대값을 임의 변경하지 않음 |
| 6 | 전체 검증 및 결과 정리 | 마지막: 정식 결과 수집 절차 보완, 전체 build/CTest/shader와 필요한 GPU·성능 회귀. 기존 실패·환경 제한·신규 회귀를 분리 |

각 단계의 관련 build/test와 문서 동기화는 진행 중에도 수행한다. 전체 검증을 마지막에 둔다는 이유로 단계별 검증을 생략하지 않는다. 일반 소스의 region은 함수/파일 책임을 대신하지 않으며, 작은 파일·모든 함수에 일괄 강제하지 않는다.

## Compile-Time GPU A/B Shader Variants

상태: **Decision 0029 구현 완료 · A/B 성능·화질 검증 대기**

Decision 0029에 따라 build configuration당 43개의 유효 SPIR-V variant를 생성하고 A/B pipeline으로 선택한다. Solver의 cache/layout/precision, rendering의 fragment sampling/precompute 및 세 shared halo를 별도 variant로 연결했다. 측정 결과를 기록한 뒤 채택할 fast path와 제거할 비교 경로를 후속 결정한다.

Debug/Release 각각 대상 43개 SPIR-V, Debug 전체 MDSS 빌드, BrickCube.Scene 기본값 2 frame과 세 shared halo ON 5 frame 기동을 확인했다. 남은 검증은 같은 Scene, 초기 State, 입력, GPU와 측정 구간으로 variant별 GPU timestamps·전체 frame time을 비교하는 것이다. Smoothing의 chart/Profile 경계, tile halo 및 출력 동등성도 검사한다. Debug/Release shader 검증 define은 각자 별도 variant set을 만든다.

## Surface Water Wetting

상태: **향후 검토**

### 목표

표면 물이 옆으로 퍼지거나 흘러간 뒤에도 접촉했던 위치에 수분이 남도록 한다. 고여 있는 물과 재질에 남은 Wetness는 별도 State로 표현한다.

### 검토안

초기 후보는 최소 잔류량과 잔류 시간이었다. 현재 공통 Reaction 구현의 `inflowFraction` 모드와 `fraction`을 사용해, 새로 입력되거나 이웃에서 유입된 양 중 일부를 Wetness로 전환하는 방식이다.

```text
DepositedWetness = NewSurfaceWater × wettingFraction
RemainingSurfaceWater = NewSurfaceWater - DepositedWetness
```

전환은 기존에 고여 있던 전체 SurfaceWater가 아니라 새 Input과 Incoming에만 적용한다. 물이 제자리에 있어도 매 timestep마다 계속 흡수되는 현상을 막고, 물의 총량은 SurfaceWater와 Wetness 사이에서 보존한다. Wetness는 자체 `decayRate`로 마르고 SurfaceWater는 기존 Transport 규칙에 따라 이동한다.

### 구현 전 결정

- `fraction`의 데모 값을 정한다. 지원 범위는 `[0,1]`이다. 큐브 데모에서는 `0.1`을 시작값으로 검토한다.
- SurfaceWater와 Wetness를 모두 지원하지 않는 Profile의 동작을 정한다.
- [[02_Architecture/01_Contracts/06_Reactions|구현된 Reaction]]의 신규 positive Input·Incoming 전환을 사용한다. GPU 회귀는 지원하며, Wetting 전용 Scene·외관과 재질별 계수 선정은 후속 작업이다.

### 검증 기준

- 평평한 표면에서 물이 이동한 경로에 Wetness가 남는다.
- 입력량 = 남은 SurfaceWater + Wetness 전환량 + 기존 Solver 손실량을 만족한다.
- 정지한 SurfaceWater가 timestep 경과만으로 Wetness로 계속 바뀌지 않는다.
- Wetness는 Profile의 `decayRate`에 따라 감소한다.

---

## Angle-Sampled Gravity Cache

상태: **후속 작업 · 미채택 · 미구현** · 날짜: 2026-10-01

### 목표와 현재 경로

정적 Mesh도 회전하면 이웃 간 월드 높이 차와 표면에 투영된 중력 방향이 변한다. 현재 Solver는 instance model 행렬과 World Gravity를 push constant로 받아 GPU에서 GeometryDrive의 HeightDrive × DirectionDrive를 매 step 계산한다. Decision 0025는 Directional RawFlux cache를 제거했으며, Decision 0029의 현재 A/B plan은 성능 비교용 cache variant를 복귀시킨다. 어떤 경로를 남길지는 해당 GPU 측정 뒤 결정한다.

Decision 0021의 순수 회전 TransferWeight buffer 갱신 제거가 선행한다. 이 후속 작업은 불필요한 GPU queue 대기를 해결하는 수단이 아니라, 남아 있는 GPU 중력 계산을 조회로 바꿀지 평가하는 별도 최적화다.

### 검토할 방식

1. 고정된 Mesh·크기·Meso 형상과 World Gravity에서 물체에 대한 **local gravity 방향**별 이웃 GeometryDrive를 CPU에서 준비한다. 전체 State량과 source saturation은 매 step 바뀌므로 표에 bake하지 않는다.
2. 모든 회전 각도를 균등 간격으로 나열하지 않는다. 단일 축 뒤집기 연출에는 1차원 회전 경로를, 자유 회전에는 local gravity 방향의 구면을 사용한다. 높이·방향 항의 보간 오차가 큰 구간을 더 촘촘히 분할한다. 단순히 양 끝값 차이만 보면 구간 내부 극값을 놓칠 수 있으므로 중간 표본 또는 오차 상한으로 확인한다.
3. CPU는 선택된 표본들을 Scene 준비 시 한 번 계산해 GPU에 상주시키고, Solver는 현재 local gravity 방향의 표본을 조회·보간한다. 회전할 때마다 CPU가 표본을 조회해 기존 GPU buffer에 다시 업로드하는 경로는 기본안이 아니다. 그것은 동기화·전송 비용을 되살릴 수 있다.
4. 적층 형상 갱신으로 유효 위치나 normal이 바뀌면 정적 조회표가 낡는다. 이 경우 GPU에서 동적 부분을 계산할지, 제한된 부분만 갱신할지, 조회 경로를 비활성화할지 결정한다.

### 비용과 결정 기준

- 예시 메모리 가정: Surface 하나가 256×256 texel이고 texel당 8개 이웃 방향, 표본·간선당 float32 값 하나를 padding 없이 저장하며 instance별 GPU buffer로 복제한다. 이때 각도 표본 하나는 256×256×8×4 byte = 2 MiB, 16개는 instance당 32 MiB다. 6 Surface인 같은 해상도 Cube 한 instance면 16개 표본에 192 MiB다. buffer allocation overhead, 보간 인덱스, 높이·방향을 별도 저장할 경우의 추가분은 제외한다.
- 먼저 Decision 0021을 반영한 뒤 기존 GPU 경로의 Pass 1 시간, 회전 중 frame time, CPU 대기 시간을 반복 측정한다.
- 표본 생성 시간, GPU 메모리, buffer 읽기 대 산술 연산, 보간 오차와 시각적 연속성을 기존 경로와 비교한다. 조회가 빨라지지 않거나 메모리 비용이 과도하면 기존 GPU 직접 계산을 유지한다.
- Flat·경사·오목한 홈, 0°·90°·180° 및 표본 사이 각도에서 직접 계산과 조회 결과를 비교한다. State 양 보존과 방향 부호가 바뀌는 구간을 포함한다.

### Related

- GPU GeometryDrive
- Pass 간 RawFlux 재사용
- Decision 0021 — 회전 불변 캐시
- Decision 0022 — 유효 형상
