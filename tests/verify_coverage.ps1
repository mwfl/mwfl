param(
    [Parameter(Mandatory = $true)] [string] $Report,
    [Parameter(Mandatory = $true)] [string] $ProjectRoot,
    [double] $MinimumPercent = 74.0
)

$ErrorActionPreference = 'Stop'
[xml] $coverage = Get-Content -LiteralPath $Report
$root = (Resolve-Path -LiteralPath $ProjectRoot).Path.TrimEnd('\')
$sourcePrefixes = @("$root\src\", "$root\include\mwfl\")
$lines = @{}

foreach ($class in $coverage.coverage.packages.package.classes.class) {
    $filename = [string] $class.filename
    $isProjectSource = $false
    foreach ($prefix in $sourcePrefixes) {
        if ($filename.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            $isProjectSource = $true
            break
        }
    }
    if (-not $isProjectSource) { continue }

    foreach ($line in $class.lines.line) {
        $key = "${filename}:$($line.number)"
        $hits = [int] $line.hits
        if (-not $lines.ContainsKey($key) -or $hits -gt $lines[$key]) {
            $lines[$key] = $hits
        }
    }
}

if ($lines.Count -eq 0) {
    throw 'Coverage report contains no mwfl source lines.'
}

$covered = @($lines.Values | Where-Object { $_ -gt 0 }).Count
$percent = 100.0 * $covered / $lines.Count
Write-Host ('mwfl source coverage: {0:N2}% ({1}/{2} lines)' -f
    $percent, $covered, $lines.Count)
if ($percent -lt $MinimumPercent) {
    throw ('Coverage {0:N2}% is below the {1:N2}% floor.' -f
        $percent, $MinimumPercent)
}
