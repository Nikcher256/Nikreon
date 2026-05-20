#!/usr/bin/env sh
set -eu

if command -v cmake >/dev/null 2>&1; then
    command -v cmake
    exit 0
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
tool_root="$repo_root/external/vcpkg/downloads/tools"

if [ -d "$tool_root" ]; then
    cmake_path=$(find "$tool_root" -type f -name cmake 2>/dev/null | head -n 1 || true)
    if [ -n "$cmake_path" ]; then
        printf '%s\n' "$cmake_path"
        exit 0
    fi
fi

printf '%s\n' "CMake was not found. Install CMake or run vcpkg bootstrap first." >&2
exit 1
