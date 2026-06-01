#!/usr/bin/env sh
set -eu

build_dir="${1:-build}"
config="${2:-Debug}"
triplet="${3:-x64-linux}"
vcpkg_root="${4:-external/vcpkg}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
sh "$script_dir/ensure-submodules.sh"
cmake=$("$script_dir/find-cmake.sh")
build_path="$repo_root/$build_dir"

if [ ! -f "$build_path/CMakeCache.txt" ]; then
    "$script_dir/configure.sh" "$build_dir" "$config" "$triplet" "$vcpkg_root"
fi

"$cmake" --build "$build_path" --config "$config"
