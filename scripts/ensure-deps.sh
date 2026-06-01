#!/usr/bin/env sh
set -eu

triplet="${1:-x64-linux}"
vcpkg_root="${2:-external/vcpkg}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
toolchain="$repo_root/$vcpkg_root/scripts/buildsystems/vcpkg.cmake"

if [ -f "$toolchain" ] && sh "$script_dir/find-cmake.sh" >/dev/null 2>&1; then
    exit 0
fi

printf '%s\n' "Local build dependencies are missing. Bootstrapping vcpkg and CMake..."
sh "$script_dir/bootstrap-deps.sh" "$triplet" "$vcpkg_root"
