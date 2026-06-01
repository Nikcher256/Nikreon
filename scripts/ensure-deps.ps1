param(
    [string]$Triplet = "x64-windows",
    [string]$VcpkgRoot = "external/vcpkg"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$toolchain = Join-Path $repoRoot (Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake")
$cmakeAvailable = $true

try {
    & (Join-Path $PSScriptRoot "find-cmake.ps1") | Out-Null
}
catch {
    $cmakeAvailable = $false
}

if ((Test-Path $toolchain -PathType Leaf) -and $cmakeAvailable) {
    return
}

Write-Host "Local build dependencies are missing. Bootstrapping vcpkg and CMake..."
& (Join-Path $PSScriptRoot "bootstrap-deps.ps1") -Triplet $Triplet -VcpkgRoot $VcpkgRoot
