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
$cmake = & (Join-Path $PSScriptRoot "find-cmake.ps1")
$buildPath = Join-Path $repoRoot $BuildDir

if (-not (Test-Path (Join-Path $buildPath "CMakeCache.txt"))) {
    & (Join-Path $PSScriptRoot "configure.ps1") -BuildDir $BuildDir -Config $Config -Triplet $Triplet -VcpkgRoot $VcpkgRoot
}

& $cmake --build $buildPath --config $Config
if ($LASTEXITCODE -ne 0) {
    throw "CMake build failed with exit code $LASTEXITCODE."
}
