# Branch 6 — Surface Input Integration

브랜치: `feat/surface-input-integration`  
선행 조건: `feat/surface-solver-2pass` 병합  
관련 설계: [[03_Architecture/0004_Surface-State-Update|Contact Input]], [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]

## 목표

사용자 입력이 실제 Mesh를 맞히고, 해당 위치 주변의 Simulation texel에 `InputDelta`를 생성해 Solver와 Debug Rendering까지 연결한다.

```text
Mouse
→ Camera Ray
→ StaticMeshInstance hit
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
struct SurfaceRayHit
{
    bool Hit = false;
    std::uint32_t InstanceIndex = 0;
    std::uint32_t TriangleID = 0;
    glm::vec3 Barycentric{0.0F};
    glm::vec3 WorldPosition{0.0F};
    float Distance = 0.0F;
};
```

MVP는 CPU triangle raycast로 충분하다. 먼저 ray를 instance local space로 변환하고 Mesh triangle과 교차 검사한다.

주의:

- non-uniform scale이 있으면 ray direction과 hit distance 변환을 신중히 처리
- 뒤집힌 triangle을 hit 대상으로 둘지 정책 명시
- 가장 가까운 양의 hit만 반환
- TriangleID는 Mapping에서 사용하는 triangle 순서와 동일해야 함

## Hit → Simulation 위치

hit triangle의 Simulation UV를 barycentric으로 보간한다.

```text
HitUV = b0*UV0 + b1*UV1 + b2*UV2
centerX = floor(HitUV.x * width)
centerY = floor(HitUV.y * height)
```

좌표를 grid 범위로 clamp하기 전에 `HitUV == 1` 경계를 처리한다. 계산된 texel이 invalid이면 같은 TriangleID의 가장 가까운 valid texel을 찾거나 입력을 거부하고 로그를 남긴다. 4주차 권장은 작은 반경 검색 후 실패 시 거부다.

## ContactWeight

접촉 중심 texel만이 아니라 실제 표면 위치를 기준으로 반경 안의 valid texel을 찾는다.

```text
distance = length(TexelWorldPosition - HitWorldPosition)
normalized = clamp(distance / radius, 0, 1)
ContactWeight = falloff(normalized)
```

초기 falloff:

```text
ContactWeight = 1 - normalized
```

UV 원형 범위를 그대로 사용하면 UV 왜곡으로 실제 표면 접촉 범위가 달라지므로 Position distance를 기준으로 한다.

## InputDelta 생성

```text
InputDelta[channel]
+= Strength * ContactWeight * Profile.inputFactor[channel]
```

Input event의 State 이름/ID는 `SurfaceStateRegistry`를 통해 `ChannelIndex`로 해석한다. 해당 texel과 channel의 `InputDelta`에 `Strength * ContactWeight * Profile.inputFactor[channelIndex]`를 누적한다. `InputDelta`의 GPU 위치는 Branch 4에서 정한 dynamic layout helper를 사용하며, 특정 이름이나 고정 channel 순서에 의존하지 않는다.
대상 texel의 Profile이 입력 State를 지원하지 않으면 그 입력은 거부하거나 no-op 처리하고 진단 정보를 남긴다. 이 동작은 입력 테스트에서 고정한다.

- discrete event이므로 `DeltaTime`을 곱하지 않는다.
- 같은 frame에 여러 event가 겹치면 합산한다.
- 4주차에는 CPU vector에 합산한 뒤 한 번 upload한다.
- 음수 입력이 필요하지 않으면 Strength를 `>= 0`으로 검증한다.
- Capacity clamp는 Solver Pass 2의 최종 NextState에서 적용한다.

GPU atomic/reduce 입력은 event 수가 성능 문제로 확인된 뒤 추가한다.

## 실행 순서

한 frame의 권장 순서:

```text
1. Input event 수집
2. Raycast
3. ContactWeight/InputDelta CPU 생성
4. InputDelta upload
5. Solver Pass 1
6. Barrier
7. Solver Pass 2
8. Barrier
9. A/B swap
10. State debug rendering
11. 소비한 InputDelta clear
```

InputDelta clear가 다음 upload보다 먼저 끝나는지 확인한다. host-visible memory를 직접 0으로 만들 경우에도 GPU 사용 완료 fence가 필요하다.

## DebugUI

최소 제어 항목:

- 활성 State 선택: Registry에 등록된 State 목록에서 선택
- Input Strength
- Contact Radius
- Solver pause/step
- Reset State
- Simulation texel 수와 valid 비율
- 현재 ping-pong buffer 표시
- 최근 GPU solver 시간

최소 시각화:

- Valid/invalid texel view (derived from InvalidSurfaceID)
- SurfaceID
- NeighborCount
- seam texel
- State A/B의 선택 채널
- TempAlpha

렌더링용 색은 디버그 표현이며 실제 Material 표현과 분리한다.

## 구현 대상

- `Source/InputSystem/Raycaster.h/.cpp`
- `Source/InputSystem/InputSystem.h/.cpp`
- `Source/SurfaceStateSystem/State/SurfaceInput.h`
- `Source/SurfaceStateSystem/SurfaceStateSystem.h/.cpp`
- `Source/DebugUI/DebugUI.h/.cpp`
- `Shaders/SurfaceDebug.frag`
- 필요 시 `StaticMeshInstance`에 SurfaceState handle 추가

## 통합 테스트 시나리오

### 단일 클릭

- DemoCube의 한 면을 클릭
- 해당 Surface의 목표 texel 주변만 State 증가
- 다른 instance에는 영향 없음

### 반복 클릭

- 같은 위치에 여러 번 입력
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
