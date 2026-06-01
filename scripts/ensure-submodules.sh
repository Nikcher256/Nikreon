#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
nikreon_ui_path="$repo_root/external/NikreonUI"

if [ -f "$nikreon_ui_path/CMakeLists.txt" ]; then
    exit 0
fi

if ! command -v git >/dev/null 2>&1; then
    printf '%s\n' "NikreonUI is missing and git is required to initialize its submodule." >&2
    exit 1
fi

printf '%s\n' "NikreonUI checkout is missing. Initializing Git submodules..."
git -C "$repo_root" submodule update --init --recursive -- external/NikreonUI

if [ ! -f "$nikreon_ui_path/CMakeLists.txt" ]; then
    printf '%s\n' "NikreonUI submodule initialization completed, but its CMakeLists.txt was not found." >&2
    exit 1
fi
