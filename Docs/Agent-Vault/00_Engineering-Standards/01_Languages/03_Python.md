# Python Engineering Profile

## 00. 프로젝트 설정

- Python 버전: `{{PYTHON_VERSION}}`; 패키지·의존성 관리: `{{PYTHON_ENV}}`; 테스트: `{{PYTHON_TEST_COMMAND}}`.
- 의존성 잠금 방식, 실행 진입점, 가상 환경 위치를 [`Project Brief`](../../01_Project-Context/00_Project-Brief.md) 또는 [`Constraints`](../../01_Project-Context/03_Constraints.md)에 기록한다.

## 01. 스타일과 추상화

- PEP 8 기반의 일관된 서식·이름을 사용한다. 프로젝트에 Ruff/Black 등 포맷터가 있으면 실제 설정을 우선한다.
- 공개 함수와 중요한 내부 경계에는 유용한 타입 힌트를 작성한다. 프로젝트가 정한 Python 버전에서 지원하는 문법만 사용한다.
- `main()` 등 최상위 단계는 `load_config() → validate_inputs() → process() → write_results()`처럼 의도를 먼저 드러낸다. 이름은 실제 역할을 따라 구체화한다.
- 클래스는 상태와 책임을 묶을 때만 사용하고, 단순 작업에 불필요한 계층을 도입하지 않는다.
- 모듈은 기능·책임 중심으로 나누며 import 순환을 피한다.

## 02. 데이터·오류·재현성

- 입력 검증과 예외 처리는 실제 실패 가능성이 있는 경계에서 수행한다. 광범위한 `except Exception: pass`처럼 오류를 숨기지 않는다.
- 파일·네트워크 등 자원은 `with`를 통해 명시적으로 관리한다.
- 전역 상태·숨은 I/O 부작용·암묵적 환경 변수 의존성을 줄인다.
- 무작위성, 시간, 경로, 외부 서비스에 의존하는 작업은 재현 가능하도록 설정·시드를 통제한다.
- 공개 API docstring은 동작을 반복하기보다 파라미터 의미, 단위, 예외, 부작용, 사용 제약을 기록한다.

## 03. 검증

- 프로젝트에서 정한 `pytest` / `unittest`, 타입 검사 및 린터를 활용한다.
- 실제 실행한 명령·환경·출력만 검증 기록에 남긴다. 실패 또는 미실행 이유를 구분한다.
- 동작/인터페이스/패키지 구조 변경 시 관련 Source Map, 계약, 테스트 문서를 함께 검토한다.

## 주석 규칙

주석과 설명은 [`Source Comments`](../02_Documentation/01_Source-Comments.md)를 따른다. 코드와 주석을 함께 수정하며, 이미 이름으로 드러나는 동작을 반복하지 않는다.

## 섹션 Region 마커

큰 Python 파일에서 책임별 접기가 필요한 경우 [공통 표기법](../00_Core/01_Local-Numbering-and-Navigation.md)을 따른다. `#region`은 **Python 주석**이며, VS Code의 기본 Region 마커로 사용할 수 있다.

```python
# ============================================================
# 00. Main Process
# ============================================================
#region 00_Main_Process

def main():
    load_config()
    run_pipeline()

#endregion
```

짧은 파일이나 개별 함수마다 마커를 강제하지 않는다.

## 큰 모듈/클래스의 단계적 책임 분리

[공통 2단계 리팩터링 지침](../00_Core/03_Section-and-Responsibility-Refactoring.md)을 따른다. 1단계는 주석과 `#region`으로 탐색을 개선한다. 2단계는 관련 상태·동작을 함수, 클래스, `dataclass`, 모듈로 추출하되 초기화, import 순환, 가변 기본값, 호출 API를 점검한다.
