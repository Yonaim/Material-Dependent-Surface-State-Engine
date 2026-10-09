# GPU 최적화 구현 메모

작성일: 2026-10-07

## 구현 상태

- Coverage는 render tile 단위의 dirty list와 indirect dispatch를 사용한다. 결과는 기존처럼 정점별 Coverage buffer에 저장한다. 최초 생성과 설정 변경 시에는 전체 정점을 한 번 다시 계산한다.
- 공통 render tile 크기는 8×8, 16×16, 32×32이며 기본값은 16×16이다. Occupancy, Height, Normal, Height smoothing, Coverage가 같은 크기를 사용한다.
- Performance 탭에 Base Mesh, Overlay Top, Overlay Sides draw 토글과 render tile 크기, Per-WG Active Channel Mask 토글을 추가했다. 토글 변경 시 profiling 평균을 초기화한다.
- active tile과 dirty coverage tile을 분리했다. State의 실제 표시 saturation과 마지막 scan 결과를 비교해 dirty tile을 만들고, bilinear sampling·5×5 smoothing·top geometry 판정을 위해 한 타일 halo까지 갱신한다. 변화가 없는 active tile의 정점 Coverage는 재사용한다.
- Solver의 기존 instance-global ActiveChannelMask 아래에 64-texel WG별 A/B 마스크를 추가했다. 현재 상태 또는 새 상태가 양수인 channel을 다음 세대에 남기고, 이웃 WG로 1-hop 확장한다. 이전 ping-pong buffer 정리를 위한 한 세대 유지도 포함한다.

## 데이터와 동기화

- Solver sparse metadata는 인스턴스별 AccumulationHeights buffer에 있다. 각 A/B block에 `WG count × uint32` 마스크 plane을 추가했고, CPU contact input에도 그룹별 channel mask를 추가했다. Pass2는 WG 내부 channel bit를 shared memory로 합친 뒤 자신의 다음 WG mask와 instance-global mask에 기록한다. 서로 다른 WG로 향하는 topology edge의 mask 병합에는 atomic OR을 사용한다.
- Render geometry cache 역시 인스턴스별이다. 기존 active flag와 height/normal list 옆에 built coverage signal, coverage dirty flag/list, indirect command를 추가했다. 각 tile은 자신의 활성 여부와 마지막 표시 신호를 검사한다. geometry 변화도 coverage 갱신을 예약한다.
- 동일 메시의 tile→vertex CSR 목록은 shared geometry의 mesh variant에서 8/16/32별로 한 번 생성한다. 정점별 Coverage, compact index, CSR 업로드 위치와 draw 명령은 인스턴스·overlay layer·mesh resolution별 output에 보관한다. 새 storage-buffer binding 없이 기존 binding 4 공간을 확장해 장치의 descriptor 한도를 지킨다.
- Coverage shader는 한 WG가 한 dirty tile의 CSR 정점 목록을 순회한다. 첫 output, tile 크기, smoothing, 재질 channel/mask, profile, geometry 입력 revision, 표시 높이 배율 또는 opaque-base 설정이 바뀌면 해당 output의 정점 Coverage를 전체 재계산한다. Boundary와 top/side 판정은 재사용되거나 갱신된 동일 Coverage buffer를 읽는다.
- Draw count 초기화는 새 `OverlayDrawReset.comp`로 분리했다. Coverage dispatch가 0개인 프레임에도 boundary/top/side draw command 초기화가 실행된다. 새 shader는 CMake의 `GLOB_RECURSE CONFIGURE_DEPENDS` 목록에 자동 포함되어 별도 CMake 수정이 필요 없다.

## 확인 결과

- Debug 전체 빌드와 shader SPIR-V 컴파일 통과. `git diff --check` 통과.
- CTest 7개 중 5개 통과. `MDSS_SharedSurfaceGeometry` 1건과 `MDSS_SurfaceGPUResource` 7건은 작업 시작 시점 소스를 별도로 빌드해 실행해도 같은 실패가 재현된다. 이번 변경에 추가한 CSR mapping, WG 경계 이웃 channel, transport, 한 세대 stale cleanup 검사는 새 실패를 만들지 않았다.
- Vulkan validation이 켜진 Debug 실행에서 기본 16×16, 8×8+Per-WG, 32×32+Per-WG와 동일 geometry를 공유하는 2개 instance 장면을 실행했다. 세 실행 모두 종료 코드 0이며 `[ERROR]`/VUID validation 오류가 없었다. 2개 instance 로그에서는 shared Surface variant 1개, Surface instance 2개를 확인했고, 초기 contact는 instance 0에만 적용됐다. 이 검사는 descriptor·recording 경로를 확인하며, 두 인스턴스의 모든 픽셀/상태 값이 동일하지 않음을 수치로 증명하는 비교 검사는 아니다.
- 실제 GPU 시간 개선량은 장면·해상도·장치에 따라 달라지므로 여기에는 수치를 기재하지 않는다. A/B 측정 절차는 별도 가이드에 있다.

## 패키지 범위

전체 소스 ZIP은 현재 작업 디렉터리의 소스·셰이더·에셋·설정·문서·테스트·스크립트·ThirdParty를 담는다. 작업 전부터 있던 수정 파일도 현재 내용 그대로 포함한다. 패치는 작업 시작 직전 복사본과 비교한 **이번 작업의 변경분만** 담는다. `.git`, `Build`, `Cache`, 로컬 `AGENTS.md`, `.obsidian` 설정, 임시 파일은 ZIP에서 제외한다.
