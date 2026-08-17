[CmdletBinding()]
param(
    [ValidateSet('Auto', '2022', '2026')]
    [string]$VisualStudio = 'Auto',
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'developer-tools.ps1')

Write-Host 'mwfl developer environment'
Write-Host "  OS: $([Environment]::OSVersion.VersionString)"
Write-Host "  Process architecture: $([Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture)"

$installations = @(Get-MwflVisualStudioInstallations)
if ($installations.Count -eq 0) {
    throw 'No supported Visual Studio installation was found.'
}
foreach ($item in $installations) {
    $features = @(
        if ($item.CMake -and $item.CTest) { 'CMake' } else { 'no CMake' }
        if ($item.CppX64) { 'x64 C++' } else { 'no x64 C++' }
        if ($item.CppARM64) { 'ARM64 C++' } else { 'no ARM64 C++' })
    Write-Host "  Visual Studio $($item.Year): $($item.Version) ($($features -join ', '))"
}

$selected = Resolve-MwflToolchain -VisualStudio $VisualStudio -Architecture $Architecture
$presets = Get-MwflPresetNames -Toolchain $selected -Architecture $Architecture
Write-Host "  Selected: Visual Studio $($selected.Year), $Architecture"
Write-Host "  CMake: $($selected.CMake)"
Write-Host "  Compiler: $($selected.Compiler)"
Write-Host "  Windows SDK: $($selected.WindowsSDK)"
Write-Host "  Configure preset: $($presets.Configure)"
Write-Host "  Fast validation: ./scripts/verify.ps1 -Mode Fast -VisualStudio $($selected.Year) -Architecture $Architecture"
Write-Host '  Reproduce this environment: open .vsconfig with Visual Studio Installer'

$missingOffline = @()
foreach ($name in @('MWFL_WIL_SOURCE_DIR')) {
    if (-not [Environment]::GetEnvironmentVariable($name)) {
        $missingOffline += $name
    }
}
if ($missingOffline.Count -eq 0) {
    Write-Host '  Offline dependency paths: ready'
} else {
    Write-Host "  Offline dependency paths: optional; unset $($missingOffline -join ', ')"
}
