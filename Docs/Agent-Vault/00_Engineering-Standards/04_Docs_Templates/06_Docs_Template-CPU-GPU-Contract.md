# Document Template — {{PIPELINE_OR_PASS}} — CPU–GPU Contract

> **선택 템플릿.** GLSL/셰이더를 사용하는 프로젝트에서 CPU–GPU 연결이 복잡해질 때, 관련 모듈·패스별로 복사한다. `Contract Register`에 등록하고 **실제 선언의 정본은 소스에 둔다.**

- 상태: `초안 / 소스 확인 / 검증 완료`
- CPU 측 구현: `{{CPU_SOURCE_PATH}}`
- GLSL 셰이더: `{{SHADER_SOURCE_PATH_AND_STAGE}}`
- 관련 Flow / Requirements / Constraints: `{{LINKS}}`

## 계약 개요

- Host에서 수행하는 작업(Descriptor 설정 / Dispatch 또는 Draw): `{{HOST_ACTION}}`
- 셰이더 `main()`이 수행하는 주요 단계: `{{SHADER_FLOW}}`
- 리소스의 생성·갱신·소유권과 수명: `{{OWNERSHIP_AND_LIFETIME}}`

## 인터페이스 대응표

> 소스 선언을 그대로 복제하지 말고, **Host ↔ GLSL의 대응 관계**와 검증할 제약을 요약한다. 확인되지 않은 번호·크기·단위는 임의로 채우지 않는다.

| Host 측 설정/구조 | 셰이더 선언 위치 | 연결 / 제약 | 확인 근거 |
|---|---|---|---|
| `{{HOST_DESCRIPTOR_SETUP}}` | `{{SHADER_BINDING_LOCATION}}` | `{{DESCRIPTOR_TYPE_ACCESS_LAYOUT}}` | `{{SOURCE_OR_REFLECTION}}` |
| `{{HOST_PUSH_CONSTANT}}` | `{{SHADER_PUSH_CONSTANT}}` | `{{SIZE_OFFSET_STAGE_FLAGS}}` | `{{SOURCE_OR_TEST}}` |

- 버퍼 레이아웃: `{{STD140_STD430_MEMBER_OFFSETS_ALIGNMENT_AND_PADDING}}`
- Vertex Input / Attachment Output(해당 시): `{{LOCATION_AND_FORMAT_MATCHING}}`
- Workgroup/Dispatch(Compute인 경우): `{{LOCAL_SIZE_DISPATCH_DIMENSIONS_BOUNDS}}`

## 동기화 및 사용 순서

- 이전 패스 → 현재 패스 → 다음 패스: `{{PASS_SEQUENCE}}`
- 읽기/쓰기 액세스, Image Layout(해당 시), 메모리 가시성: `{{HAZARDS_AND_LAYOUTS}}`
- 셰이더 내부 Workgroup Barrier와 Host API 동기화의 역할 구분: `{{SYNC_CONTRACT}}`

## 검증과 변경 영향

- 실제 수행한 검증(셰이더 컴파일 / SPIR-V Validation / Reflection / Pipeline Validation / GPU 실행): `{{COMMAND_AND_RESULT}}`
- 관련 테스트·기록: `{{TEST_LINKS}}`
- 셰이더·Host 어느 쪽을 수정하든 **양쪽 소스와 이 계약 문서의 영향**을 함께 확인한다.
- 미실행 검증과 미확인 필드는 `미실행/미확인`으로 남긴다.
