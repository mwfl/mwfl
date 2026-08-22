[CmdletBinding()]
param(
    [string]$MwflSource = (Split-Path $PSScriptRoot -Parent),
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('Auto', '2022', '2026')]
    [string]$VisualStudio = 'Auto',
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [string[]]$Application
)

$ErrorActionPreference = 'Stop'
$mwflRoot = [IO.Path]::GetFullPath($MwflSource)
$familyRoot = Split-Path $mwflRoot -Parent
. (Join-Path $mwflRoot 'scripts/developer-tools.ps1')
$toolchain = Resolve-MwflToolchain -VisualStudio $VisualStudio -Architecture x64
$manifest = Get-Content (Join-Path $mwflRoot 'docs/downstream-consumers.json') -Raw |
    ConvertFrom-Json
$consumers = @($manifest.consumers)
if ($Application) {
    $consumers = @($consumers | Where-Object { $_.name -in $Application })
    $missing = @($Application | Where-Object { $_ -notin $consumers.name })
    if ($missing) { throw "Unknown downstream application(s): $($missing -join ', ')" }
}

$failures = @()
foreach ($consumer in $consumers) {
    $source = Join-Path $familyRoot $consumer.name
    if (-not (Test-Path (Join-Path $source 'CMakeLists.txt'))) {
        $failures += "$($consumer.name): checkout not found at $source"
        continue
    }
    Write-Host "==> $($consumer.name)"
    Push-Location $source
    try {
        & $toolchain.CMake --preset $consumer.preset
        if ($LASTEXITCODE -ne 0) {
            $failures += "$($consumer.name): configure failed"
            continue
        }
        $preset = "$($consumer.preset)-$($Configuration.ToLowerInvariant())"
        & $toolchain.CMake --build --preset $preset --parallel $Jobs
        if ($LASTEXITCODE -ne 0) {
            $failures += "$($consumer.name): build failed"
            continue
        }
        & $toolchain.CTest --preset $preset --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            $failures += "$($consumer.name): tests failed"
        }
    } finally {
        Pop-Location
    }
}

if ($failures) {
    throw "Downstream verification failed:`n$($failures -join "`n")"
}
Write-Host "Verified $($consumers.Count) downstream application(s) against $mwflRoot."
