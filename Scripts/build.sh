#!/usr/bin/env bash
# 의존성과 빌드 캐시를 확인하고 프로젝트의 CMake 빌드를 실행한다.
set -euo pipefail

# ============================================================
# 00. Build Preparation and Execution Helpers
# ============================================================
#region 00_Build_Preparation_and_Execution_Helpers
# 기존 cache 보존과 옵션 분리를 통해 준비·실행 단계의 오류를 처리한다.

# 00.00. Prerequisites and Build Cache
#region 00_00_Prerequisites_and_Build_Cache
# 필요 도구와 cache 경로를 검사하고 이전 빌드의 덮어쓰기를 피한다.

check_prerequisites() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo "Error: cmake is required but was not found in PATH." >&2
        exit 1
    fi

}

preserve_relocated_build() {
    if [[ -d "$BUILD_DIR" ]]; then
        while IFS= read -r -d '' cache_file; do
            cached_build="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache_file")"
            cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file")"
            if [[ "$cached_build" != "$(dirname -- "$cache_file")" ]] || \
                [[ "$cache_file" == "$BUILD_DIR/CMakeCache.txt" && "$cached_source" != "$PROJECT_ROOT" ]]; then
                backup_dir="$(mktemp -d "$PROJECT_ROOT/build-backup-XXXXXX")"
                mv "$BUILD_DIR" "$backup_dir/Build"
                echo "Project path changed; previous build saved to $backup_dir/Build"
                echo "CMake cache options will be reset; pass any required options again."
                break
            fi
        done < <(find "$BUILD_DIR" -type f -name CMakeCache.txt -print0)
    fi

}

#endregion

# 00.01. Arguments and Application Launch
#region 00_01_Arguments_and_Application_Launch
# 첫 -- 앞은 CMake 옵션으로, 뒤는 앱 인자로 보존해 실행한다.

# Arguments before `--` configure CMake; arguments after it go to the app.
parse_build_and_app_arguments() {
    local app_args_mode=false

    for arg in "$@"; do
        if [[ "$arg" == "--" && "$app_args_mode" == false ]]; then
            app_args_mode=true
        elif [[ "$app_args_mode" == true ]]; then
            APP_ARGS+=("$arg")
        else
            CMAKE_ARGS+=("$arg")
        fi
    done
}

launch_built_application() {
    local app="$BUILD_DIR/bin/MDSS"

    if [[ ! -x "$app" ]]; then
        echo "Error: build succeeded but the executable was not found at $app" >&2
        exit 1
    fi

    exec "$app" "${APP_ARGS[@]}"
}

#endregion

#endregion

# ============================================================
# 01. Main Process
# ============================================================
#region 01_Main_Process
# 의존성과 빌드 캐시를 확인하고 프로젝트의 CMake 빌드를 실행한다.

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

check_prerequisites

BUILD_DIR="$PROJECT_ROOT/Build"
preserve_relocated_build

CMAKE_ARGS=()
APP_ARGS=()
parse_build_and_app_arguments "$@"

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel

launch_built_application

#endregion
