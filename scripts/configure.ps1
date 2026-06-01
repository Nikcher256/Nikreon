param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$Triplet = "x64-windows",
    [string]$VcpkgRoot = "external/vcpkg"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "normalize-env.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
& (Join-Path $PSScriptRoot "ensure-submodules.ps1")
& (Join-Path $PSScriptRoot "ensure-deps.ps1") -Triplet $Triplet -VcpkgRoot $VcpkgRoot
$cmake = & (Join-Path $PSScriptRoot "find-cmake.ps1")
$toolchain = Join-Path $repoRoot (Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake")
$buildPath = Join-Path $repoRoot $BuildDir

if (-not (Test-Path $toolchain)) {
    throw "vcpkg toolchain file was not found. Run scripts/bootstrap-deps.ps1 first."
}

$configureArgs = @(
    "-S", "$repoRoot",
    "-B", "$buildPath",
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    "-DVCPKG_TARGET_TRIPLET=$Triplet"
)

& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}
