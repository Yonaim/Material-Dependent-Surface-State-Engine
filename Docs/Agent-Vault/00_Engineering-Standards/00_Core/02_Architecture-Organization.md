# Architecture Documentation Organization

> 프로젝트의 기능·모듈에 따라 `Flows`, `Contracts`, `Source` 문서를 체계적으로 그룹화하는 규칙이다. **문서 한 개가 소스 파일 한 개에 대응할 필요는 없다.**

## 00. 그룹화 기준

| 영역 | 디렉터리 구성 기준 | 문서에서 설명할 내용 |
|---|---|---|
| `00_Flows/` | 초기화, 프레임, 요청 처리, 종료 등 **실행 흐름·시나리오** | 상위 단계, 제어 순서, 예외·종료 조건, 데이터 흐름 |
| `01_Contracts/` | 공개 API, 데이터 포맷, 리소스 수명, 상태 규약 및 **CPU–GPU 인터페이스** 등 계약·경계 | 호출자/소비자가 보장받아야 할 불변식, 입력/출력, 소유권, GPU 바인딩·레이아웃·패스 동기화 연결 |
| `02_Source/` | 실제 코드의 **기능·서브시스템·모듈 책임** | CPU 코드와 셰이더 모듈의 역할, 엔트리 포인트/셰이더 단계, 주요 의존성, 관련 Flow/Contract |

셰이더를 사용하는 경우 **셰이더 소스는 실제 `layout` 선언과 필수 로컬 제약**, **CPU 소스는 Descriptor/Push Constant 설정**, **Contracts 문서는 양쪽의 연결과 패스 간 동기화 관계**를 책임진다. 상세한 선언 블록을 문서에 복사하지 않고 실제 파일 경로와 검증 근거를 링크한다.

동일한 기능이 세 영역에 등장할 수 있지만 세 문서는 역할이 다르므로 동일한 내용을 복제하지 않는다. 상위 수준 문서는 해당하는 상세 문서를 **링크**한다.

## 01. 계층적 구성 예시

```text
02_Architecture/
├── 00_Flows/
│   ├── 00_Main-Flow.md
│   ├── 00_Initialization/
│   │   └── 00_Start-Sequence.md
│   ├── 01_Runtime/
│   │   ├── 00_Tick-Flow.md
│   │   └── 01_Event-Handling.md
│   └── 02_Shutdown/
│       └── 00_Shutdown-Sequence.md
├── 01_Contracts/
│   ├── 00_Contract-Register.md
│   ├── 00_Data-Model/
│   └── 01_Public-API/
└── 02_Source/
    ├── 00_Source-Map.md
    ├── 00_Application/
    ├── 01_Renderer/
    └── 02_Resource-Management/
```

예시는 **선택 가능한 구조**다. `Renderer`가 없는 프로젝트에 이 디렉터리를 빈 상태로 만들지 않는다.

## 02. 문서 분리 및 인덱스 생성 기준

1. **처음에는** 최상위 `Main Flow`, `Contract Register`, `Source Map`만으로 시작한다.
2. 특정 기능/모듈의 문서가 늘어나거나 탐색 책임이 분명해지면 하위 디렉터리로 묶는다.
3. 디렉터리는 문서화할 대상의 실제 책임에 맞춰 명명한다. 서로 독립적인 기능을 단순 파일 크기 기준으로 섞지 않는다.
4. 그룹 내부에도 **지역 번호 방식**을 적용한다. 파일 그룹과 폴더 그룹은 별도로 `00, 01, 02...` 번호를 사용한다.
5. 복잡한 그룹에만 `00_Index.md`를 두고, **각 문서의 책임과 링크만** 담는다. 모든 디렉터리에서 Overview/Index 문서를 의무 생성하지 않는다.
6. 실제 소스 구조와 문서 구조는 꼭 1:1일 필요가 없다. 소스의 물리적 경로는 Source Map에 명시하고 구조 변화 시 갱신한다.
7. 새 모듈, 핵심 경로 또는 탐색 키워드가 생기면 [Document Map](../../01_Document-Map.md)의 라우팅도 갱신한다.
8. 프로젝트에 존재하지 않거나 확인하지 않은 코드·계약·호출 관계를 완성된 사실처럼 문서화하지 않는다.

## 03. 탐색 원칙

- 전체 흐름부터 알고 싶다면 [Main Flow](../../02_Architecture/00_Flows/00_Main-Flow.md)에서 시작한다.
- API·상태의 보장이 궁금하면 [Contract Register](../../02_Architecture/01_Contracts/00_Contract-Register.md)로 이동한다.
- 구현 위치와 모듈 책임은 [Source Map](../../02_Architecture/02_Source/00_Source-Map.md)에서 찾는다.
- 관련 문서는 해당 작업의 키워드·소스 경로·모듈을 기준으로 선택적으로 읽는다.
