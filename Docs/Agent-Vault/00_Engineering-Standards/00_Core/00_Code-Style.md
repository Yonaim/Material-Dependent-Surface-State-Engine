# Code Style Guidelines — Intention-Revealing Abstraction

## 00. 핵심 철학

**가능한 한 적은 코드를 읽고 전체 프로세스를 이해하며, 필요한 경우에만 하위 구현으로 내려갈 수 있어야 한다.**

상위 함수는 **무엇을(What), 어떤 순서로(When)** 수행하는지 표현하고, 하위 함수는 **어떻게(How)** 수행하는지 구현한다. 추상화는 줄 수 줄이기가 아니라 의도와 제어 흐름을 드러내기 위한 수단이다.

## 01. Before / After — 원래 예시

### Before — 하나의 함수에 흐름과 세부 구현이 함께 존재

```java
private String enter(int room) {
    player = room;                                  // 1. 플레이어를 그 방에 놓는다
    if (room == wumpus) {                           // 2. Wumpus 방인가?
        if (new Random().nextInt(4) != 0) {         //    4분의 3 확률로 잡아먹힘
            gameOver = true; return "The Wumpus ate you.";
        }
        wumpus = cave.exitsOf(wumpus).get(new Random().nextInt(3));  // 4분의 1: Wumpus가 옆방으로 도망
    }
    if (pits.contains(room)) { gameOver = true; return "You fell into a pit."; }  // 3. 구덩이?
    if (bats.contains(room)) return enter(new Random().nextInt(20) + 1);          // 4. 박쥐? 무작위 방으로 다시 enter
    return describe(player);                        // 5. 아무 일 없음. 방 설명을 돌려준다
}
```

### After — 함수의 이름과 순서로 전체 처리 과정을 읽음

```java
public String move(int target) {
    if (!canMoveTo(target)) return CANNOT_GO;   // 이웃 방이 아니면 거부
    player = target;
    return resolveHazards();                    // 위험 처리로 넘긴다
}

private String resolveHazards() {
    if (isWumpusRoom()) return encounterWumpus();  // 순서 1: Wumpus
    if (isPitRoom())    return fallIntoPit();      // 순서 2: 구덩이
    if (isBatRoom())    return carriedByBat();     // 순서 3: 박쥐
    return describeRoom();                         // 아무 일 없음
}
```

**주의:** 두 예시는 **스타일의 비교**이며 동작이 완전히 동일한 리팩터링이 아니다. 원본의 Wumpus가 도망간 뒤 위험 검사를 계속하는 흐름, 박쥐에 의한 재진입, 이동 가능 여부, 반환값·상태 변경은 실제 작업에서 보존해야 한다.

## 02. 코딩 원칙

1. **Intention-Revealing Naming:** 함수명·변수명이 목적을 드러낸다. `canMoveTo`, `resolveHazards`, `fallIntoPit` 등.
2. **Top-Down Readability:** 진입점은 실행 단계의 목차처럼 읽힌다. 주요 분기와 작업 순서를 상위 함수에서 파악할 수 있다.
3. **Single Level of Abstraction:** 고수준 조율과 저수준 계산을 한 함수에 불필요하게 섞지 않는다.
4. **Explicit Control Flow:** 처리 순서, Guard Clause, Early Return, 상태 변경 및 오류 경로를 이해하기 쉽게 표현한다.
5. **Cohesive Abstraction:** 하위 함수는 이름 붙일 수 있는 응집된 책임을 가진다. 한 줄짜리 무의미한 래퍼나 과도한 인터페이스는 피한다.
6. **Side-Effect Visibility:** 은닉된 전역 상태 변화, 암묵적 오류 무시, 예상 밖의 재귀·비용을 피한다.
7. **Comments Explain Why:** 코드가 이미 드러내는 What을 반복하지 않고 제약·계약·비자명한 근거를 기록한다.
8. **Preserve Behavior:** 리팩터링과 기능 변경을 분리하며 반환·예외·순서·자원 수명·성능 제약을 보존한다.

## 03. 파일 수준의 추상화

- 책임이 커지고 내부 탐색 부담이 커질 때 파일/모듈을 분리한다. 단순히 L1/L2/L3를 만들기 위해 분리하지 않는다.
- 파일 안에서 상위 흐름 → 세부 구현의 순서를 유지한다. 큰 파일에서는 **번호가 있는 세 줄 섹션 주석과 Region 마커**를 함께 사용한다. C/C++은 `#pragma region`, GLSL·JS/TS는 `// #region`, Python·Shell은 `#region` 형식을 사용한다. 정확한 형식은 [`Local Numbering`](01_Local-Numbering-and-Navigation.md)을 정본으로 한다.
- GLSL에서는 `#version` 등의 언어 제약을 우선하고, `// #region`을 컴파일러에 무해한 주석으로 작성한다. 리소스 인터페이스 → `main()` → 세부 구현을 구분하며, 호스트–셰이더 계약은 [GLSL 규칙](../01_Languages/02_GLSL.md)을 따른다.
- 디렉터리 이름·숫자는 탐색 보조 수단이며 의존성 설계·컴파일 제약을 대체하지 않는다.
- 세부적인 섹션·탐색 관례는 [`Local Numbering and Navigation`](01_Local-Numbering-and-Navigation.md)을 따른다.

## 04. 리팩터링 절차

1. 기존 입력/출력, 제어 흐름, 예외, 상태 변경, 외부 계약을 파악한다.
2. 독립적으로 명명할 수 있는 논리적 단위를 식별한다.
3. 주요 호출 순서를 상위 함수에 드러내고 세부 내용을 하위 함수로 이동한다.
4. 변경으로 인해 데이터 흐름·상태·성능이 불투명해지지 않았는지 검토한다.
5. 기존 동작을 테스트하고 관련 Architecture·검증 문서를 동기화한다.

## 05. 코드 리뷰 체크리스트

- [ ] 최상위 함수를 읽는 것만으로 전체 흐름을 이해할 수 있는가?
- [ ] 중요한 분기 우선순위·오류 경로·상태 전이가 명시적인가?
- [ ] 함수가 주로 한 추상화 수준을 유지하는가?
- [ ] 함수·파일 분리가 의미 있는 책임을 나타내는가?
- [ ] 과도한 추상화로 탐색 비용이 오히려 커지지 않는가?
- [ ] 주요 계약·기존 동작·성능 제약이 보존되었는가?
