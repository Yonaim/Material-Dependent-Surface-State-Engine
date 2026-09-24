# TODO

상태: **현재 설계 기준 후속 작업**

## 4주차 구현 전에 작성할 설계

- [ ] **Surface Simulation Mapping**
  - Simulation UV 생성 방식
  - Mesh → Texel 대응
  - Valid Texel 처리
  - 8-neighbor / NeighborIndex
  - UV Seam 연결
- [ ] **Surface State GPU Resource**
  - State ping-pong + TempState
  - Shared Geometry GPU Resource
  - SRProfile GPU representation / binding
  - Descriptor / Barrier / Resource type

위 두 문서는 아직 설계를 확정하지 않았으므로 현재 Architecture 문서에 임의의 결론을 넣지 않는다.

## 구현 순서

- [ ] VulkanContext / Renderer / Scene / AssetManager 기본 골격
- [ ] OBJ / MTL / `.scene` / `.srprofile` 로딩 및 Surface→Profile 연결
- [ ] Surface Simulation Mapping 확정 및 구현
- [ ] Shared Surface Geometry Data 생성
- [ ] Surface Instance State Data 생성
- [ ] Contact Input → ContactWeight → Input 적용
- [ ] Input / Transport / Decay Solver
- [ ] 2-Pass + alpha 저장 기본안 구현 및 성능 비교
- [ ] Accumulation Height 계산
- [ ] Accumulation으로 변한 Geometry를 후속 Simulation에 반영
- [ ] Rendering 적용
- [ ] Heat → Burn Transition
- [ ] SurfaceWater / Snow State 확장 및 목표 데모 구현

## 검증

- [ ] Normal Map Integration / Non-Integrable fallback 실험
- [ ] Solver 1-Pass vs 2-Pass 성능 측정
- [ ] 상태 해상도 / GPU 시간 / 메모리 사용량 측정
