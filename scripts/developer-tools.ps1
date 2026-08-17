Set-StrictMode -Version Latest

function Get-MwflVisualStudioInstallations {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        return @()
    }

    $items = & $vswhere -products * -format json -utf8 | ConvertFrom-Json
    return @($items | Where-Object {
        $major = [int]([version]$_.installationVersion).Major
        $major -in @(17, 18)
    } | ForEach-Object {
        $major = [int]([version]$_.installationVersion).Major
        $cmake = Join-Path $_.installationPath `
            'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        $ctest = Join-Path $_.installationPath `
            'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
        $toolRoots = @(Get-ChildItem (Join-Path $_.installationPath 'VC\Tools\MSVC') `
            -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)
        $toolRoot = $toolRoots | Select-Object -First 1
        $compilers = if ($toolRoot) {
            @(Get-ChildItem $toolRoot.FullName -Filter cl.exe -Recurse -ErrorAction SilentlyContinue)
        } else { @() }
        $cppX64 = $compilers | Where-Object FullName -Match `
            '\\bin\\Host(x64|arm64)\\x64\\cl[.]exe$' | Select-Object -First 1
        $cppArm64 = $compilers | Where-Object FullName -Match `
            '\\bin\\Host(x64|arm64)\\arm64\\cl[.]exe$' | Select-Object -First 1
        [pscustomobject]@{
            Year = if ($major -eq 18) { 2026 } else { 2022 }
            Major = $major
            Version = $_.installationVersion
            Path = $_.installationPath
            CMake = if (Test-Path -LiteralPath $cmake) { $cmake } else { $null }
            CTest = if (Test-Path -LiteralPath $ctest) { $ctest } else { $null }
            CppX64 = if ($cppX64) { $cppX64.FullName } else { $null }
            CppARM64 = if ($cppArm64) { $cppArm64.FullName } else { $null }
        }
    } | Sort-Object Year -Descending)
}

function Resolve-MwflToolchain {
    param(
        [ValidateSet('Auto', '2022', '2026')]
        [string]$VisualStudio = 'Auto',
        [ValidateSet('x64', 'ARM64')]
        [string]$Architecture = 'x64'
    )

    $installations = @(Get-MwflVisualStudioInstallations)
    if ($VisualStudio -eq 'Auto') {
        $selected = $installations | Select-Object -First 1
    } else {
        $selected = $installations |
            Where-Object Year -eq ([int]$VisualStudio) |
            Select-Object -First 1
    }
    if (-not $selected) {
        throw "Visual Studio $VisualStudio was not found. Install VS 2022 or VS 2026 with Desktop development with C++ and a Windows SDK."
    }
    if (-not $selected.CMake -or -not $selected.CTest) {
        throw "Visual Studio $($selected.Year) is missing CMake tools. Install component Microsoft.VisualStudio.Component.VC.CMake.Project or import .vsconfig."
    }
    $compiler = if ($Architecture -eq 'ARM64') { $selected.CppARM64 } else { $selected.CppX64 }
    if (-not $compiler) {
        $component = if ($Architecture -eq 'ARM64') {
            'Microsoft.VisualStudio.Component.VC.Tools.ARM64'
        } else {
            'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
        }
        throw "Visual Studio $($selected.Year) is missing the $Architecture C++ compiler. Install component $component or import .vsconfig."
    }
    $sdkRoot = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits\10\Include'
    $windowsHeader = Get-ChildItem $sdkRoot -Filter windows.h -Recurse -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '\\um\\windows[.]h$' | Select-Object -First 1
    if (-not $windowsHeader) {
        throw 'A Windows SDK was not found. Install Microsoft.VisualStudio.Component.Windows11SDK.26100 or import .vsconfig.'
    }
    $selected | Add-Member -NotePropertyName WindowsSDK `
        -NotePropertyValue $windowsHeader.Directory.Parent.Name -Force
    $selected | Add-Member -NotePropertyName Compiler `
        -NotePropertyValue $compiler -Force
    return $selected
}

function Get-MwflPresetNames {
    param(
        [Parameter(Mandatory)]$Toolchain,
        [ValidateSet('x64', 'ARM64')]
        [string]$Architecture = 'x64'
    )
    $archName = $Architecture.ToLowerInvariant()
    $root = "vs$($Toolchain.Year)-$archName"
    return [pscustomobject]@{
        Configure = $root
        Debug = "$root-debug"
        Release = "$root-release"
    }
}

function Reset-MwflStalePresetCache {
    # A repository that was moved or cloned into a new path keeps a
    # CMakeCache.txt whose recorded build directory no longer matches the
    # preset binary directory; CMake then refuses to configure. Remove the
    # stale build tree so the caller's configure step starts clean.
    param(
        [Parameter(Mandatory)][string]$RepositoryRoot,
        [Parameter(Mandatory)][string]$ConfigurePreset
    )
    $buildDirectory = Join-Path $RepositoryRoot "build/presets/$ConfigurePreset"
    $cachePath = Join-Path $buildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { return }
    $recorded = $null
    foreach ($line in Get-Content -LiteralPath $cachePath) {
        if ($line -match '^# For build in directory: (.+)$') {
            $recorded = $Matches[1].Trim()
            break
        }
    }
    if (-not $recorded) { return }
    $normalize = {
        param([string]$path)
        [System.IO.Path]::GetFullPath($path).TrimEnd('\', '/').Replace('/', '\').ToLowerInvariant()
    }
    if ((& $normalize $recorded) -eq (& $normalize $buildDirectory)) { return }
    Write-Host "Removing stale CMake cache generated for '$recorded' at '$buildDirectory'"
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}
