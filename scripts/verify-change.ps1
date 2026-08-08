[CmdletBinding()]
param(
    [string[]]$ChangedFiles,
    [ValidateSet('Auto', '2022', '2026')][string]$VisualStudio = 'Auto',
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [switch]$Execute
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $ChangedFiles) {
    $ChangedFiles = @(git -C $root diff --name-only HEAD)
    $ChangedFiles += @(git -C $root ls-files --others --exclude-standard)
}
$ChangedFiles = @($ChangedFiles | ForEach-Object { $_.Replace('\', '/').TrimStart('.', '/') } | Where-Object { $_ } | Sort-Object -Unique)
if ($ChangedFiles.Count -eq 0) {
    Write-Host 'No changed files; no validation selected.'
    return
}

$matrix = Get-Content (Join-Path $root 'docs/change-matrix.json') -Raw | ConvertFrom-Json
$selected = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in $ChangedFiles) {
    $matched = $false
    foreach ($entry in $matrix.paths) {
        if ($file -like $entry.pattern) {
            foreach ($validation in $entry.validation) { [void]$selected.Add($validation) }
            $matched = $true
        }
    }
    if (-not $matched) { foreach ($validation in $matrix.default) { [void]$selected.Add($validation) } }
}

$mode = 'Fast'
if ($selected.Contains('full')) { $mode = 'Full' }
elseif ($selected.Contains('package')) { $mode = 'Package' }
elseif ($selected.Contains('docs') -and -not ($selected.Contains('fast') -or $selected.Contains('unit') -or $selected.Contains('lifecycle'))) { $mode = 'Docs' }

[pscustomobject]@{ Files = $ChangedFiles; Validation = @($selected | Sort-Object); VerifyMode = $mode }
if ($Execute) {
    & (Join-Path $PSScriptRoot 'verify.ps1') -Mode $mode -VisualStudio $VisualStudio -Architecture $Architecture
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
