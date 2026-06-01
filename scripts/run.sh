#!/usr/bin/env sh
set -eu

build_dir="${1:-build}"
config="${2:-Debug}"
triplet="${3:-x64-linux}"
vcpkg_root="${4:-external/vcpkg}"
shift 4 || true

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)

sh "$script_dir/build.sh" "$build_dir" "$config" "$triplet" "$vcpkg_root"

exe="$repo_root/$build_dir/$config/NikreonEngine"
if [ ! -x "$exe" ]; then
    exe="$repo_root/$build_dir/NikreonEngine"
fi

if [ ! -x "$exe" ]; then
    printf '%s\n' "Built executable was not found under $build_dir." >&2
    exit 1
fi

"$exe" "$@"
