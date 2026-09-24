# Contact Input

상태: **데이터 구조 및 기본 가중치 흐름 확정** · 근거: [[05_Assets/Documents/0004_Contact-Input.pdf|Contact Input]], [[05_Assets/Documents/0005_Next-State-Calculation.pdf|Next State 계산]]

외부 접촉은 `SurfaceContactInput`으로 Surface State System에 전달한다.

```cpp
enum class SurfaceStateType : uint8_t {
    Wetness,
    Heat,
    Burn,
    Mud
};

struct SurfaceContactInput {
    SurfaceInstanceID targetSurface; // 입력을 받을 Surface Instance
    SurfaceStateType stateType;      // 어떤 State에 대한 입력인지
    glm::vec3 worldPosition;         // 접촉 중심 위치
    glm::vec3 worldDirection;        // 입력이 들어오는 방향
    float radius;                    // 영향을 주는 범위
    float strength;                  // 외부 입력 자체의 세기
    float falloff;                   // 중심에서 멀어질수록 입력이 감소하는 정도
};
```

## ContactWeight

Input은 접촉 영역 전체에 동일하게 적용하지 않고, texel과 접촉 중심의 거리에 따라 `ContactWeight`를 적용한다.

$$
ContactWeight_i = Falloff\left(\frac{Distance_i}{Radius}\right)
$$

기본 흐름은 다음과 같다.

```text
Raycast
→ World Hit Position + Hit Triangle

Hit Triangle의 Simulation UV 계산
→ 영향 받을 후보 texel 영역 탐색

각 후보 texel의 실제 Surface Position 계산
→ Hit Position과의 거리 계산

Distance / Radius
→ ContactWeight
```

`ContactWeight ∈ [0,1]`이며 반경 밖의 texel은 0으로 처리한다. `falloff`가 실제로 어떤 함수/지수 형태를 제어하는지는 구현 시 확정한다.

`worldDirection`은 입력 방향 정보를 제공하며, 입사각 감쇠를 ContactWeight에 추가할지는 아직 필수 규칙으로 정하지 않았다.

Input 항의 전체 계산은 [[02_Architecture/0006_Propagation-Solver|Propagation Solver]]를 본다.
