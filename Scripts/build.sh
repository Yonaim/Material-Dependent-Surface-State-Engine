#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# 00. Prerequisites and Build Cache Preservation
# ============================================================
#region 00_Prerequisites_and_Build_Cache_Preservation

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

# ============================================================
# 01. Main Process
# ============================================================
#region 01_Main_Process

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

check_prerequisites

BUILD_DIR="$PROJECT_ROOT/Build"
preserve_relocated_build

# Extra arguments are forwarded to CMake configuration, e.g. -DCMAKE_BUILD_TYPE=Release.
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" "$@"
cmake --build "$BUILD_DIR" --parallel

#endregion
