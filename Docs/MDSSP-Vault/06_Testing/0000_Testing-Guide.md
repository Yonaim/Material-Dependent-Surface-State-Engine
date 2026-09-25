# 테스트 구성 가이드

이 문서는 테스트 코드와 입력 fixture를 구분하는 공통 원칙을 간략히 정리한다. 개별 구현의 구체적인 테스트 사례는 별도 문서에 둔다.

## 테스트 코드와 fixture의 역할

| 구성 요소 | 담당 역할 |
|---|---|
| 테스트 코드 | 조건을 구성하고 동작과 결과를 검증 |
| Fixture | parser/loader 등 실제 입력 파일 경로를 검증 |
| 테스트 runner | 테스트 실행 및 결과 보고 |

JSON parser의 파일 처리 자체를 확인하는 사례에는 fixture를 쓴다. 값 범위나 자료구조 불변조건은 테스트 코드에서 객체를 직접 구성하면 fixture가 불필요하게 늘어나는 것을 막을 수 있다.

## 테스트 사례 문서화

사례 문서에는 검증 대상, 입력·조건, 기대 결과와 테스트 방식을 기록한다. 반복 적용할 기준만 여기에 남기고, 구현별 함수명·fixture명·개별 결과는 주제별 문서에서 관리한다.

## Fixture 관리

- fixture는 `Tests/Fixtures/`에 두고 검증 의도가 드러나는 이름을 사용한다. 예: `Valid.SRProfile`, `UnknownState.SRProfile`.
- 정상 입력과 실패 입력을 각각 최소 단위로 구성한다. 하나의 fixture에 여러 오류를 섞지 않는다.
- 테스트가 실행 당시 작업 디렉터리에 의존하지 않도록 CMake가 fixture 경로를 전달하게 한다.
- fixture는 실제 parser/loader를 통해 읽고, 객체 직접 검증을 대신하는 용도로 쓰지 않는다.

## GPU 및 통합 테스트 경계

| 검증 대상 | 적절한 단계 |
|---|---|
| CPU 로직과 파일 parsing | CPU 단위 테스트 |
| GPU 배치와 동기화 | layout 단위 테스트 또는 Vulkan 통합 테스트 |

GPU resource가 아직 존재하지 않는 단계에서 GPU 배치 검증을 완료로 표시하지 않는다.

세부 사례: [[0001_Surface-Data-Contract-Tests|Surface Data Contract 테스트 사례]].
