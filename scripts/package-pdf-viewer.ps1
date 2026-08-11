[CmdletBinding()]
param(
    [ValidateSet('Auto', '2022', '2026')]
    [string]$VisualStudio = 'Auto',
    [ValidatePattern('^[0-9]+[.][0-9]+[.][0-9]+$')]
    [string]$Version = '0.1.0',
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

$toolchain = Resolve-MwflToolchain -VisualStudio $VisualStudio -Architecture x64
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $projectRoot "build\pdf-viewer\vs$($toolchain.Year)-x64"
$stageRoot = Join-Path $buildRoot 'stage'
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$packageName = "mwfl-pdf-viewer-$Version-windows-x64"
$archive = Join-Path $resolvedOutput "$packageName.zip"
$generator = "Visual Studio $($toolchain.Major) $($toolchain.Year)"

Invoke-Checked $toolchain.CMake @(
    '-S', $projectRoot, '-B', $buildRoot, '-G', $generator, '-A', 'x64',
    '-DMWFL_BUILD_EXAMPLES=ON', '-DMWFL_BUILD_TESTS=ON',
    '-DMWFL_BUILD_WEBVIEW2=ON')
Invoke-Checked $toolchain.CMake @(
    '--build', $buildRoot, '--config', 'Release', '--target', 'mwfl_pdf_viewer')
Invoke-Checked $toolchain.CTest @(
    '--test-dir', $buildRoot, '-C', 'Release',
    '-R', 'mwfl[.]pdf_viewer_gui', '--output-on-failure')

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
    '--component', 'pdf_viewer', '--prefix', $stageRoot)

$readme = Join-Path $stageRoot 'README.txt'
@"
MWFL PDF Viewer $Version

Run bin\mwfl_pdf_viewer.exe.

Requirements:
- Windows 10 1809 or newer, x64
- Microsoft Edge WebView2 Evergreen Runtime

Documents stay local. Open PDF files with Ctrl+O or drag them into the window.
The built-in PDF surface provides page navigation, zoom, search, and printing.

Source and build instructions: https://github.com/mwfl/mwfl
"@ | Set-Content -LiteralPath $readme -Encoding utf8

if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -Path (Join-Path $stageRoot '*') -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "Created $archive"
Write-Host "SHA256 $hash"
