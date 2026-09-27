# Normal Map Transfer 테스트 사례

상태: **통과** · 대상 브랜치: `feat/solver-normal-map-weights`

Simulation texel mapping으로 지정된 triangle/barycentric 좌표에서 Material UV를 얻고, Normal Map tangent-space normal을 mesh-local transfer normal로 바꾸는 CPU 전처리를 검증한다. GPU fixture는 이 결과가 instance별 TransferWeight cache와 Solver flux까지 이어지는지 확인한다.

## CPU fixture

| 사례 | 검증 범위 |
|---|---|
| Flat / tilted normal | 평탄 Map이 geometric normal을 보존하고 tangent-space 기울기가 mesh tangent로 변환됨 |
| Barycentric UV | 삼각형의 barycentric 좌표로 보간한 Material UV에서 sample함 |
| UV chart 경계 | 서로 다른 triangle UV가 각 texel의 Map sample 좌표를 독립적으로 결정함 |
| Repeat 주소 지정 | UV가 `[0, 1]` 바깥에 있어도 repeat sampler와 같은 좌표 wrapping을 적용함 |
| Tangent handedness | mirrored tangent frame에서 bitangent 방향을 반전함 |
| 잘못된 입력 fallback 신호 | 퇴화 tangent, 누락/잘못된 texture, triangle 또는 Surface 불일치는 geometric normal fallback을 요청함 |

테스트 코드는 `Tests/NormalMapTransferTests.cpp`, CPU 변환은 `Source/SurfaceStateSystem/Mapping/NormalMapTransferNormalBuilder.cpp`에 있다. 전처리 fixture는 Vulkan device 없이 실행한다.

## GPU cache 및 Solver fixture

`Tests/SurfaceGPUResourceTests.cpp`의 `TestTransferWeightSolver`는 precomputed transfer normal을 shared geometry에 설정해 다음을 확인한다.

- texel의 transfer normal 내적이 cached `NormalWeight`와 Solver flux를 조절한다.
- non-uniform instance scale에서도 normal inverse-transpose 결과를 쓴다.
- Map normal이 없는 texel은 geometric normal로 fallback한다.
- NormalWeight debug contribution을 끄면 중립값 `1.0`을 쓴다.

## 실행 및 결과

| 항목 | 값 |
|---|---|
| CPU target / CTest | `MDSS_NormalMapTransferTests` / `MDSS_NormalMapTransfer` |
| GPU target / CTest | `MDSS_SurfaceGPUResourceTests` / `MDSS_SurfaceGPUResource` |
| 전체 실행 | `ctest --test-dir <build-dir> --output-on-failure` |
| 로컬 검증 | 2026-09-27: 전체 CTest 5/5 통과, Apple M1에서 데모 1 frame 실행 및 Vulkan validation 오류 없음 |
| 제한 | live Normal Map/UV/tangent hot reload는 구현되어 있지 않다. 재생성은 asset load 수명에 한정된다. |
