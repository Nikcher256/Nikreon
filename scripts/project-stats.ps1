param(
    [string]$BuildDir = "build",
    [string]$OutputPath = "",
    [string]$OutputHtmlPath = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $RepoRoot

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $BuildDir "project-stats.md"
}

if ([string]::IsNullOrWhiteSpace($OutputHtmlPath)) {
    $OutputHtmlPath = Join-Path $BuildDir "project-stats.html"
}

$OutputFullPath = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath
} else {
    Join-Path $RepoRoot $OutputPath
}

$OutputHtmlFullPath = if ([System.IO.Path]::IsPathRooted($OutputHtmlPath)) {
    $OutputHtmlPath
} else {
    Join-Path $RepoRoot $OutputHtmlPath
}

$OutputDirectory = Split-Path -Parent $OutputFullPath
if (-not [string]::IsNullOrWhiteSpace($OutputDirectory)) {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
}

$OutputHtmlDirectory = Split-Path -Parent $OutputHtmlFullPath
if (-not [string]::IsNullOrWhiteSpace($OutputHtmlDirectory)) {
    New-Item -ItemType Directory -Force -Path $OutputHtmlDirectory | Out-Null
}

$TextExtensions = @(
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp",
    ".vert", ".frag", ".glsl", ".comp", ".geom", ".tesc", ".tese",
    ".cmake", ".txt", ".md", ".json", ".yml", ".yaml",
    ".ps1", ".sh", ".bat", ".cmd", ".mk"
)

$SpecialTextFiles = @(
    "Makefile",
    "CMakeLists.txt"
)

function Get-RelativePath {
    param(
        [string]$Root,
        [string]$Path
    )

    $rootFull = (Resolve-Path $Root).Path.TrimEnd("\", "/")
    $pathFull = (Resolve-Path $Path).Path
    if ($pathFull.Length -le $rootFull.Length) {
        return ""
    }

    return $pathFull.Substring($rootFull.Length).TrimStart("\", "/")
}

function Test-ExcludedPath {
    param(
        [string]$RelativePath,
        [string[]]$ExcludedPrefixes
    )

    $normalized = $RelativePath -replace "\\", "/"
    foreach ($prefix in $ExcludedPrefixes) {
        $normalizedPrefix = ($prefix -replace "\\", "/").TrimEnd("/")
        if ($normalized -eq $normalizedPrefix -or $normalized.StartsWith("$normalizedPrefix/")) {
            return $true
        }
    }

    return $false
}

function Get-FileCategory {
    param([System.IO.FileInfo]$File)

    $extension = $File.Extension.ToLowerInvariant()
    switch ($extension) {
        { $_ -in @(".cpp", ".cc", ".cxx", ".c") } { return "C++ source" }
        { $_ -in @(".hpp", ".hh", ".hxx", ".h", ".inl", ".ipp") } { return "C++ headers" }
        { $_ -in @(".vert", ".frag", ".glsl", ".comp", ".geom", ".tesc", ".tese") } { return "Shaders" }
        { $_ -in @(".ps1", ".sh", ".bat", ".cmd", ".mk") } { return "Scripts" }
        { $_ -in @(".md", ".txt") } { return "Docs/text" }
        { $_ -in @(".json", ".yml", ".yaml") } { return "Config/data" }
        ".cmake" { return "CMake" }
        default {
            if ($File.Name -eq "CMakeLists.txt") {
                return "CMake"
            }
            if ($File.Name -eq "Makefile") {
                return "Scripts"
            }
            return "Other text"
        }
    }
}

function Get-LineCount {
    param([string]$Path)

    try {
        return [System.Linq.Enumerable]::Count([System.IO.File]::ReadLines($Path))
    } catch {
        return 0
    }
}

function Test-CppFile {
    param([System.IO.FileInfo]$File)

    return $File.Extension.ToLowerInvariant() -in @(".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp")
}

function Get-Declarations {
    param(
        [System.IO.FileInfo]$File,
        [string]$RelativePath
    )

    $declarations = New-Object System.Collections.ArrayList
    if (-not (Test-CppFile -File $File)) {
        return @()
    }

    $lineNumber = 0
    foreach ($line in [System.IO.File]::ReadLines($File.FullName)) {
        ++$lineNumber
        $match = [regex]::Match($line, "^\s*(class|struct|enum\s+class)\s+([A-Za-z_][A-Za-z0-9_:]*)\b")
        if (-not $match.Success) {
            continue
        }

        if (-not $line.Contains("{")) {
            continue
        }

        $kind = $match.Groups[1].Value -replace "\s+", " "
        $name = $match.Groups[2].Value
        $declarations.Add([pscustomobject]@{
            Kind = $kind
            Name = $name
            Path = $RelativePath
            Line = $lineNumber
        })
    }

    return @($declarations)
}

function ConvertTo-HtmlText {
    param([object]$Value)

    return [System.Net.WebUtility]::HtmlEncode([string]$Value)
}

function Get-ScopeStats {
    param(
        [string]$Name,
        [string]$Root,
        [string[]]$ExcludedPrefixes
    )

    if (-not (Test-Path $Root)) {
        return [pscustomobject]@{
            Name = $Name
            Root = $Root
            Files = 0
            Lines = 0
            Categories = @{}
            LargestFiles = @()
            Declarations = @()
        }
    }

    $categoryStats = @{}
    $fileStats = New-Object System.Collections.Generic.List[object]
    $declarations = New-Object System.Collections.Generic.List[object]

    Get-ChildItem -LiteralPath $Root -Recurse -File -Force | ForEach-Object {
        $relative = Get-RelativePath -Root $Root -Path $_.FullName
        if (Test-ExcludedPath -RelativePath $relative -ExcludedPrefixes $ExcludedPrefixes) {
            return
        }

        $extension = $_.Extension.ToLowerInvariant()
        if (($TextExtensions -notcontains $extension) -and ($SpecialTextFiles -notcontains $_.Name)) {
            return
        }

        $lineCount = Get-LineCount -Path $_.FullName
        $category = Get-FileCategory -File $_

        if (-not $categoryStats.ContainsKey($category)) {
            $categoryStats[$category] = [pscustomobject]@{
                Files = 0
                Lines = 0
            }
        }

        $categoryStats[$category].Files += 1
        $categoryStats[$category].Lines += $lineCount

        $fileStats.Add([pscustomobject]@{
            Path = $relative
            Lines = $lineCount
            Category = $category
        })

        foreach ($declaration in (Get-Declarations -File $_ -RelativePath $relative)) {
            $null = $declarations.Add($declaration)
        }
    }

    $totalFiles = 0
    $totalLines = 0
    foreach ($entry in $categoryStats.Values) {
        $totalFiles += $entry.Files
        $totalLines += $entry.Lines
    }

    return [pscustomobject]@{
        Name = $Name
        Root = $Root
        Files = $totalFiles
        Lines = $totalLines
        Categories = $categoryStats
        LargestFiles = @($fileStats | Sort-Object Lines -Descending | Select-Object -First 10)
        Declarations = @($declarations | Sort-Object Kind, Name, Path)
    }
}

function Get-GitStats {
    param(
        [string]$Name,
        [string]$Root
    )

    if (-not (Test-Path (Join-Path $Root ".git"))) {
        return [pscustomobject]@{
            Name = $Name
            Available = $false
        }
    }

    Push-Location $Root
    try {
        $branch = (& git branch --show-current 2>$null)
        if ([string]::IsNullOrWhiteSpace($branch)) {
            $branch = "(detached)"
        }

        $commitCount = (& git rev-list --count HEAD 2>$null)
        $latestCommit = (& git log -1 --format="%h %ad %s" --date=short 2>$null)
        $firstCommit = (& git log --reverse --format="%h %ad %s" --date=short 2>$null | Select-Object -First 1)
        $dirtyCount = (& git status --short 2>$null | Measure-Object).Count
        $contributors = @(& git shortlog -sn --all 2>$null | Select-Object -First 5)

        return [pscustomobject]@{
            Name = $Name
            Available = $true
            Branch = $branch
            CommitCount = $commitCount
            LatestCommit = $latestCommit
            FirstCommit = $firstCommit
            DirtyCount = $dirtyCount
            Contributors = $contributors
        }
    } finally {
        Pop-Location
    }
}

function Add-CategoryTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Stats
    )

    $null = $Builder.AppendLine("| Category | Files | Lines |")
    $null = $Builder.AppendLine("| --- | ---: | ---: |")

    foreach ($name in ($Stats.Categories.Keys | Sort-Object)) {
        $entry = $Stats.Categories[$name]
        $null = $Builder.AppendLine("| $name | $($entry.Files) | $($entry.Lines) |")
    }
}

function Add-LargestFiles {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Stats
    )

    $null = $Builder.AppendLine("| File | Lines | Category |")
    $null = $Builder.AppendLine("| --- | ---: | --- |")

    foreach ($file in $Stats.LargestFiles) {
        $path = $file.Path -replace "\|", "\\|"
        $null = $Builder.AppendLine("| " + [char]96 + $path + [char]96 + " | " + $file.Lines + " | " + $file.Category + " |")
    }
}

function Add-DeclarationTable {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Stats
    )

    $null = $Builder.AppendLine("| Kind | Name | File | Line |")
    $null = $Builder.AppendLine("| --- | --- | --- | ---: |")

    foreach ($declaration in ($Stats.Declarations | Select-Object -First 80)) {
        $path = $declaration.Path -replace "\|", "\\|"
        $null = $Builder.AppendLine("| $($declaration.Kind) | `$($declaration.Name)` | " + [char]96 + $path + [char]96 + " | $($declaration.Line) |")
    }
}

function Add-GitSection {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$GitStats
    )

    if (-not $GitStats.Available) {
        $null = $Builder.AppendLine([string]::Concat("-", " Git stats unavailable."))
        return
    }

    $null = $Builder.AppendLine([string]::Concat("-", " Branch: ", $GitStats.Branch))
    $null = $Builder.AppendLine([string]::Concat("-", " Commits: ", $GitStats.CommitCount))
    $null = $Builder.AppendLine([string]::Concat("-", " Latest commit: ", $GitStats.LatestCommit))
    $null = $Builder.AppendLine([string]::Concat("-", " First commit: ", $GitStats.FirstCommit))
    $null = $Builder.AppendLine([string]::Concat("-", " Dirty files: ", $GitStats.DirtyCount))

    if ($GitStats.Contributors.Count -gt 0) {
        $null = $Builder.AppendLine([string]::Concat("-", " Top contributors:"))
        foreach ($contributor in $GitStats.Contributors) {
            $null = $Builder.AppendLine([string]::Concat("  - ", $contributor))
        }
    }
}

function Add-HtmlMetricCard {
    param(
        [System.Text.StringBuilder]$Builder,
        [string]$Label,
        [object]$Value,
        [string]$Hint
    )

    $null = $Builder.AppendLine('<article class="metric-card">')
    $null = $Builder.AppendLine('<div class="metric-label">' + (ConvertTo-HtmlText $Label) + '</div>')
    $null = $Builder.AppendLine('<div class="metric-value">' + (ConvertTo-HtmlText $Value) + '</div>')
    $null = $Builder.AppendLine('<div class="metric-hint">' + (ConvertTo-HtmlText $Hint) + '</div>')
    $null = $Builder.AppendLine('</article>')
}

function Add-HtmlCategoryRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Stats
    )

    foreach ($name in ($Stats.Categories.Keys | Sort-Object)) {
        $entry = $Stats.Categories[$name]
        $null = $Builder.AppendLine('<tr><td>' + (ConvertTo-HtmlText $name) + '</td><td>' + $entry.Files + '</td><td>' + $entry.Lines + '</td></tr>')
    }
}

function Add-HtmlLargestFileRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object]$Stats
    )

    foreach ($file in $Stats.LargestFiles) {
        $null = $Builder.AppendLine('<tr><td><code>' + (ConvertTo-HtmlText $file.Path) + '</code></td><td>' + $file.Lines + '</td><td>' + (ConvertTo-HtmlText $file.Category) + '</td></tr>')
    }
}

function Add-HtmlDeclarationRows {
    param(
        [System.Text.StringBuilder]$Builder,
        [object[]]$Declarations
    )

    foreach ($declaration in ($Declarations | Select-Object -First 140)) {
        $kindClass = ($declaration.Kind -replace "\s+", "-").ToLowerInvariant()
        $null = $Builder.AppendLine(
            '<tr><td><span class="badge badge-' + (ConvertTo-HtmlText $kindClass) + '">' +
            (ConvertTo-HtmlText $declaration.Kind) +
            '</span></td><td><code>' +
            (ConvertTo-HtmlText $declaration.Name) +
            '</code></td><td><code>' +
            (ConvertTo-HtmlText $declaration.Path) +
            '</code></td><td>' +
            $declaration.Line +
            '</td></tr>')
    }
}

function Get-TotalCommits {
    param([object[]]$GitStats)

    $total = 0
    foreach ($entry in $GitStats) {
        if ($entry.Available) {
            $value = 0
            if ([int]::TryParse([string]$entry.CommitCount, [ref]$value)) {
                $total += $value
            }
        }
    }

    return $total
}

$nikreonStats = Get-ScopeStats `
    -Name "Nikreon" `
    -Root $RepoRoot `
    -ExcludedPrefixes @(".git", ".agents", ".codex", ".vs", "build", "out", "external", "vcpkg_installed")

$nikreonUiRoot = Join-Path $RepoRoot "external/NikreonUI"
$nikreonUiStats = Get-ScopeStats `
    -Name "NikreonUI" `
    -Root $nikreonUiRoot `
    -ExcludedPrefixes @(".git", "build", "out", ".vs")

$combinedFiles = $nikreonStats.Files + $nikreonUiStats.Files
$combinedLines = $nikreonStats.Lines + $nikreonUiStats.Lines
$combinedDeclarations = $nikreonStats.Declarations.Count + $nikreonUiStats.Declarations.Count

$mainGitStats = Get-GitStats -Name "Nikreon" -Root $RepoRoot
$uiGitStats = Get-GitStats -Name "NikreonUI" -Root $nikreonUiRoot
$combinedCommits = Get-TotalCommits -GitStats @($mainGitStats, $uiGitStats)

$report = New-Object System.Text.StringBuilder
$null = $report.AppendLine("# Nikreon Project Stats")
$null = $report.AppendLine("")
$null = $report.AppendLine("Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz")")
$null = $report.AppendLine("")
$null = $report.AppendLine("## Summary")
$null = $report.AppendLine("")
$null = $report.AppendLine("| Scope | Files | Lines |")
$null = $report.AppendLine("| --- | ---: | ---: |")
$null = $report.AppendLine("| Nikreon | $($nikreonStats.Files) | $($nikreonStats.Lines) |")
$null = $report.AppendLine("| NikreonUI | $($nikreonUiStats.Files) | $($nikreonUiStats.Lines) |")
$null = $report.AppendLine("| Combined | $combinedFiles | $combinedLines |")
$null = $report.AppendLine("")
$null = $report.AppendLine("- Combined declarations found: $combinedDeclarations")
$null = $report.AppendLine("- Combined commits: $combinedCommits")
$null = $report.AppendLine("")
$null = $report.AppendLine("## Git")
$null = $report.AppendLine("")
$null = $report.AppendLine("### Nikreon")
Add-GitSection -Builder $report -GitStats $mainGitStats
$null = $report.AppendLine("")
$null = $report.AppendLine("### NikreonUI")
Add-GitSection -Builder $report -GitStats $uiGitStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## Nikreon Lines By Category")
$null = $report.AppendLine("")
Add-CategoryTable -Builder $report -Stats $nikreonStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## NikreonUI Lines By Category")
$null = $report.AppendLine("")
Add-CategoryTable -Builder $report -Stats $nikreonUiStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## Largest Nikreon Files")
$null = $report.AppendLine("")
Add-LargestFiles -Builder $report -Stats $nikreonStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## Largest NikreonUI Files")
$null = $report.AppendLine("")
Add-LargestFiles -Builder $report -Stats $nikreonUiStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## Nikreon Classes, Structs, And Enums")
$null = $report.AppendLine("")
Add-DeclarationTable -Builder $report -Stats $nikreonStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## NikreonUI Classes, Structs, And Enums")
$null = $report.AppendLine("")
Add-DeclarationTable -Builder $report -Stats $nikreonUiStats
$null = $report.AppendLine("")
$null = $report.AppendLine("## Notes")
$null = $report.AppendLine("")
$null = $report.AppendLine("- Counts include text/source files and skip generated build output, vcpkg, and git metadata.")
$null = $report.AppendLine("- Combined totals are Nikreon plus `external/NikreonUI`.")

$generatedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$css = @'
:root {
    color-scheme: dark;
    --bg: #11161f;
    --panel: #192231;
    --panel-2: #202b3d;
    --text: #e8edf7;
    --muted: #9eabc1;
    --line: #344158;
    --accent: #7dc4ff;
    --accent-2: #8ee8c3;
    --warn: #f4c67a;
    --danger: #f092a8;
}

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    background: var(--bg);
    color: var(--text);
    font: 14px/1.45 "Segoe UI", system-ui, sans-serif;
}

main {
    width: min(1280px, calc(100vw - 48px));
    margin: 0 auto;
    padding: 32px 0 48px;
}

.hero {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: 24px;
    border-bottom: 1px solid var(--line);
    padding-bottom: 22px;
    margin-bottom: 24px;
}

h1, h2, h3 {
    margin: 0;
    letter-spacing: 0;
}

h1 {
    font-size: 34px;
}

h2 {
    font-size: 20px;
    margin-bottom: 12px;
}

h3 {
    font-size: 16px;
    color: var(--muted);
    margin: 18px 0 10px;
}

.subtitle {
    margin: 8px 0 0;
    color: var(--muted);
}

.generated {
    color: var(--muted);
    text-align: right;
}

.metrics {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 12px;
    margin-bottom: 24px;
}

.metric-card, .section {
    background: var(--panel);
    border: 1px solid var(--line);
    border-radius: 8px;
}

.metric-card {
    padding: 16px;
}

.metric-label, .metric-hint {
    color: var(--muted);
}

.metric-value {
    font-size: 28px;
    font-weight: 700;
    margin: 6px 0;
}

.grid {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 16px;
}

.section {
    padding: 18px;
    margin-bottom: 16px;
    overflow: hidden;
}

table {
    width: 100%;
    border-collapse: collapse;
}

th, td {
    border-bottom: 1px solid var(--line);
    padding: 8px 10px;
    text-align: left;
    vertical-align: top;
}

th {
    color: var(--muted);
    font-weight: 600;
    background: var(--panel-2);
}

td:nth-child(2), td:nth-child(3), td:nth-child(4),
th:nth-child(2), th:nth-child(3), th:nth-child(4) {
    text-align: right;
}

code {
    color: #dbe7ff;
    font-family: "Cascadia Code", Consolas, monospace;
    font-size: 12px;
}

.git-list {
    margin: 0;
    padding-left: 18px;
    color: var(--muted);
}

.badge {
    display: inline-block;
    min-width: 76px;
    text-align: center;
    border-radius: 999px;
    padding: 2px 8px;
    font-size: 12px;
    color: #07111f;
    background: var(--accent);
}

.badge-struct {
    background: var(--accent-2);
}

.badge-enum-class {
    background: var(--warn);
}

.badge-class {
    background: var(--accent);
}

.notes {
    color: var(--muted);
}

@media (max-width: 900px) {
    main {
        width: min(100vw - 24px, 1280px);
        padding-top: 18px;
    }

    .hero {
        display: block;
    }

    .generated {
        text-align: left;
        margin-top: 12px;
    }

    .metrics,
    .grid {
        grid-template-columns: 1fr;
    }
}
'@

$html = New-Object System.Text.StringBuilder
$null = $html.AppendLine('<!doctype html>')
$null = $html.AppendLine('<html lang="en">')
$null = $html.AppendLine('<head>')
$null = $html.AppendLine('<meta charset="utf-8">')
$null = $html.AppendLine('<meta name="viewport" content="width=device-width, initial-scale=1">')
$null = $html.AppendLine('<title>Nikreon Project Stats</title>')
$null = $html.AppendLine('<style>')
$null = $html.AppendLine($css)
$null = $html.AppendLine('</style>')
$null = $html.AppendLine('</head>')
$null = $html.AppendLine('<body>')
$null = $html.AppendLine('<main>')
$null = $html.AppendLine('<header class="hero">')
$null = $html.AppendLine('<div><h1>Nikreon Project Stats</h1><p class="subtitle">Nikreon plus NikreonUI, generated from the current workspace.</p></div>')
$null = $html.AppendLine('<div class="generated">Generated<br>' + (ConvertTo-HtmlText $generatedAt) + '</div>')
$null = $html.AppendLine('</header>')
$null = $html.AppendLine('<section class="metrics">')
Add-HtmlMetricCard -Builder $html -Label "Combined Lines" -Value $combinedLines -Hint "Nikreon + NikreonUI"
Add-HtmlMetricCard -Builder $html -Label "Combined Files" -Value $combinedFiles -Hint "tracked project text/source files"
Add-HtmlMetricCard -Builder $html -Label "Declarations" -Value $combinedDeclarations -Hint "classes, structs, enum classes"
Add-HtmlMetricCard -Builder $html -Label "Commits" -Value $combinedCommits -Hint "combined git history"
$null = $html.AppendLine('</section>')

$null = $html.AppendLine('<section class="grid">')
$null = $html.AppendLine('<article class="section"><h2>Summary</h2><table><thead><tr><th>Scope</th><th>Files</th><th>Lines</th><th>Declarations</th></tr></thead><tbody>')
$null = $html.AppendLine('<tr><td>Nikreon</td><td>' + $nikreonStats.Files + '</td><td>' + $nikreonStats.Lines + '</td><td>' + $nikreonStats.Declarations.Count + '</td></tr>')
$null = $html.AppendLine('<tr><td>NikreonUI</td><td>' + $nikreonUiStats.Files + '</td><td>' + $nikreonUiStats.Lines + '</td><td>' + $nikreonUiStats.Declarations.Count + '</td></tr>')
$null = $html.AppendLine('<tr><td>Combined</td><td>' + $combinedFiles + '</td><td>' + $combinedLines + '</td><td>' + $combinedDeclarations + '</td></tr>')
$null = $html.AppendLine('</tbody></table></article>')

$null = $html.AppendLine('<article class="section"><h2>Git</h2>')
$null = $html.AppendLine('<h3>Nikreon</h3><ul class="git-list">')
$null = $html.AppendLine('<li>Branch: ' + (ConvertTo-HtmlText $mainGitStats.Branch) + '</li>')
$null = $html.AppendLine('<li>Commits: ' + (ConvertTo-HtmlText $mainGitStats.CommitCount) + '</li>')
$null = $html.AppendLine('<li>Latest: ' + (ConvertTo-HtmlText $mainGitStats.LatestCommit) + '</li>')
$null = $html.AppendLine('<li>Dirty files: ' + (ConvertTo-HtmlText $mainGitStats.DirtyCount) + '</li>')
$null = $html.AppendLine('</ul><h3>NikreonUI</h3><ul class="git-list">')
$null = $html.AppendLine('<li>Branch: ' + (ConvertTo-HtmlText $uiGitStats.Branch) + '</li>')
$null = $html.AppendLine('<li>Commits: ' + (ConvertTo-HtmlText $uiGitStats.CommitCount) + '</li>')
$null = $html.AppendLine('<li>Latest: ' + (ConvertTo-HtmlText $uiGitStats.LatestCommit) + '</li>')
$null = $html.AppendLine('<li>Dirty files: ' + (ConvertTo-HtmlText $uiGitStats.DirtyCount) + '</li>')
$null = $html.AppendLine('</ul></article>')
$null = $html.AppendLine('</section>')

$null = $html.AppendLine('<section class="grid">')
$null = $html.AppendLine('<article class="section"><h2>Nikreon Lines By Category</h2><table><thead><tr><th>Category</th><th>Files</th><th>Lines</th></tr></thead><tbody>')
Add-HtmlCategoryRows -Builder $html -Stats $nikreonStats
$null = $html.AppendLine('</tbody></table></article>')
$null = $html.AppendLine('<article class="section"><h2>NikreonUI Lines By Category</h2><table><thead><tr><th>Category</th><th>Files</th><th>Lines</th></tr></thead><tbody>')
Add-HtmlCategoryRows -Builder $html -Stats $nikreonUiStats
$null = $html.AppendLine('</tbody></table></article>')
$null = $html.AppendLine('</section>')

$null = $html.AppendLine('<section class="grid">')
$null = $html.AppendLine('<article class="section"><h2>Largest Nikreon Files</h2><table><thead><tr><th>File</th><th>Lines</th><th>Category</th></tr></thead><tbody>')
Add-HtmlLargestFileRows -Builder $html -Stats $nikreonStats
$null = $html.AppendLine('</tbody></table></article>')
$null = $html.AppendLine('<article class="section"><h2>Largest NikreonUI Files</h2><table><thead><tr><th>File</th><th>Lines</th><th>Category</th></tr></thead><tbody>')
Add-HtmlLargestFileRows -Builder $html -Stats $nikreonUiStats
$null = $html.AppendLine('</tbody></table></article>')
$null = $html.AppendLine('</section>')

$null = $html.AppendLine('<section class="section"><h2>Nikreon Declarations</h2><table><thead><tr><th>Kind</th><th>Name</th><th>File</th><th>Line</th></tr></thead><tbody>')
Add-HtmlDeclarationRows -Builder $html -Declarations $nikreonStats.Declarations
$null = $html.AppendLine('</tbody></table></section>')

$null = $html.AppendLine('<section class="section"><h2>NikreonUI Declarations</h2><table><thead><tr><th>Kind</th><th>Name</th><th>File</th><th>Line</th></tr></thead><tbody>')
Add-HtmlDeclarationRows -Builder $html -Declarations $nikreonUiStats.Declarations
$null = $html.AppendLine('</tbody></table></section>')

$null = $html.AppendLine('<section class="section notes"><h2>Notes</h2><p>Counts include text/source files and skip generated build output, dependency folders, vcpkg, and git metadata. Declaration scanning is intentionally lightweight and counts common C++ class, struct, and enum class definitions.</p></section>')
$null = $html.AppendLine('</main>')
$null = $html.AppendLine('</body>')
$null = $html.AppendLine('</html>')

Set-Content -Path $OutputFullPath -Value $report.ToString() -Encoding UTF8
Set-Content -Path $OutputHtmlFullPath -Value $html.ToString() -Encoding UTF8
Write-Host "Project stats written to $OutputFullPath"
Write-Host "Project stats HTML written to $OutputHtmlFullPath"
