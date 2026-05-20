param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$Triplet = "x64-windows",
    [string]$VcpkgRoot = "external/vcpkg",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
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

try {
    & $exe @AppArgs
}
catch {
    $message = $_.Exception.Message
    if ($message -like "*Application Control policy*") {
[Console]::Error.WriteLine(@"
Windows blocked the built executable with an Application Control policy.

The project built successfully, but this machine is not allowed to launch this unsigned user-built .exe from:
  $exe

Fix options:
  - allow this project/output folder in Windows Security or your organization policy
  - run from a developer folder approved by the policy
  - sign the executable with a certificate trusted by the policy
  - ask the policy/admin owner to allow local C++ debug builds
"@)
        exit 1
    }

    throw
}
