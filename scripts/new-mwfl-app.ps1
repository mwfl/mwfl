[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[A-Za-z][A-Za-z0-9_-]*$')]
    [string]$Name,

    [Parameter(Mandatory)]
    [string]$Destination,

    [ValidateSet('basic', 'form')]
    [string]$Template = 'basic',

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$templateDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) "templates/$Template-app"
if (-not (Test-Path -LiteralPath $templateDirectory -PathType Container)) {
    throw "Unknown or missing template: $Template"
}

$destinationRoot = [System.IO.Path]::GetFullPath($Destination)
$projectDirectory = Join-Path $destinationRoot $Name
if (Test-Path -LiteralPath $projectDirectory) {
    if (-not $Force) {
        throw "Destination already exists: $projectDirectory. Pass -Force only to replace an empty directory."
    }
    if (@(Get-ChildItem -LiteralPath $projectDirectory -Force).Count -ne 0) {
        throw "Refusing to replace non-empty destination: $projectDirectory"
    }
} else {
    New-Item -ItemType Directory -Path $projectDirectory | Out-Null
}

Copy-Item -LiteralPath (Join-Path $templateDirectory 'main.cpp') -Destination $projectDirectory
Copy-Item -LiteralPath (Join-Path $templateDirectory 'CMakeLists.txt') -Destination $projectDirectory
Copy-Item -LiteralPath (Join-Path $templateDirectory 'README.md') -Destination $projectDirectory
Copy-Item -LiteralPath (Join-Path (Split-Path $PSScriptRoot -Parent) 'templates/basic-app/app.manifest') -Destination $projectDirectory

$cmakePath = Join-Path $projectDirectory 'CMakeLists.txt'
$cmake = [System.IO.File]::ReadAllText($cmakePath)
$target = ($Name -replace '-', '_')
$cmake = $cmake.Replace('mwfl_basic_app', $target).Replace('mwfl_form_app', $target)
$cmake = $cmake.Replace('../basic-app/app.manifest', 'app.manifest')
[System.IO.File]::WriteAllText($cmakePath, $cmake, [System.Text.UTF8Encoding]::new($false))

$mainPath = Join-Path $projectDirectory 'main.cpp'
$main = [System.IO.File]::ReadAllText($mainPath)
$main = $main.Replace('mwfl starter', $Name)
[System.IO.File]::WriteAllText($mainPath, $main, [System.Text.UTF8Encoding]::new($false))

Write-Host "Created $Template mwfl application: $projectDirectory"
Write-Host "Configure: cmake -S `"$projectDirectory`" -B `"$projectDirectory/build`" -G `"Visual Studio 18 2026`" -A x64"
Write-Host "Build:     cmake --build `"$projectDirectory/build`" --config Debug"
