# JavaScript / TypeScript Engineering Profile (선택 적용)

> 이 문서는 JS/TS를 사용하는 프로젝트에만 활성화한다. 공통적인 [코드 스타일](../00_Core/00_Code-Style.md)과 [섹션 표기법](../00_Core/01_Local-Numbering-and-Navigation.md)이 우선한다.

## 00. 프로젝트 설정

- 런타임/번들러: `{{JS_RUNTIME}}` (예: Node.js, Browser) / `{{BUNDLER}}`
- TypeScript 버전 및 `tsconfig.json`: `{{TYPESCRIPT_VERSION}}` / `{{TSCONFIG_PATH}}`
- 포맷터/린터/테스트: `{{JS_FORMATTER}}` / `{{JS_LINTER}}` / `{{JS_TEST_COMMAND}}`
- 해당 프로젝트의 기존 포맷터 및 타입 검사 설정을 우선한다.

## 01. 코드 구조와 의도 중심 추상화

- 최상위 함수는 주요 비동기·동기 작업 단계와 오류 경계를 먼저 드러낸다. 단순 호출을 모두 별도 함수로 감싸지는 않는다.
- 타입 검증, 리소스 정리 및 부작용은 호출 관계에서 추적할 수 있도록 한다.
- Promise/async 제어 흐름에서 오류를 조용히 무시하지 않는다.
- TypeScript에서는 `any`를 무분별하게 추가하지 않고 프로젝트의 strict 설정과 실제 계약을 따른다.

## 02. 섹션 주석과 Region

VS Code가 기본 지원하는 `// #region` / `// #endregion`을 사용한다. GLSL과 같은 표기이며, JavaScript/TypeScript에서는 일반 주석이다.

```ts
// ============================================================
// 00. Main Process
// ============================================================
// #region 00_Main_Process

export async function main(): Promise<void> {
    const config = loadConfig();
    await executePipeline(config);
}

// #endregion
```

- 섹션 번호는 파일마다 `00`, `01`, `02`부터 독립적으로 시작한다.
- 작은 파일에 Region을 강제하거나 실행 흐름을 섹션으로 숨기지 않는다.
- 실제 사용 중인 런타임과 VS Code 언어 모드가 마커를 인식하는지 확인한다.

## 03. 검증 및 문서 동기화

- 실제 프로젝트의 타입 검사, 린트, 단위 테스트, 빌드 명령을 구분하여 실행한다.
- 공개 계약이나 데이터 형식을 바꾸면 관련 Contracts/Source/Tests와 Work Log를 갱신한다.
- 주석과 JSDoc은 [Source Comments](../02_Documentation/01_Source-Comments.md)를 따른다.
