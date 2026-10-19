# Decision 0016 — Texel 묶음 Grid와 데모 Lit 반응

> **한 줄 요약:** 텍셀 격자와 Wetness·Mud·WaterFilm의 데모 외관을 Lit 화면에 연결한다.

- 분류: **Rendering**
- Status: **Historical implementation; Heat appearance superseded by Decision 0027**
- Date: 2026-09-30

## 쉽게 읽기

texel 격자를 화면에 표시하고 Wetness·Mud·WaterFilm 등의 데모 상태를 조명에 연결한다. State 종류는 Profile Registry에서 찾으며 고정 채널 번호를 두지 않는다.

## Context — 왜 필요했나

높이 색상만으로 연결면의 굴곡과 공간 해상도를 함께 확인하기 어렵다. 기존 Texel Grid처럼 여러 texel을 한 셀로 표시하는 선이 필요하다. 초기 Lit은 Normal Map에 확산광과 ambient만 적용했으며 State 재질 반응은 미연결이었다. Wetness·Mud 데모를 관찰하려면 반사광과 재질 변화, Mud 두께 표시가 필요하다. WaterFilm은 별도의 `waterfilm` State key로 물막 외관과 누적 높이를 표현한다. State 종류와 ID는 로드된 `.SRProfile`의 `states`로 구성하며 고정 채널을 도입하지 않는다.

## Decision — 무엇을 정했나

1. Meso Color/Displacement, Accumulation, Final Geometry에 `Off / Overlay / Grid only` 설정을 제공한다. UV에서 texel 묶음 크기로 셀 경계를 계산하고 fragment 미분으로 선을 antialias한다. 축소 시 과밀한 선은 숨긴다. Grid only는 어두운 불투명 면 위에 선을 표시한다. 실제 삼각형 대각선이나 메시 topology 변경은 없으며 변위 실루엣은 유지한다. 새 View Mode를 추가하지 않는다.
2. `ResolveDemoSurfaceStates`는 현재 Registry에서 `wetness`, `mud`, `waterfilm` 이름의 선택적 ID를 찾는다. Scene 교체 시 바뀐 ID를 재조회하고 없는 이름은 invalid ID로 전달한다. GPU는 현재 State A/B와 texel의 Profile 지원 여부를 확인한다. 선택한 debug State channel은 Lit 반응을 결정하지 않는다.
3. Shader는 공통 read-only Surface 데이터 선언(`SurfaceStateData.glsl`), Profile·면적을 확인하는 표시용 State 샘플링(`StateSampling.glsl`), 효과별 재질 변경(`Effects/Wetness.glsl`, `Effects/Mud.glsl`, `Effects/WaterFilm.glsl`), 공통 조명(`Lighting.glsl`)으로 분리한다. 세 데모 State key만 명시적으로 연결하며 범용 State ID→효과 분배 시스템은 도입하지 않는다. Registry와 `.SRProfile` 로딩 계약은 유지한다.
4. 외관 입력은 `clamp(State / (ProfileCapacity × WorldTexelArea / SurfaceStateReferenceArea), 0, 1)`이다. bilinear 샘플은 중심 texel과 같은 Surface·chart·Profile 안에서만 반응하며 invalid/미지원 샘플은 0을 기여한다. GPU State와 Solver용 Saturation은 수정하지 않는다.
5. Mud는 갈색 피복과 demo roughness를 적용한 뒤 Wetness가 색을 더 어둡게 하고 roughness를 낮춘다. 둘이 같은 Profile에 있으면 이 순서로 함께 반응한다. 색·roughness는 현재 데모 렌더링 기본값이며 물성 측정값이나 새 `.SRProfile` 필드는 아니다.
6. WaterFilm은 `waterfilm` saturation에 따라 albedo를 어둡게 하고 roughness를 낮춘다. 누적 높이는 Capacity 제한 State와 해당 Profile의 `accumulationFactor`, `cavityFillFactor`, `thicknessPerAmount`로 계산해 Lit texel mesh에 적용한다. Solver의 별도 Accumulation Geometry Update 옵션도 각 State의 Capacity 제한 형상 기여를 합산한다. 물리적 재질 layer 순서를 뜻하지 않는다 ([[03_Architecture/0004_Surface-Geometry|Surface Geometry]]).
7. Lit은 고정 흰색 방향광과 기존 ambient, GGX/Smith/Schlick 반사 항을 사용한다. roughness는 perceptual roughness이며 GGX alpha로 제곱한다. 카메라 위치에 따라 하이라이트가 움직인다. BRDF 항의 기준은 [Filament standard model](https://google.github.io/filament/main/filament.html)이다. 환경맵·그림자·tone mapping은 포함하지 않는다.
8. Mud 또는 WaterFilm State를 지원하는 Runtime Profile table을 가진 instance는 해당 State ID의 Capacity 제한 형상 기여를 높이 입력으로 쓰며, 기존 compute 표시 위치·normal과 texel 연결면을 Lit에서도 사용한다. `Lit height display scale`과 State별 두께 계약은 [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]를 따른다. 원본 Normal Map을 computed normal에 중복 적용하지 않는다. 해당 State가 없는 instance는 원본 메시와 Normal Map 경로를 유지한다. 연결 삼각형이 없는 Surface가 포함된 instance도 원본 메시로 fallback한다.
9. Material UBO와 descriptor set은 in-flight frame별로 분리한다. 해당 frame fence 이후 카메라·재질 설정·현재 State ID를 업로드해 진행 중인 draw의 uniform을 덮어쓰지 않는다. Scene/해상도 교체는 Lit pipeline도 기존 리소스와 함께 교체하며 실패 시 이전 리소스를 유지한다.

## Alternatives Considered — 다른 방법

- 실제 polygon wireframe: 이미 제공하지만 texel 묶음의 셀 경계와 달리 연결 삼각형 대각선을 표시한다. 별도 UV grid 표시를 채택한다.
- 고정 Wetness/Mud/WaterFilm 채널 번호: Profile 집합에 따라 ID가 바뀌는 Registry 계약과 맞지 않는다.
- 범용 State→렌더 효과 시스템: 현재 세 데모 연결의 요구 범위보다 크므로 후속 설계 대상으로 남긴다.
- 원본 메시 정점으로 Mud 높이 표시: 중앙 texel만 높아질 때 실루엣 변화를 놓치는 초기 경로이므로 기존 texel 연결면을 재사용한다.

## Consequences — 결정의 영향

- 후속 설계 결정 [[05_Decisions/0019_Base-Surface-and-Accumulation-Overlay|Decision 0019]]은 단일 변위 texel 연결면을 Base Surface와 적층 Overlay의 분리 렌더링으로 발전시킨다. 이 문서의 단일 Lit draw 설명은 이전 데모 구현을 기록한다. 1차 Overlay 렌더 경로는 코드에 반영됐으며 실행 검증은 남아 있다.
- 기본 표면 반응 데모는 Wetness에서 Heat saturation 기반 red tint로 전환됐다. 현재 결정과 구현은 [[05_Decisions/0027_Heat-Red-Lit-Demo|Decision 0027]]을 따른다. 이 문서는 당시 Wetness 동작과 검증 이력을 보존한다.
- 2026-09-30 후속 결정: 위 Decision 6·8의 임시 `Accumulation height ref`는 `.SRProfile`의 State별 `thicknessPerAmount`와 렌더 전용 `Lit height display scale`로 대체했다. Lit은 Profile 두께에 표시 배율을 곱하고 Solver는 표시 배율을 읽지 않는다 ([[03_Architecture/0004_Surface-Geometry|Surface Geometry]]).
- Render Settings에서 전역 Lit 효과, Mud·WaterFilm 높이 적용 여부 및 roughness를 조절한다. 선택한 Meso·Accumulation·Final Geometry 뷰의 설명 상자에서 높이 grid를 조절한다. 표시 설정은 `.Scene`/`.SRProfile`에 저장하지 않는다.
- 실제 GPU 출력 회귀 검증은 grid 셀 크기·실루엣 보존, Wetness diffuse 변화·specular peak·카메라 반응, Mud 색·중앙 적층, 이름 조회 후 ID 이동, 미지원 Profile·A/B 전환·면적 보정·State 보존을 포함한다. Scene 교체와 Lit 형상/외관 토글은 실제 renderer에서도 검증한다.
- Material UBO는 `vec4`, `uvec4`, 32-bit scalar의 std140 block이며 16-byte 정렬, 128-byte 크기다. 기존 80-byte prefix에 세 16-byte 항목을 추가하며 별도 tail padding은 없다. 현재 in-flight frame 2개, Material 하나당 `128 × 2 = 256` byte의 uniform payload이며 texture image는 공유하고 descriptor set만 frame별로 복제한다. allocator/descriptor overhead는 제외한다.
- Mud 형상 출력과 정적 index payload는 Decision 0015의 instance별/Runtime별 공유 범위를 유지한다. Lit·Debug 간에도 같은 compute 출력 allocation을 재사용한다.
- chart 경계 봉합·경계 확장은 [[05_Decisions/0017_Source-Topology-Seam-Stitching|Decision 0017]]로 구현했다. 원본 메시의 열린 경계는 유지한다. LOD·대표 Scene 성능 측정, 물리 재질별 다중 layer 합성은 후속 작업이다. Solver 동적 형상 피드백의 현재 동작은 [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]에 기록한다. 이 구현은 최종 물리 재질 모델을 확정하지 않는다.

## Related — 관련 문서

- [[05_Decisions/0014_Accumulation-Debug-and-Texel-Inspector|Decision 0014 — 적층 디버그]]
- [[05_Decisions/0015_Texel-Geometry-Preview|Decision 0015 — Texel 연결면]]
- [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[03_Architecture/0008_Rendering|Rendering]]
- [[03_Architecture/0009_UI-Interface|UI Interface]]
