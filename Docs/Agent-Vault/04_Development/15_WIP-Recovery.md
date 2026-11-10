# 31a38c4 WIP 기능·버그 수정 복구

상태: **현재 구조에 이식 및 검증 완료 — 2026-11-10**. 기준은 `31a38c4170e414084e6aa95911bb4768601628f4`의 첫 부모 대비 변경이다. 이전 커밋 `af8085be`의 누락 파일 복원과는 별도 작업이다.

## 반영 범위

| 기능 | 현재 반영 위치·보존한 경계 |
|---|---|
| Scratch descriptor 수명 수정 | DescriptorResources의 `RebindInstanceBuffers`, resource manager의 GPU idle 이후 재연결. 기존 layout/pool/AB/BA set과 State buffer 유지 |
| Reaction ledger | InstanceResources의 opt-in 추가 allocation, SurfaceReaction의 flag 28과 실제 적용 extent 누적. 기본 계산식·기존 scratch plane 유지 |
| 연구용 geometry 제어 | flag 26의 frozen target position·갱신 억제, flag 27의 tolerance 없는 전체 갱신, benchmark `geometry_feedback=false` 반영 |
| 동일 step 캡처 | 첫 contact rasterization 뒤 1회 observer, 지정 serial의 이미지·State·geometry·초기 입력 readback. 현재 `FBenchmarkCaptureState` 안에 캡처 상태 소유 |
| 연구 도구 빌드 | CMake의 CouplingStudy·AuthoringProbe 및 modular shader 연결 복구. MesoStudy 생성 C++의 타입명을 현재 `FSharedSurfaceGeometryData`로 보완 |
| 회귀 검증 | ScratchDescriptorLifetime 추가, SharedSurfaceGeometry의 render index → compact vertex → simulation sample 대응 수정 |
| Shell 실행 흐름 | build.sh의 `--` 앞 CMake 옵션·뒤 앱 인자 분리 및 빌드 후 실행. 현재 prerequisite/cache 보존 함수·Region 유지 |

현재 F/E 타입·bool 이름, Renderer/solver의 추출된 상태 구조, 기존 중첩 Region, 복원된 자산을 보존했다. 과거 WIP의 전체 파일을 덮어쓰지 않았으며 현재 시작 Scene 설정을 과거 Rock Scene으로 되돌리지 않았다. 루트 run.sh의 Scripts/run.sh 위임은 이미 반영되어 있어 추가 변경하지 않았다.

## 검증

- CMake configure 및 전체 Debug build 성공. 앱·기존 테스트·복구한 두 연구 executable과 modular shader target 빌드 포함.
- Debug/Release shader entry·variant 150개 컴파일 및 spirv-val 통과. 이번에는 기능 추가가 있어 이전 SPIR-V 바이트 동일성을 요구하지 않는다.
- 전체 CTest **15개 중 9 통과·6 실패**. 신규 ScratchDescriptorLifetime은 12회 storage/cache 전환, layout/set·State 보존과 실제 compute/render 검사를 통과했다. SharedSurfaceGeometry도 통과하여 이전 7개 실패에서 하나 줄었다. 나머지는 SceneResources·DemoAnimation·CavityFillDisplayBound·LavaScene·OverlaySideCompaction·SurfaceMapping의 기존 실패다.
- CouplingStudy: 128 해상도, 60 Hz, 각 15 step의 ledger ON/OFF·frozen·dirty 경로 4개 실행 성공. ledger ON/OFF 최종 State 바이트 동일, 실제 누적 extent는 0보다 크며 frozen 위반·음수·비유한 값·반응 예산/산술 오류 없음.
- AuthoringProbe: 기존 Ice profile의 waterfilm 및 비교 profile의 fuel, 2개 loader/registry/appearance 사례 통과.
- 일반 비교 씬: 123 Solver step, 3개 instance의 State 배열이 WIP 반영 전 복원 검증 결과와 비트 동일. 런타임 ERROR 및 음수/비유한/Reaction 진단 오류 없음.
- checkpoint: 초기 step 0의 입력·State, step 15와 60의 이미지·State·geometry·metadata 생성 확인. 해당 metadata의 serial·target instance 일치 및 명시적 `geometry_feedback=false` 반영 확인.
- Bash 구문 검사와 분리된 fixture의 빌드→실행 순서·공백 포함 앱 인자 전달 확인. fixture는 POSIX LF source text로 실행했으며 Windows 작업 파일의 기존 CRLF 정책은 유지했다.
- CRLF 대응 diff check 통과. [실행 원자료](../../../Build/Validation/20261110-wip-port/runtime-summary.json), 전체 build/CTest/shader·개별 실행 로그는 같은 디렉터리에 보존했다.

## Engineering Standards 후속 보완 (2026-11-10)

첫 이식은 기존 구획 보존·기능 검증에 치우쳐 새 checkpoint 코드의 책임 분리·의미적 공백·API 문서화를 충분히 마무리하지 못했다. 후속 작업에서 `WriteAuthoringCheckpoint`의 저장 세부 구현을 익명 namespace 내부 함수로 추출하고 상위 조율·오류/GPU 대기 순서를 보존했다. 관련 책임별 2단계 Region, bool `bBGRA`, 의미 단위 빈 줄·명시적 분기 블록, 신규 API의 한국어 호출 조건·참조 수명·오류 문서를 보완했다. Bash의 인자 해석·실행 준비도 함수로 추출했다. 기존 공개 API·state record·GPU ABI·출력 키는 변경하지 않았다.

- 전체 Debug build 성공. descriptor 수명·SharedSurfaceGeometry·LocalReactions의 관련 CTest 3개 모두 통과. 전체 CTest 15개는 이 후속에서 반복하지 않았으며 위의 9 통과/6 실패는 최초 이식의 검증 결과다.
- step 0/15/60의 State·metadata 6개 파일과 최종 State 배열 3개가 보완 전 결과와 바이트 동일.
- 두 캡처 이미지의 차이는 Time Profiling 숫자 표시 영역에만 존재: step 15는 282 pixel, step 60은 257 pixel. 해당 bounding box 밖의 모든 pixel은 동일하며 직접 이미지 확인도 수행했다. 실시간 성능 수치를 이미지 바이트 동등성으로 간주하지 않는다.
- 잘못된 checkpoint serial, 없는 target, 기존 checkpoint 덮어쓰기 거부를 실제 앱으로 검증했다. 예상한 오류 메시지·비정상 종료 및 기존 파일 보존 확인.
- Bash 구문·분리된 LF fixture의 빌드/앱 인자 전달, Region 짝·최대 2단계·들여쓰기·CRLF 대응 diff check 확인. [후속 원자료](../../../Build/Validation/20261110-wip-standards/summary.json).

## 한계

과거 WIP 실행 파일과의 전체 동등성, 장시간/다른 GPU·OS, 독립적인 시각 품질 판정 및 성능 재측정은 수행하지 않았다. 이번 짧은 실행의 timing을 연구 성능 결과로 해석하지 않는다. 기존 6개 CTest 실패는 별도 작업이다.
