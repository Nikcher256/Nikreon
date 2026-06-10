.RECIPEPREFIX := >

BUILD_DIR ?= build
CONFIG ?= Debug
REL_BUILD_DIR ?= build-rel
REL_CONFIG ?= RelWithDebInfo
VCPKG_ROOT ?= external/vcpkg
CMAKE ?= cmake

ifeq ($(OS),Windows_NT)
HOST_PLATFORM := windows
TRIPLET ?= x64-windows
DEPS_CMD := powershell -ExecutionPolicy Bypass -File scripts/bootstrap-deps.ps1 -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"
CONFIGURE_CMD := powershell -ExecutionPolicy Bypass -File scripts/configure.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"
BUILD_CMD := powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)"
RUN_CMD := powershell -ExecutionPolicy Bypass -File scripts/run.ps1 -BuildDir "$(BUILD_DIR)" -Config "$(CONFIG)" -Triplet "$(TRIPLET)" -VcpkgRoot "$(VCPKG_ROOT)" $(RUN_ARGS)
PROJECT_STATS_CMD := powershell -ExecutionPolicy Bypass -File scripts/project-stats.ps1 -BuildDir "$(BUILD_DIR)"
CLEAN_CMD := powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"
DISTCLEAN_CMD := powershell -NoProfile -Command "if (Test-Path '$(VCPKG_ROOT)') { Remove-Item -Recurse -Force '$(VCPKG_ROOT)' }"
else
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Darwin)
HOST_PLATFORM := macos
ifeq ($(UNAME_M),arm64)
TRIPLET ?= arm64-osx
else
TRIPLET ?= x64-osx
endif
else
HOST_PLATFORM := linux
ifeq ($(UNAME_M),aarch64)
TRIPLET ?= arm64-linux
else
TRIPLET ?= x64-linux
endif
endif

DEPS_CMD := sh scripts/bootstrap-deps.sh "$(TRIPLET)" "$(VCPKG_ROOT)"
CONFIGURE_CMD := sh scripts/configure.sh "$(BUILD_DIR)" "$(CONFIG)" "$(TRIPLET)" "$(VCPKG_ROOT)"
BUILD_CMD := sh scripts/build.sh "$(BUILD_DIR)" "$(CONFIG)" "$(TRIPLET)" "$(VCPKG_ROOT)"
RUN_CMD := sh scripts/run.sh "$(BUILD_DIR)" "$(CONFIG)" "$(TRIPLET)" "$(VCPKG_ROOT)" $(RUN_ARGS)
PROJECT_STATS_CMD := sh scripts/project-stats.sh "$(BUILD_DIR)"
CLEAN_CMD := rm -rf "$(BUILD_DIR)"
DISTCLEAN_CMD := rm -rf "$(VCPKG_ROOT)"
endif

.PHONY: help deps configure build run configure-rel build-rel run-rel clean distclean git-status projectStats

help:
> @echo Available targets:
> @echo   host: $(HOST_PLATFORM), triplet: $(TRIPLET)
> @echo   make deps       - clone/bootstrap vcpkg and install dependencies
> @echo   make configure  - configure CMake using the vcpkg toolchain
> @echo   make build      - build the engine
> @echo   make run        - run the engine executable
> @echo   make run-rel    - run an optimized RelWithDebInfo build for FPS testing
> @echo   make clean      - remove CMake build directory
> @echo   make distclean  - remove build directory and local vcpkg checkout
> @echo   make git-status - show git status
> @echo   make projectStats - write build/project-stats.md and .html with project metrics

deps:
> $(DEPS_CMD)

configure:
> $(CONFIGURE_CMD)

build:
> $(BUILD_CMD)

run:
> $(RUN_CMD)

configure-rel:
> $(MAKE) configure BUILD_DIR="$(REL_BUILD_DIR)" CONFIG="$(REL_CONFIG)"

build-rel:
> $(MAKE) build BUILD_DIR="$(REL_BUILD_DIR)" CONFIG="$(REL_CONFIG)"

run-rel:
> $(MAKE) run BUILD_DIR="$(REL_BUILD_DIR)" CONFIG="$(REL_CONFIG)"

clean:
> $(CLEAN_CMD)

distclean: clean
> $(DISTCLEAN_CMD)

git-status:
> git status --short

projectStats:
> $(PROJECT_STATS_CMD)
