#!/usr/bin/env sh
set -eu

triplet="${1:-x64-linux}"
vcpkg_root="${2:-external/vcpkg}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
vcpkg_path="$repo_root/$vcpkg_root"
vcpkg_exe="$vcpkg_path/vcpkg"

if ! command -v git >/dev/null 2>&1; then
    printf '%s\n' "git is required to bootstrap dependencies." >&2
    exit 1
fi

if [ ! -d "$vcpkg_path" ]; then
    mkdir -p "$(dirname -- "$vcpkg_path")"
    git clone https://github.com/microsoft/vcpkg.git "$vcpkg_path"
fi

if [ ! -x "$vcpkg_exe" ]; then
    "$vcpkg_path/bootstrap-vcpkg.sh" -disableMetrics
fi

"$vcpkg_exe" install --triplet "$triplet"
