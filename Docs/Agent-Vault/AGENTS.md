# Universal Agent Vault — Agent Entry

> 모든 **코드 작업**의 필수 진입 규칙. 프로젝트별 설정은 [`00_Home.md`](00_Home.md), 관련 문서의 위치는 [`01_Document-Map.md`](01_Document-Map.md)를 따른다.

## 실행 절차 — Read → Inspect → Change → Validate → Sync → Log

1. **매 코드 작업 시작 시** 이 `AGENTS.md`와 [`01_Document-Map.md`](01_Document-Map.md)를 읽는다. 이미 동일 작업에서 읽었다면 같은 내용을 불필요하게 반복 로드하지 않아도 된다.
2. **코드 수정 전에** 작업 유형·키워드·예정 소스 경로를 기준으로 관련 문서를 선택하고, [`00_Engineering-Standards/00_Core/00_Code-Style.md`](00_Engineering-Standards/00_Core/00_Code-Style.md), 활성 언어 규칙(셰이더 작업이면 GLSL 포함) 및 주석 규칙(관련 시), 관련 Context/Flows/Contracts/Source 및 필요 테스트 문서의 해당 부분을 읽는다. **전체 볼트를 일괄 읽지 않는다.**
3. 현재 소스·설정·테스트와 문서를 대조한다. 불일치가 있으면 **실제 구현에 관한 관찰**과 **지켜야 할 요구사항·계약**을 구분하고, 근거 없이 어느 한쪽을 정답으로 가정하지 않는다.
4. 의도 중심 추상화와 위에서 아래로 읽히는 실행 흐름을 유지하면서 작업한다. 기능 변경과 리팩터링을 구분하고 기존 동작·제약을 존중한다.
5. 작업 영향에 맞는 검증 범위를 [`Verification and Review`](00_Engineering-Standards/03_Workflow/01_Verification-and-Review.md)에서 확인하고, 가능한 범위에서 검증한다. **실행하지 않은 빌드·테스트·벤치마크의 성공을 주장하지 않는다.** 미실행이면 이유를 적는다.
6. 작업 종료 전에 영향받는 문서(요구사항, 용어, 계약, 흐름, 소스 맵, 테스트, ADR 포함)를 **검토하고 필요한 부분만 갱신**한다. 문서를 바꿀 필요가 없어도 영향 검토는 생략할 수 없다.
7. **모든 코드 변경 작업마다** [`04_Development/01_Work-Log.md`](04_Development/01_Work-Log.md)에 작업 단위로 짧은 기록을 남긴다. 문서 수정 없음이면 그 근거를 기록한다.
8. 종료 보고에는 `읽은 문서 / 코드 변경 / 문서 동기화 / 검증 결과·미실행 범위`를 간단히 정리한다.

## 정보의 책임

- **Engineering Standards:** 개발 방식의 규칙. **Project Context:** 프로젝트 목표·용어·요구사항·제약.
- **Architecture:** 현재 시스템의 흐름·계약·핵심 소스 위치. 기능/모듈별 문서 그룹화는 [`Architecture Organization`](00_Engineering-Standards/00_Core/02_Architecture-Organization.md)을 따른다. **Development:** 작업 현황·짧은 작업 기록.
- **Experiments:** 실제 검증·측정의 근거. **Decisions:** 왜 그 설계를 선택했는지에 관한 ADR.
- CPU–GPU 인터페이스는 **셰이더 자체의 실제 `layout` 선언·필수 주석**, **CPU 대응 코드**, **Architecture/Contracts의 전체 연결 관계**로 역할을 분리한다. 변경 시 세 곳을 함께 검토한다.
- 중복 정의를 만들지 않는다. 라우팅 맵은 정보를 복제하지 않고 **위치만 안내**한다.

## 적용 범위

- 파일/폴더 번호는 **각 디렉터리에서 `00, 01, 02…`으로 다시 시작**하고, 같은 디렉터리의 파일과 폴더 번호는 독립적이다.
- 큰 소스 파일의 섹션은 [Local Numbering](00_Engineering-Standards/00_Core/01_Local-Numbering-and-Navigation.md)의 3줄 주석 + 언어별 `#region` 계열 마커를 따른다. 편집기 접기는 보조 기능이며, 컴파일러/셰이더 호환성을 우선한다.
- 번호는 탐색 힌트이지 호출 깊이·빌드 순서가 아니다. 빌드·패키지·언어 규칙을 우선한다.
- 저장소 루트의 에이전트 설정이 이 파일을 실제로 참조하도록 연결되어 있어야 한다. 설정 방법은 [`README.md`](README.md)에 있다.

상세 운영 규칙: [`00_Engineering-Standards/00_Core/01_Document-Workflow.md`](00_Engineering-Standards/03_Workflow/00_Document-Workflow.md).
