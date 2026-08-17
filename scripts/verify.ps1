[CmdletBinding()]
param(
    [ValidateSet('Fast', 'Full', 'Docs', 'Package')]
    [string]$Mode = 'Fast',
    [ValidateSet('Auto', '2022', '2026')]
    [string]$VisualStudio = 'Auto',
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64',
    [ValidateRange(1, 64)]
    [int]$Jobs = 2,
    [string]$BuildRoot,
    [switch]$Offline
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
$presets = Get-MwflPresetNames -Toolchain $toolchain -Architecture $Architecture
$repositoryRoot = Split-Path $PSScriptRoot -Parent
$configureArguments = @('--preset', $presets.Configure)
if ($BuildRoot) {
    $resolvedBuildRoot = [IO.Path]::GetFullPath($BuildRoot, $repositoryRoot)
    New-Item -ItemType Directory -Path $resolvedBuildRoot -Force | Out-Null
    $writeProbe = Join-Path $resolvedBuildRoot '.mwfl-write-probe'
    try {
        [IO.File]::WriteAllText($writeProbe, 'write test')
        Remove-Item -LiteralPath $writeProbe -Force
    } catch {
        throw "Build root is not writable: $resolvedBuildRoot. $($_.Exception.Message)"
    }
    $configureArguments += @('-B', $resolvedBuildRoot)
} else {
    Reset-MwflStalePresetCache -RepositoryRoot $repositoryRoot -ConfigurePreset $presets.Configure
}
if ($Offline) {
    $wil = [Environment]::GetEnvironmentVariable('MWFL_WIL_SOURCE_DIR')
    if (-not $wil) {
        throw '-Offline requires MWFL_WIL_SOURCE_DIR.'
    }
    $configureArguments += @(
        '-DMWFL_DEPENDENCY_MODE=SYSTEM',
        "-DMWFL_WIL_SOURCE_DIR=$wil")
}

Invoke-Checked $toolchain.CMake $configureArguments

function Invoke-MwflBuild {
    param([ValidateSet('Debug', 'Release')] [string]$Configuration,
          [string[]]$ExtraArguments = @())
    if ($BuildRoot) {
        $arguments = @('--build', $resolvedBuildRoot, '--config', $Configuration,
            '--parallel', "$Jobs") + $ExtraArguments
    } else {
        $preset = if ($Configuration -eq 'Debug') { $presets.Debug } else { $presets.Release }
        $arguments = @('--build', '--preset', $preset, '--parallel', "$Jobs") +
            $ExtraArguments
    }
    Invoke-Checked $toolchain.CMake $arguments
}

function Invoke-MwflTest {
    param([ValidateSet('Debug', 'Release')] [string]$Configuration,
          [string[]]$ExtraArguments = @())
    if ($BuildRoot) {
        $arguments = @('--test-dir', $resolvedBuildRoot, '-C', $Configuration) +
            $ExtraArguments
    } else {
        $preset = if ($Configuration -eq 'Debug') { $presets.Debug } else { $presets.Release }
        $arguments = @('--preset', $preset) + $ExtraArguments
    }
    Invoke-Checked $toolchain.CTest $arguments
}

switch ($Mode) {
    'Fast' {
        Invoke-MwflBuild Debug
        Invoke-MwflTest Debug @('-L', 'fast')
    }
    'Docs' {
        Invoke-MwflBuild Debug @('--target', 'mwfl_agent_api_probe')
        Invoke-MwflTest Debug @('-L', 'docs|metadata')
    }
    'Package' {
        Invoke-MwflBuild Debug
        Invoke-MwflTest Debug @('-L', 'package')
    }
    'Full' {
        Invoke-MwflBuild Debug
        Invoke-MwflTest Debug
        if ($Architecture -eq 'x64') {
            Invoke-MwflBuild Release
            Invoke-MwflTest Release
        } else {
            Write-Host 'Release ARM64 has no repository preset; Debug validation completed.'
        }
    }
}

Write-Host "mwfl verification completed: $Mode, Visual Studio $($toolchain.Year), $Architecture"
