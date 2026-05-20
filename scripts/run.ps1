param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$Triplet = "x64-windows",
    [string]$VcpkgRoot = "external/vcpkg"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "normalize-env.ps1")

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

& (Join-Path $PSScriptRoot "build.ps1") -BuildDir $BuildDir -Config $Config -Triplet $Triplet -VcpkgRoot $VcpkgRoot

$exe = Join-Path $repoRoot (Join-Path $BuildDir (Join-Path $Config "NikreonEngine.exe"))
if (-not (Test-Path $exe)) {
    $exe = Join-Path $repoRoot (Join-Path $BuildDir "NikreonEngine.exe")
}

if (-not (Test-Path $exe)) {
    throw "Built executable was not found under $BuildDir."
}

& $exe
