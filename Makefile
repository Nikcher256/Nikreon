.RECIPEPREFIX := >

BUILD_DIR ?= build
CONFIG ?= Debug
TRIPLET ?= x64-windows
VCPKG_ROOT ?= external/vcpkg
CMAKE ?= cmake

.PHONY: help deps configure build run clean distclean git-status

help:
> @echo Available targets:
> @echo   make deps       - clone/bootstrap vcpkg and install dependencies
> @echo   make configure  - configure CMake using the vcpkg toolchain
> @echo   make build      - build the engine
> @echo   make run        - run the engine executable
> @echo   make clean      - remove CMake build directory
> @echo   make distclean  - remove build directory and local vcpkg checkout
> @echo   make git-status - show git status

deps:
> powershell -ExecutionPolicy Bypass -File scripts/bootstrap-deps.ps1 -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"

configure:
> powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"

build:
> powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"

run:
> powershell -ExecutionPolicy Bypass -File scripts/run.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"

clean:
> powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"

distclean: clean
> powershell -NoProfile -Command "if (Test-Path '$(VCPKG_ROOT)') { Remove-Item -Recurse -Force '$(VCPKG_ROOT)' }"

git-status:
> git status --short
