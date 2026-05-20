$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    return $cmakeCommand.Source
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$toolRoot = Join-Path $repoRoot "external/vcpkg/downloads/tools"

if (Test-Path $toolRoot) {
    $cmakeExe = Get-ChildItem $toolRoot -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "cmake-.+-windows" } |
        Select-Object -First 1

    if ($cmakeExe) {
        return $cmakeExe.FullName
    }
}

throw "CMake was not found on PATH or in the local vcpkg tools cache. Run scripts/bootstrap-deps.ps1 first or install CMake."
