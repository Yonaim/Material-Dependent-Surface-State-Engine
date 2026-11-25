# Validation & Tests — 현재 테스트 안내

> **기록 시점:** 아래 링크는 2026-11-10까지의 과거 검증 기록이다. 이후 소스 변경은 Work Log의 각 항목에 적힌 범위로만 확인하며, 이 문서 갱신 과정에서 빌드·테스트·GPU 실행은 하지 않았다. 최신 체크아웃의 검증 상태라고 과거 결과를 확대 해석하지 않는다.

## 검증 작업 시작 시

1. 변경 모듈의 [[02_Architecture/01_Contracts/00_Contract-Register|Contract Register]]와 관련 [[02_Architecture/02_Source/00_Source-Map|Source Map]]을 읽는다.
2. 테스트의 실제 경로·fixture·지원 환경을 확인하고 해당 테스트를 실행한다.
3. 성공 여부는 실행 로그를 기준으로 기록하고, 이전 실행만 확인했을 경우 **과거 통과**라고 표시한다.
4. 실행 불가 조건, 빌드 실패, GPU/드라이버 요구를 분리하여 남긴다.

## 세부 검증 경로

- [[05_Experiments/Records/Validation-History-Through-2026-11-10#Solver Validation|Solver / 면적·시간·Geometry 보정]]
- [[05_Experiments/Records/Validation-History-Through-2026-11-10#Surface System Tests|Surface Mapping / Normal / Scene / CPU·GPU 계약]]
- [[05_Experiments/Records/Validation-History-Through-2026-11-10#Dev Demo|데모 및 Reaction 비교 씬]]
- [[05_Experiments/Records/Validation-History-Through-2026-11-10#2026-11-10 — v5.9 파일·Region 설명 및 구 볼트 폐기|2026-11-10 기준 개발환경 변경 검증]]

## 새 검증 기록 형식

`날짜 / Git Commit(또는 작업 트리) / 환경 / 재현 명령 / 입력과 Fixture / 기대 조건 / 실행 결과 / 원자료 / 미검증 범위`

원본 저장소에 접근 가능한 경우 경로와 현행 동작을 다시 대조해야 한다.

## Surface Simulation 표면 시뮬레이션

[Surface Simulation 검증](04_Surface-Simulation-Validation.md)에 실행 명령, CPU/GPU 결과, 형상 소스 실험, 렌더링 캡처와 타이밍을 기록한다.

## Python 정적 검사

저장소 루트에서 Python 3.12로 로컬 검사 의존성을 설치한 후 Pyright를 실행한다.

```powershell
python -m pip install --target Build/ResearchPython -r Research/B1_Material_State_Identifiability/requirements.txt Pillow==13.0.0
python -m pip install --target Build/ConnectedSurfacePython -r Research/G_Connected_Surface_Validation/requirements.txt
npx --yes pyright --project pyrightconfig.json
```

연결 표면 연구는 별도로 고정한 NumPy·SciPy를 사용한다. `pyrightconfig.json`은 해당 연구 소스와 라이브러리의 검사 환경에 각각 경계를 두어 두 NumPy 버전의 타입이 섞이지 않게 한다.
