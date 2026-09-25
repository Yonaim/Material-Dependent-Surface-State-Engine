# Week-04 — Surface 데이터 기반 구현

상태: **계획** · 상위 계획: [[02_Planning/00_Project-Overview/0000_Project-Plan|Project Plan]], [[02_Planning/00_Project-Overview/0001_Roadmap|Roadmap]] · 상세 계획: [[02_Planning/02_Weekly-Details/Week-04/0000_Week4-Branch-Plan|4주차 구현 브랜치 계획]]

## 목표

준비된 Simulation UV를 가진 테스트 Mesh에서 Surface data 생성부터 GPU Solver와 접촉 입력까지 최소 end-to-end 경로를 연결한다.

## 작업 순서

1. **Surface Data Contract** — 네 상태 채널, CPU data type, ID, Profile과 Surface의 소유권·검증 계약을 만든다.
2. **Simulation Mapping** — UV를 검증하고 Triangle rasterization / barycentric mapping, valid texel, 8-neighbor와 seam 연결을 구현한다.
3. **Shared Geometry Build** — mapping 결과로 texel Position·Normal·Neighbor와 기본 Geometry field를 생성한다.
4. **Surface GPU Resources** — Shared Geometry, Profile, instance State A/B, TempAlpha와 dense InputDelta를 업로드하고 descriptor를 준비한다.
5. **Surface Solver 2-Pass** — Pass 1의 alpha, Pass 2의 Next State 계산, barrier와 ping-pong을 연결한다.
6. **Surface Input Integration** — Raycast / ContactWeight로 dense InputDelta를 만들고 Solver와 debug view에 연결한다.

## Simulation UV 범위

- 자동 UV unwrap이나 chart 생성 도구는 이번 주에 구현하지 않는다.
- UV가 준비된 테스트 OBJ를 사용한다. 기존 OBJ `vt`는 Mapping 검증을 통과할 때만 임시 Simulation UV로 사용한다.
- 잘못된 UV는 자동 보정하지 않고 Mesh / Surface / Triangle 식별 정보와 오류 원인을 보고한다.
- 별도 UV channel 또는 preprocessing cache에 보존할 최종 Asset 형식은 후속 설계다.

## 산출물 / 확인 경로

```text
OBJ + prepared UV
→ Mesh-to-Texel Mapping
→ Shared Geometry
→ GPU State A/B + TempAlpha
→ 2-Pass Solver
→ Contact Input
→ Debug View
```

테스트 OBJ에서 위 경로가 확인되고, 기본 네 상태 채널이 서로 섞이지 않으며, seam을 건너는 neighbor가 동작해야 한다.

## 참고

- [[05_Development/Notes/0000_Surface-Simulation-Mapping|Surface Simulation Mapping]]
- [[05_Development/Notes/0003_Surface-State-GPU-Resource|Surface State GPU Resource]]
- [[03_Architecture/0004_Surface-State-Update|Surface State Update]]
