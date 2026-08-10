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
$configureArguments = @('--preset', $presets.Configure)
if ($Offline) {
    $wtl = [Environment]::GetEnvironmentVariable('MWFL_WTL_SOURCE_DIR')
    $wil = [Environment]::GetEnvironmentVariable('MWFL_WIL_SOURCE_DIR')
    if (-not $wtl -or -not $wil) {
        throw '-Offline requires MWFL_WTL_SOURCE_DIR and MWFL_WIL_SOURCE_DIR.'
    }
    $configureArguments += @(
        '-DMWFL_DEPENDENCY_MODE=SYSTEM',
        "-DMWFL_WTL_SOURCE_DIR=$wtl",
        "-DMWFL_WIL_SOURCE_DIR=$wil")
}

Invoke-Checked $toolchain.CMake $configureArguments

switch ($Mode) {
    'Fast' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug, '--parallel', "$Jobs")
        Invoke-Checked $toolchain.CTest @('--preset', $presets.Debug, '-L', 'fast')
    }
    'Docs' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug,
            '--target', 'mwfl_agent_api_probe', '--parallel', "$Jobs")
        Invoke-Checked $toolchain.CTest @('--preset', $presets.Debug, '-L', 'docs|metadata')
    }
    'Package' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug, '--parallel', "$Jobs")
        Invoke-Checked $toolchain.CTest @('--preset', $presets.Debug, '-L', 'package')
    }
    'Full' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug, '--parallel', "$Jobs")
        Invoke-Checked $toolchain.CTest @('--preset', $presets.Debug)
        if ($Architecture -eq 'x64') {
            Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Release, '--parallel', "$Jobs")
            Invoke-Checked $toolchain.CTest @('--preset', $presets.Release)
        } else {
            Write-Host 'Release ARM64 has no repository preset; Debug validation completed.'
        }
    }
}

Write-Host "mwfl verification completed: $Mode, Visual Studio $($toolchain.Year), $Architecture"
