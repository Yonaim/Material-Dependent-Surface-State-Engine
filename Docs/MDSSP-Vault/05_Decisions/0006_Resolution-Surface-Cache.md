# Decision 0006 — 해상도별 Surface 전처리 캐시

> **한 줄 요약:** Surface 전처리 결과를 해상도별 `.Surface` 파일에 저장해 다시 사용한다.

- 분류: **Assets**
- Status: **Accepted · 구현 완료**
- Date: 2026-09-28
- 관련 문서: [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]

## 후속 결정 — 2026-09-29

현재 캐시는 AreaVector를 포함해 format 4, preprocessing version 4를 사용한다. 아래 format 3·128 B 설명은 최초 구현의 역사적 기록이며 현재 파일 layout을 설명하지 않는다.

## 쉽게 읽기

메시 전처리 결과를 해상도별 `.Surface` 파일로 저장해 다음 실행에서 재사용한다. 입력 내용과 알고리즘 버전이 달라지면 캐시를 다시 만들고, 손상된 파일은 사용하지 않는다.

## Context — 왜 필요했나

초기 설계는 Mesh당 단일 `.Surface` 파일을 선택했고, 이후 Runtime 전처리와 메모리 공유만 유지하도록 바뀌었다. Normal Map sample, PCG 높이 적분과 국소 미분 fit을 도입한 뒤 128·256·512 grid 전환을 지원한다. 이 정적 계산을 매 실행과 다시 선택한 해상도에서 반복하면 초기 준비가 길어진다.

현재 구현은 format 4와 preprocessing version 4를 사용한다. 정확한 texel 레코드 크기는 직렬화 필드 구성에서 확인해야 하므로 고정 byte 수를 여기서 주장하지 않는다.

현재 `TSharedSurfaceGeometryData`는 최종 mesh-local 형상, 이웃 graph와 texel Profile map을 포함한다. 이 결과를 복원하면 기존 GPU packing, instance별 TransferWeight 및 Solver 경로를 유지하면서 정적 CPU 계산을 생략할 수 있다.

## Decision — 무엇을 정했나

1. `.Surface`를 해상도별 생성 바이너리 캐시로 사용한다. `BuildMesoGeometry()` 완료 후 최종 CPU Geometry 전체와 Profile map을 저장한다. 필드·좌표계는 [[03_Architecture/0004_Surface-Geometry#해상도별 정적 Geometry 캐시|Geometry 캐시 계약]]을 따른다.
2. `Cache/Surface/<MeshName>_<MeshMapIdentity>/<MeshName>_<Resolution>.Surface`를 사용한다. Identity는 정규화한 절대 Mesh·Map 경로 쌍의 FNV-1a 값이다. 다른 Map과 해상도는 별도 파일로 보존하며 내용 변경은 대응하는 파일을 교체한다. 선택한 해상도만 필요 시 생성한다.
3. Fingerprint에는 파싱된 Mesh의 Position·Normal·UV·Tangent, topology와 Surface ID, Surface별 Normal Map 경로와 파일 bytes, Distribution 경로/bytes와 Profile 배치/경로/순서, grid 및 전처리 버전을 포함한다. 파일 크기·mtime만으로 유효성을 판정하지 않는다.
4. `.SRProfile`의 반응 파라미터·Transition·`states`는 원본 loader와 `TSurfaceStateRegistry`로 계속 읽는다. 이 값과 Registry channel count는 Geometry fingerprint에 넣지 않는다. Runtime Asset handle을 직렬화하지 않는다.
5. Format version 3으로 명시적 little-endian 정수와 IEEE-754 float32를 기록한다. C++ 구조체 dump를 사용하지 않는다. 이전 v1/v2는 재생성한다. 파일은 header, Surface 정의, 순서 있는 Profile 경로, padding 없는 128-byte texel 레코드로 구성한다.
6. 예상 파일 길이·개수, magic/version/fingerprint, payload checksum, Profile table, 유한 값·법선·sentinel 및 이웃 범위·중복·양방향 관계를 검증한 결과만 등록한다. Missing/stale/corrupt/unreadable cache는 Runtime 전처리로 대체한다. 원본 입력 오류는 load 실패로 유지한다.
7. 저장은 고유 임시 파일의 write/flush/close 후 rename으로 게시한다. 실패 시 임시 파일을 제거하고 기존 완성 파일을 보존한다. 저장 오류는 경고로 남기고 새 Runtime Geometry로 계속 진행한다.
8. 같은 Mesh·Map·해상도의 Runtime 메모리 공유를 유지한다. World transform/옵션 의존 TransferWeight, GPU packing의 역방향 인덱스과 자원 handle, State 및 step scratch는 캐시에 넣지 않는다. `.Scene`은 계속 원본 Mesh와 `.SurfaceProfileMap`을 참조한다.
9. 해상도 전환은 캐시를 먼저 확인하되 GPU 재생성과 State 초기화는 유지한다. 별도 Asset Build 도구, 압축/streaming, cache eviction 및 source hot reload는 후속 기능이다.

## Alternatives Considered — 다른 방법

- Runtime 전처리와 메모리 공유만 유지: 구현은 단순하지만 실행 간 무거운 정적 결과를 재사용하지 못한다.
- Normal Map의 `TransferNormal`만 저장: 샘플링은 생략하지만 PCG 적분과 미분/곡률 계산이 남으므로 최종 Geometry 저장을 선택한다.
- Mesh당 단일 파일 유지: 경로는 단순하지만 해상도와 Map을 바꿀 때 이전 변형을 다시 생성한다. 변형별 파일을 유지한다.
- 별도 Asset Build에서 미리 생성: Runtime 전처리를 줄일 수 있으나 별도 도구와 산출물 전달이 필요하다. 현재는 자동 cache miss 재생성을 구현한다.

## Consequences — 결정의 영향

- 첫 실행과 입력/알고리즘 변경에는 기존 전처리와 파일 저장 비용이 들며, cache hit는 Mapping·Normal Map CPU sample·PCG·미분 fit을 생략한다.
- Cache hit에서도 원본 로딩/fingerprint, 파일 읽기·검증과 GPU 자원 생성, instance별 TransferWeight 준비 비용은 남는다.
- 해상도/Map별 파일을 보존하므로 디스크 사용량이 늘어난다. 현재는 자동 정리하지 않으며 Cache 디렉터리는 Git에서 제외한다. 프로젝트 절대 경로가 바뀌면 새 Identity로 다시 생성한다.
- 로드·저장은 현재 동기 방식이고 직렬화 byte 배열과 CPU Geometry를 함께 보유한다. 이는 CPU 임시 메모리이며 GPU 메모리 크기를 바꾸지 않는다.
- 생성 규칙 변경 시 `PreprocessVersion`, 파일 표현 변경 시 `FormatVersion`을 증가시켜야 한다. FNV-1a는 캐시 무효화/손상 감지용이며 보안 검증용 hash가 아니다.

### Validation

전체 빌드와 CTest 6개가 통과했다. 새 `MDSS_SurfaceCache`는 Meso 적분 후 모든 texel 필드의 정확한 round-trip, invalid/render-only sentinel, seam graph, 복원 전후 GPU 역방향 인덱스과 비균일 scale의 TransferWeight 동등성을 검사한다. Missing/stale/version mismatch/checksum failure/truncation/trailing bytes의 재생성 사유, 입력 내용 변경과 동일 size/mtime의 Normal Map 변경, `.SRProfile` 수치 변경 시 재사용, Map별 경로 분리와 해상도 변형 공존, 잘못된 저장의 기존 파일 보존 및 IO 실패를 검사한다.

실제 `Demo_Cubes_Wetness.Scene`의 기본 256 grid에서 최초 실행은 두 `.Surface` 파일을 저장하고 다음 실행은 두 파일을 로드해 Mapping·Meso 전처리 로그 없이 같은 Scene을 실행했다. 측정은 현재 로컬 build(`CMAKE_BUILD_TYPE` 미지정)와 Vulkan validation을 켠 단일 cold/warm 실행이며 성능 보장이나 FPS 개선률로 해석하지 않는다.

| 측정 구간 | 최초 생성 | 캐시 재사용 |
|---|---:|---:|
| BrickCube 6 Surface × 256² | 26.23 s | 1.86 s |
| MarbleCube 6 Surface × 256² | 3.67 s | 1.74 s |
| 두 CPU 준비 구간 합계 | 29.90 s | 3.60 s |
| 프로세스 시작부터 2 frame 실행 후 종료 | 41.00 s | 13.36 s |

CPU 준비 구간은 fingerprint를 포함하며 최초 생성에는 전처리와 save, 재사용에는 read/검증을 포함한다. 전체 시간은 Mesh/texture 로딩·GPU 자원·instance별 TransferWeight·UI/렌더링·종료를 포함한다. 최초 캐시 생성 시간과 다음 실행 시간의 비교이며, 기존 무캐시 실행의 정확한 전처리 시간이나 steady-state Solver 성능 비교가 아니다.

## Related — 관련 문서

- [[03_Architecture/0004_Surface-Geometry|Surface Geometry]]
- [[05_Decisions/0003_Normal-Map-Meso-Geometry]]
- Simulation Resolution Presets
- [[03_Architecture/0003_Assets-and-Profiles]]
- [[03_Architecture/0004_Surface-Geometry]]
- `Source/SurfaceState/Preprocessing/SurfaceCache.*`
- `Source/AssetManager/Core/AssetManager.cpp`
- `Tests/SurfaceCacheTests.cpp`
