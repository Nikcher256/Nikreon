$pathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
if (-not $pathValue) {
    $pathValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
}

# Some launcher environments expose both Path and PATH. MSBuild's C++ task can
# crash while creating child processes if both keys are present.
[Environment]::SetEnvironmentVariable("PATH", $null, "Process")
if ($pathValue) {
    [Environment]::SetEnvironmentVariable("Path", $pathValue, "Process")
}
