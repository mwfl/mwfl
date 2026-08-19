[CmdletBinding()]
param(
    [ValidateSet('Auto', '2022', '2026')]
    [string]$VisualStudio = 'Auto',
    [ValidatePattern('^[0-9]+[.][0-9]+[.][0-9]+$')]
    [string]$Version = '0.2.1',
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\build\packages')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'developer-tools.ps1')

function Invoke-Checked {
    param([string]$File, [string[]]$Arguments)
    Write-Host "> $File $($Arguments -join ' ')"
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $File"
    }
}

$toolchain = Resolve-MwflToolchain -VisualStudio $VisualStudio -Architecture $Architecture
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$architectureSlug = $Architecture.ToLowerInvariant()
$buildRoot = Join-Path $projectRoot "build\notepad\vs$($toolchain.Year)-$architectureSlug"
$stageRoot = Join-Path $buildRoot 'stage'
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$packageName = "mwfl-notepad-$Version-windows-$architectureSlug"
$archive = Join-Path $resolvedOutput "$packageName.zip"
$generator = "Visual Studio $($toolchain.Major) $($toolchain.Year)"

Invoke-Checked $toolchain.CMake @(
    '-S', $projectRoot, '-B', $buildRoot, '-G', $generator, '-A', $Architecture,
    '-DMWFL_BUILD_EXAMPLES=ON', '-DMWFL_BUILD_TESTS=ON')
Invoke-Checked $toolchain.CMake @(
    '--build', $buildRoot, '--config', 'Release', '--target', 'mwfl_notepad')
Invoke-Checked $toolchain.CTest @(
    '--test-dir', $buildRoot, '-C', 'Release',
    '-R', 'mwfl[.]notepad_gui', '--output-on-failure')

if (Test-Path -LiteralPath $stageRoot) {
    $resolvedStage = [System.IO.Path]::GetFullPath($stageRoot)
    $resolvedBuild = [System.IO.Path]::GetFullPath($buildRoot)
    if (-not $resolvedStage.StartsWith($resolvedBuild + [System.IO.Path]::DirectorySeparatorChar,
                                       [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to replace staging directory outside the build tree: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot, $resolvedOutput | Out-Null
Invoke-Checked $toolchain.CMake @(
    '--install', $buildRoot, '--config', 'Release',
    '--component', 'notepad', '--prefix', $stageRoot)

@"
MWFL Notepad $Version

Run bin\mwfl_notepad.exe.

Requirements:
- Windows 10 1809 or newer, $Architecture

The editor preserves UTF-8 and UTF-16 encodings, rejects malformed input,
uses atomic saves with external-change detection, and keeps documents local.

Source and build instructions: https://github.com/mwfl/mwfl
"@ | Set-Content -LiteralPath (Join-Path $stageRoot 'README.txt') -Encoding utf8

if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stageRoot '*') -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Created $archive"
Write-Host "SHA256 $hash"
