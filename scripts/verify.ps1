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

$toolchain = Resolve-MwtlToolchain -VisualStudio $VisualStudio -Architecture $Architecture
$presets = Get-MwtlPresetNames -Toolchain $toolchain -Architecture $Architecture
$configureArguments = @('--preset', $presets.Configure)
if ($Offline) {
    $wtl = [Environment]::GetEnvironmentVariable('MWTL_WTL_SOURCE_DIR')
    $wil = [Environment]::GetEnvironmentVariable('MWTL_WIL_SOURCE_DIR')
    if (-not $wtl -or -not $wil) {
        throw '-Offline requires MWTL_WTL_SOURCE_DIR and MWTL_WIL_SOURCE_DIR.'
    }
    $configureArguments += @(
        '-DMWTL_DEPENDENCY_MODE=SYSTEM',
        "-DMWTL_WTL_SOURCE_DIR=$wtl",
        "-DMWTL_WIL_SOURCE_DIR=$wil")
}

Invoke-Checked $toolchain.CMake $configureArguments

switch ($Mode) {
    'Fast' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug, '--parallel', "$Jobs")
        Invoke-Checked $toolchain.CTest @('--preset', $presets.Debug, '-L', 'fast')
    }
    'Docs' {
        Invoke-Checked $toolchain.CMake @('--build', '--preset', $presets.Debug,
            '--target', 'mwtl_agent_api_probe', '--parallel', "$Jobs")
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

Write-Host "mwtl verification completed: $Mode, Visual Studio $($toolchain.Year), $Architecture"
