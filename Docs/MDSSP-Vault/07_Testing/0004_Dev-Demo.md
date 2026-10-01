# Dev Demo — 개발 검증용 Scene 구성

> **한 줄 요약:** BrickCube, Bunny, Mountain Scene에서 Surface State 입력과 적층 Overlay 렌더링을 확인한다.

상태: **3개 Scene의 Profile·Map·애니메이션 연결 완료** / 2026-10-02

## 목적과 현재 구성

초기안은 BrickCube와 MarbleCube의 Wetness·Mud 쌍, 그리고 Bunny·Mountain별 Wetness/Mud 쌍을 비교하는 12개 효과별 Scene 구성이었다. 현재는 세 형상의 Scene을 제공하며, 상태별 Scene 조합은 구현하지 않았다. 각 Profile은 Wetness, WaterFilm, Mud, Lava 상태를 정의하고 현재 세 Scene의 초기 접촉 입력은 Lava를 사용한다.

| Scene | 구성 | 초기 접촉 입력 | Animation |
|---|---|---|---|
| `BrickCube.Scene` | BrickCube 1개 | `lava` | `BrickCube.DemoAnim` |
| `Bunny.Scene` | Stanford Bunny 1개 | `lava` | `Bunny_Scene.DemoAnim` |
| `Mountain.Scene` | Mountain 3개 배치 | 각 오브젝트에 `lava` | `Mountain_CenterRock.DemoAnim` |

각 Scene은 `.SurfaceProfileMap`으로 형상별 `.SRProfile`을 참조한다. Profile의 상태 집합은 해당 `.SRProfile`의 `states` key에서 정해진다. Wetness와 WaterFilm은 물 기반 Surface State 예시이며, 체적 물이나 자유 표면 유체 시뮬레이션을 뜻하지 않는다.

## 자산 위치

| 위치 | 현재 자산 |
|---|---|
| `Assets/Scenes/` | `BrickCube.Scene`, `Bunny.Scene`, `Mountain.Scene` |
| `Assets/Animations/` | 세 Scene이 참조하는 애니메이션 및 상태별 추가 애니메이션 클립 |
| `Assets/SurfaceProfiles/` | 형상별 Profile과 `DemoWetness`, `DemoWaterFilm`, `DemoMud`, `DemoLava` Profile |
| `Assets/SurfaceProfileMaps/` | `BrickCube.SurfaceProfileMap`, `Bunny.SurfaceProfileMap`, `Mountain.SurfaceProfileMap` |

`SurfaceProfiles`는 `.SRProfile`을, `SurfaceProfileMaps`는 `.SurfaceProfileMap`을 보관한다. Scene의 메시, Profile Map, animation 경로는 Scene 파일 위치 기준 상대 경로다.

## 확인 항목

- 각 Scene을 열어 Profile Map과 Mesh가 로드되는지 확인한다.
- `Restart Scene`이 초기 Camera·Transform·애니메이션 상태를 복원하고 Solver State를 초기화하는지 확인한다. 초기 접촉 입력은 다음 Solver step에 적용된다.
- Wetness, WaterFilm, Mud, Lava의 선택 State 미리보기와 Overlay를 확인한다. 적층 상태는 Accumulation Height와 렌더 형상에서 확인한다.
- Scene 저장 후 다시 시작하면 저장 당시 Camera와 Transform이 기준으로 사용되는지 확인한다.

현재 시작 Scene은 `Config/Engine.ini`의 `[Application] StartupScene`이 지정한다. 기본값은 `Mountain.Scene`이다.

## 관련

- [[../04_Architecture/0003_Assets-and-Profiles|Assets와 Profiles]]
- [[../04_Architecture/0010_UI-Interface|UI Interface]]
- [[../05_ADR/0042-Scene-Referenced-Demo-Animation|ADR 0042 — Scene 참조형 데모 애니메이션]]
- [[../03_Planning/00_Project-Overview/0002_Final_Demo|최종 목표 데모]]
