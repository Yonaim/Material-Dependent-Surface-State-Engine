#!/usr/bin/env bash
# 프로젝트 소유 C++ 파일에 공통 clang-format 설정을 적용한다.
set -euo pipefail

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "Error: clang-format is required but was not found in PATH." >&2
    exit 1
fi

cd "$PROJECT_ROOT"
find . -type d \( -name .git -o -name Build -o -name build -o -name 'build-*' -o -name CMakeFiles -o -name ThirdParty \) -prune -o \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cc' -o -name '*.cxx' \) \
    -exec clang-format -i {} +
