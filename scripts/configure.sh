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
toolchain="$repo_root/$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
build_path="$repo_root/$build_dir"

if [ ! -f "$toolchain" ]; then
    printf '%s\n' "vcpkg toolchain file was not found. Run make deps first." >&2
    exit 1
fi

"$cmake" -S "$repo_root" -B "$build_path" \
    -DCMAKE_BUILD_TYPE="$config" \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
    -DVCPKG_TARGET_TRIPLET="$triplet"
