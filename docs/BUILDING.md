# Building

The project uses CMake plus vcpkg manifest mode. The main commands are the same on Windows, Linux, and macOS:

```sh
make deps
make build
make run
```

`make deps` clones vcpkg into `external/vcpkg` and installs the packages listed in `vcpkg.json`.

`make build` and `make configure` automatically initialize a missing `external/NikreonUI` Git submodule checkout before running CMake.

## Default Triplets

The Makefile picks a default vcpkg triplet based on the host:

- Windows: `x64-windows`
- Linux x64: `x64-linux`
- Linux ARM64: `arm64-linux`
- macOS Intel: `x64-osx`
- macOS Apple Silicon: `arm64-osx`

Override the triplet when needed:

```sh
make deps TRIPLET=x64-linux
make build TRIPLET=x64-linux
```

## Windows

Install:

- Visual Studio or Visual Studio Build Tools with the C++ workload
- Git
- Optional: Vulkan SDK
- Optional: Make

If `make` is not installed, use the PowerShell scripts directly:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/bootstrap-deps.ps1
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -ExecutionPolicy Bypass -File scripts/run.ps1
```

## Linux

Install common build tools first. On Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y build-essential git curl zip unzip tar pkg-config cmake ninja-build
```

For GLFW/Vulkan development you may also need system graphics packages:

```sh
sudo apt install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libvulkan1 mesa-vulkan-drivers
```

Then:

```sh
make deps
make build
make run
```

## macOS

Install command line tools and Homebrew packages:

```sh
xcode-select --install
brew install git cmake ninja
```

For Vulkan on macOS, install the Vulkan SDK with MoltenVK support. vcpkg provides headers and `volk`, but runtime Vulkan support on macOS still needs MoltenVK/Vulkan SDK or an equivalent runtime setup.

Then:

```sh
make deps
make build
make run
```

## Fresh Reinstall

To redownload dependencies:

```sh
make distclean
make deps
make build
```
