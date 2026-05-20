param(
    [string]$Triplet = "x64-windows",
    [string]$VcpkgRoot = "external/vcpkg"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "normalize-env.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$vcpkgPath = Join-Path $repoRoot $VcpkgRoot
$vcpkgExe = Join-Path $vcpkgPath "vcpkg.exe"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git is required to bootstrap dependencies."
}

if (-not (Test-Path $vcpkgPath)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $vcpkgPath) | Out-Null
    git clone https://github.com/microsoft/vcpkg.git $vcpkgPath
    if ($LASTEXITCODE -ne 0) {
        throw "git clone failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path $vcpkgExe)) {
    & (Join-Path $vcpkgPath "bootstrap-vcpkg.bat") -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg bootstrap failed with exit code $LASTEXITCODE."
    }
}

& $vcpkgExe install --triplet $Triplet
if ($LASTEXITCODE -ne 0) {
    throw "vcpkg install failed with exit code $LASTEXITCODE."
}
