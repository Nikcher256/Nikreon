$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$nikreonUIPath = Join-Path $repoRoot "external/NikreonUI"
$nikreonUICMakeLists = Join-Path $nikreonUIPath "CMakeLists.txt"

if (Test-Path $nikreonUICMakeLists -PathType Leaf) {
    return
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "NikreonUI is missing and git is required to initialize its submodule."
}

Write-Host "NikreonUI checkout is missing. Initializing Git submodules..."
& git -C $repoRoot submodule update --init --recursive -- external/NikreonUI
if ($LASTEXITCODE -ne 0) {
    throw "Git submodule initialization failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $nikreonUICMakeLists -PathType Leaf)) {
    throw "NikreonUI submodule initialization completed, but its CMakeLists.txt was not found."
}
