# Branch 6 — Surface Input Integration

브랜치: `feat/surface-input-integration`  
선행 조건: `feat/surface-solver-2pass` 병합  
관련 설계: [[03_Architecture/0008_Surface-Input|Surface Contact Input 아키텍처]], [[03_Architecture/0004_Surface-State-Update|Contact Input 수식]], [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]], [[04_ADR/0013-InputDelta-Host-Upload-Synchronization|InputDelta Host Upload 동기화 ADR]], [[04_ADR/0014-Surface-Contact-Target-API|Surface 접촉 대상 API ADR]]

## 목표

사용자 입력이 실제 Mesh를 맞히고, 해당 위치 주변의 Simulation texel에 `InputDelta`를 생성해 Solver와 Debug Rendering까지 연결한다.

```text
Inject mode + Space press
→ Center-screen TCamera Ray
→ TStaticMeshInstance hit
→ Triangle + Barycentric
→ Surface Position / Simulation UV
→ ContactWeight
→ InputDelta
→ Solver
→ Debug View
```

## Raycast 결과 계약

현재 `Raycaster`가 dummy이므로 최소 결과 구조를 정의한다.

```cpp
struct TSurfaceRayHit
{
    bool Hit = false;
    std::uint32_t InstanceIndex = 0;
    std::uint32_t TriangleID = 0;
    glm::vec3 Barycentric{0.0F};
    glm::vec3 WorldPosition{0.0F};
    float Distance = 0.0F;
};
```

MVP는 CPU triangle raycast로 충분하다. 화면 중앙에서 생성한 ray를 Mesh triangle과 교차 검사한다. 가장 가까운 양의 hit만 반환하고, 현재 렌더러와 같이 back-face를 hit 대상에서 제외한다.

주의:

- non-uniform scale이 있으면 ray direction과 hit distance 변환을 신중히 처리
- 렌더러와 동일한 back-face culling 정책을 사용한다.
- 가장 가까운 양의 hit만 반환
- TriangleID는 Mapping에서 사용하는 triangle 순서와 동일해야 함

## Hit → Simulation 위치

hit triangle의 Simulation UV를 barycentric으로 보간한다. Screen-space ray는 framebuffer 크기와 Vulkan projection 방향에 맞춰 생성한다.

```text
HitUV = b0*UV0 + b1*UV1 + b2*UV2
centerX = floor(HitUV.x * width)
centerY = floor(HitUV.y * height)
```

좌표를 grid 범위로 clamp하기 전에 `HitUV == 1` 경계를 처리한다. 계산된 texel이 유효하고 hit Triangle과 일치하면 그대로 접촉 중심으로 사용한다. 해당 texel이 invalid이거나 다른 Triangle에 속하면 동일 Surface와 동일 hit Triangle 안에서 Chebyshev 거리 2 texel 이내(`max(|dx|, |dy|) <= 2`)를 검색하고, 그중 grid 거리 제곱이 가장 작은 유효 texel을 선택한다. 동률이면 고정된 탐색 순서에서 먼저 찾은 texel을 사용한다. 찾지 못하면 해당 입력을 거부하고 event당 진단 로그를 한 번 남긴다. fallback 검색은 다른 Triangle이나 Surface로 넘어가지 않는다.

이 fallback은 raster hit의 UV를 texel grid로 내릴 때 경계나 빈 texel에 걸린 입력을 보정한다. 아래 월드 공간 반경 검색과는 별도 단계이며, 서로 다른 목적을 가진다.

## ContactWeight

UV mapping과 필요 시 fallback으로 접촉 중심 texel을 먼저 확정한다. 그 texel의 월드 위치를 중심으로 실제 표면 거리 반경 안의 valid texel을 찾아 영향 범위를 정한다. 이는 UV 해상도와 왜곡에 좌우되지 않으며, 물리적으로 가까운 Surface texel이 반경 안에 있으면 함께 영향을 받을 수 있다.

```text
distance = length(TexelWorldPosition - ResolvedCenterTexelWorldPosition)
normalized = clamp(distance / radius, 0, 1)
ContactWeight = falloff(normalized)
```

MVP falloff:

```text
ContactWeight = 1 - normalized
```

UV 원형 범위를 그대로 사용하면 UV 왜곡으로 실제 표면 접촉 범위가 달라지므로 Position distance를 기준으로 한다.

## InputDelta 생성과 업로드 동기화

```text
InputDelta[channel]
+= Strength * ContactWeight * Profile.inputFactor[channel]
```

Input event의 State 이름/ID는 `TSurfaceStateRegistry`를 통해 `ChannelIndex`로 해석한다. 해당 texel과 channel의 `InputDelta`에 `Strength * ContactWeight * Profile.inputFactor[channelIndex]`를 누적한다. `InputDelta`의 GPU 위치는 Branch 4에서 정한 dynamic layout helper를 사용하며, 특정 이름이나 고정 channel 순서에 의존하지 않는다.
대상 texel의 Profile이 입력 State를 지원하지 않으면 그 입력은 거부하거나 no-op 처리하고 진단 정보를 남긴다. 이 동작은 입력 테스트에서 고정한다.

- 접촉은 discrete event(발생 시점에 한 번 기록되는 접촉 사건)이므로 `DeltaTime`을 곱하지 않는다. 누적한 InputDelta는 다음 실행 Solver update에서 한 번 적용하고 clear한다.
- 지속 입력은 이 이벤트 입력과 구분하며, 필요할 때 별도 rate 입력으로 설계하고 `DeltaTime`을 반영한다.
- 같은 frame에 여러 event가 겹치면 합산한다.
- Branch 6 MVP에서는 CPU dense vector에 같은 frame의 입력을 합산한 뒤 대상 instance의 기존 host-visible/coherent `InputDelta` buffer에 upload한다.
- 새 입력이 있는 경우에만 Solver가 사용하는 queue가 idle이 될 때까지 기다린 다음 CPU upload를 수행한다. 현재 Solver는 graphics queue에서 실행되므로 해당 queue를 기다린다. 전체 동기화 정책과 staging buffer 전환 조건은 [[04_ADR/0013-InputDelta-Host-Upload-Synchronization|InputDelta Host Upload 동기화 ADR]]을 따른다.
- 입력이 없는 frame에는 queue 대기와 InputDelta upload를 생략한다. Solver가 소비 후 clear한 GPU buffer를 재사용한다.
- 음수 입력이 필요하지 않으면 Strength를 `>= 0`으로 검증한다.
- Capacity clamp는 Solver Pass 2의 최종 NextState에서 적용한다.
- Profile이 해당 State를 지원하지 않는 texel은 그 texel에 한해 입력을 적용하지 않는다. 일부 texel만 적용되지 않은 경우를 포함해 한 입력 event당 진단 로그는 최대 한 번 남긴다.
- `WorldDirection`을 이용한 법선·입사각 기반 weighting은 Branch 6에서 구현하지 않는다.

GPU atomic/reduce 입력은 event 수가 성능 문제로 확인된 뒤 추가한다.

## 실행 순서

한 frame의 권장 순서:

```text
1. Inject mode가 켜져 있고 Space key가 새로 눌렸는지 확인
2. ImGui가 keyboard 입력을 사용 중이 아니면 화면 중앙 ray 생성 및 CPU raycast
3. Hit → texel mapping, ContactWeight 계산, CPU InputDelta 누적
4. 새 입력이 있을 때 queue idle 대기 후 대상 InputDelta upload
5. Solver Pass 1 → barrier → Pass 2 → barrier
6. A/B swap
7. 선택한 State channel의 debug view 렌더링
8. Pass 2에서 소비한 InputDelta clear
```

Space key는 눌린 순간 한 번만 입력하며, 누르고 있는 동안 반복 입력하지 않는다. Inject mode가 켜져 있으면 화면 중앙에 crosshair를 표시한다. ImGui가 keyboard 입력을 capture하면 inject 입력은 생성하지 않는다. InputDelta의 CPU 덮어쓰기는 ADR 0013의 queue-idle 규칙으로 이전 GPU 사용 완료 뒤에만 수행한다.

## DebugUI

최소 제어 항목:

- 기존 Camera/Render/Log 창은 화면 왼쪽에 배치
- 화면 오른쪽에 Inject 창을 추가하고 Inject mode 버튼, Registry State 선택 목록, Input Strength 입력을 둔다.
- MVP Contact Radius는 world-space 고정 기본값으로 사용하며 UI 조절은 넣지 않는다.

Solver pause/step, State 초기화, texel 통계, ping-pong 상태와 GPU solver 시간 표시는 Branch 6 범위에서 제외하고 [[02_Planning/01_Weekly-Overview/Week-05|Week-05 기본 Solver 구현·검증]]으로 넘긴다.

최소 시각화:

- Valid/invalid texel view (Surface mapping과 Profile map 유효성 기준)
- Surface ID별 색상 view
- 이웃 수 view (0–8)
- seam texel view: 다른 UV chart의 이웃이 연결된 texel 표시
- State debug view: Registry에서 선택한 단일 channel만 표시하고, State A/B 중 현재 State를 읽는다.

렌더링용 색은 디버그 표현이며 실제 Material 표현과 분리한다.
Inject 입력으로 법선 또는 입사각을 보정하는 동작은 아직 넣지 않는다.
`OutgoingFluxScale` 시각화는 5주차 Solver 디버그 작업에서 다룬다.

### State debug view 색상

한 번에 Registry에서 선택한 State channel 하나를 히트맵으로 표시한다. 각 texel 값은 해당 Profile의 `StateCapacity`로 나눈 포화도(`State / Capacity`)를 사용해 고정된 `[0, 1]` 범위로 정규화한다. 낮은 값은 짙은 남색, 중간값은 파랑과 청록, 높은 값은 노랑으로 이어지는 Viridis 계열 색상표를 사용한다. Profile마다 Capacity가 달라도 색을 비교할 수 있고, 범례는 0(비어 있음)부터 1(용량 도달)까지 표시한다. 지원하지 않는 State는 회색, invalid texel은 어두운 색으로 구분한다.

## 구현 대상

- `Source/InputSystem/Raycaster.h/.cpp`
- `Source/InputSystem/InputSystem.h/.cpp`
- `Source/SurfaceStateSystem/State/SurfaceInput.h`
- `Source/SurfaceStateSystem/SurfaceStateSystem.h/.cpp`
- `Source/DebugUI/DebugUI.h/.cpp`
- `Shaders/SurfaceDebug.frag`
- 필요 시 `TStaticMeshInstance`에 SurfaceState handle 추가

## Branch 6에서 확정한 MVP 동작

- Inject mode가 켜져 있을 때 화면 중앙 crosshair를 표시하고 Space key press edge마다 한 번 접촉 event를 생성한다.
- 기존 ImGui 창은 왼쪽에 두고 오른쪽 Inject 창에서 State channel과 Strength를 지정한다.
- 유효 texel fallback은 동일 triangle 내 2 texel 반경으로 제한한다. 실패하면 입력을 거부하고 event당 진단을 한 번 남긴다.
- 입력 State를 지원하지 않는 texel은 개별 no-op 처리하고 event당 진단을 한 번 남긴다.
- `InputDelta`는 입력이 있을 때 queue idle 후 직접 host upload한다. staging buffer는 후속 최적화다.
- 법선·입사각 weighting은 구현하지 않는다.

State debug view는 고정 범위 포화도 히트맵과 0–1 범례로 표시한다.

## 통합 테스트 시나리오

### 단일 클릭

- Inject mode를 켜고 중앙 crosshair를 DemoCube 면에 맞춘 뒤 Space를 한 번 누른다.
- 해당 Surface의 목표 texel 주변만 State 증가
- 다른 instance에는 영향 없음

### 반복 클릭

- 같은 위치에서 Space를 여러 번 눌러 입력
- State가 누적되지만 Capacity를 넘지 않음

### 채널 선택

- 임의 Registry State 중 선택한 채널에만 입력이 반영됨
- 입력 State의 `ChannelIndex`만 증가하고 다른 채널 값은 변하지 않음

### Frame-rate 비교

- 동일 클릭 event는 30/60 FPS에서 같은 Input 양
- Transport/Decay 결과만 `DeltaTime`에 따라 진행

### Seam 접촉

- seam 근처 입력이 반대 chart로 자연스럽게 전파

### Profile 분리

- 같은 Mesh의 다른 Surface가 각자 Profile의 inputFactor/rate 사용

### Instance 분리

- 같은 Mesh를 공유하는 instance 두 개 중 클릭한 instance만 변경

## 실패 처리

- UV 없는 Mesh: Surface Simulation 비활성화와 명확한 로그
- Profile 없는 Surface: 기본 Profile 또는 로드 실패 중 프로젝트 정책 적용
- invalid hit texel: 작은 범위 fallback 후 입력 거부
- GPU resource 미생성: dispatch하지 않고 오류 로그
- resize/swapchain 재생성: Surface compute resource와 불필요하게 결합하지 않음

## 성능 측정

- Raycast CPU 시간
- Contact texel 탐색 시간
- InputDelta 업로드 크기(바이트)
- Pass 1/2 GPU 시간
- valid texel 비율
- instance별 동적 메모리

성능 결과는 [[05_Development/Experiments/0000_Solver-Pass-Comparison|Solver Pass 비교]]에 기록한다.

## 권장 커밋 분할

1. `Feat: Surface Input용 Static Mesh Triangle Raycast 추가`
2. `Feat: Ray Hit를 Simulation Texel로 Mapping`
3. `Feat: Contact Event를 Input Delta에 누적`
4. `Feat: Surface Solver를 Frame Update에 통합`
5. `Feat: Surface State 디버그 채널 시각화`
6. `Test: Surface Input End-to-End 시나리오 추가`

## 완료 조건

- 실제 마우스 입력이 올바른 instance/Surface/texel을 변경한다.
- Input에 `DeltaTime`이 중복 적용되지 않는다.
- Solver 결과가 매 frame 최신 buffer로 표시된다.
- seam, invalid texel, instance 분리 사례를 확인한다.
- validation layer 경고 없이 반복 실행과 State reset이 가능하다.

## 후속 작업

- GPU contact event reduce/atomic
- ProfileBoundaryWeight 최종 결합식
- Normal Map/Meso geometry
- Accumulation dynamic geometry
- SurfaceWater/Snow의 구체적인 물리 layer 모델과 실제 transition step (별도 설계 범위)
- 렌더링 품질 표현
