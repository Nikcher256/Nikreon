# Nikreon Engine

Nikreon Engine is a C++20 Vulkan game engine project. The current repository is in the bootstrap stage: it has the build system, dependency manifest, and upgrade roadmap needed to grow into a modular renderer/game engine.

The long-term direction is documented in `ENGINE_UPGRADE_PLAN.md`: modern 3D rendering for Blender GLB/glTF assets, batched 2D rendering, text, UI/HUD, debug overlays, materials, lighting, shadows, physics, audio, scripting, and editor tooling.

## Current Stack

- C++20
- Vulkan headers plus `volk`
- GLFW
- GLM
- EnTT
- tinygltf
- stb
- FreeType
- OpenAL Soft
- miniaudio
- Jolt Physics
- spdlog
- nlohmann-json
- CMake
- vcpkg manifest mode

## Repository Layout

```text
.
|-- CMakeLists.txt
|-- Makefile
|-- vcpkg.json
|-- src/
|   `-- main.cpp
|-- scripts/
|   |-- bootstrap-deps.ps1
|   |-- build.ps1
|   |-- run.ps1
|   |-- bootstrap-deps.sh
|   |-- build.sh
|   `-- run.sh
|-- docs/
|   |-- BUILDING.md
|   `-- DEPENDENCIES.md
`-- ENGINE_UPGRADE_PLAN.md
```

## Quick Start

Install dependencies:

```sh
make deps
```

Build:

```sh
make build
```

Run:

```sh
make run
```

Pass engine arguments through `RUN_ARGS`:

```sh
make run RUN_ARGS="--frames 180"
```

On Windows, `make run` builds the executable and then launches it from the build
folder. If Windows Application Control blocks `NikreonEngine.exe`, the build is
still valid, but the machine policy must allow unsigned local debug builds,
allowlist the output folder, or run the project from an approved developer path.

## Windows Without `make`

If `make` is not installed on Windows, use the PowerShell scripts directly:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap-deps.ps1
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -ExecutionPolicy Bypass -File scripts/run.ps1
```

The configure and build scripts automatically initialize a missing
`external/NikreonUI` Git submodule checkout before running CMake. They also
bootstrap a missing local vcpkg checkout and fetch a local CMake tool when
neither `cmake` on `PATH` nor the vcpkg tool cache is available.

## Platform Defaults

The Makefile chooses a vcpkg triplet automatically:

- Windows: `x64-windows`
- Linux x64: `x64-linux`
- Linux ARM64: `arm64-linux`
- macOS Intel: `x64-osx`
- macOS Apple Silicon: `arm64-osx`

Override it when needed:

```sh
make deps TRIPLET=arm64-osx
make build TRIPLET=arm64-osx
```

## Fresh Dependency Install

To remove generated files and redownload dependencies:

```sh
make distclean
make deps
make build
```

## More Documentation

- Build details: `docs/BUILDING.md`
- Dependency details: `docs/DEPENDENCIES.md`
- Engine roadmap: `ENGINE_UPGRADE_PLAN.md`

## Notes

`external/vcpkg`, `build`, and vcpkg install output are intentionally ignored by git. They can be regenerated from `vcpkg.json`.
