param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [switch]$MeasureOnly
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe was not found' }
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'A supported Visual C++ installation was not found' }
$developer = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$environment = & $env:ComSpec /d /s /c "`"$developer`" -no_logo -arch=x64 >nul && set"
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        Set-Item -Path "Env:$($line.Substring(0, $separator))" -Value $line.Substring($separator + 1)
    }
}

$work = Join-Path $ProjectRoot 'build\compile-cost'
New-Item -ItemType Directory -Path $work -Force | Out-Null
$profiles = @(
    @{ Name = 'starter'; Includes = @('mwfl/application.h', 'mwfl/window.h', 'mwfl/controls.h', 'mwfl/layout.h'); Limit = 9000000 },
    @{ Name = 'umbrella'; Includes = @('mwfl/mwfl.h'); Limit = 11400000 }
)
foreach ($profile in $profiles) {
    $source = Join-Path $work "$($profile.Name).cpp"
    $output = Join-Path $work "$($profile.Name).i"
    $translationUnit = @($profile.Includes | ForEach-Object { "#include <$_>" })
    $translationUnit += 'int main() { return 0; }'
    $translationUnit | Set-Content -LiteralPath $source -Encoding utf8
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    & cl.exe /nologo /std:c++20 /Zc:preprocessor /P "/Fi$output" "/I$ProjectRoot\include" $source
    if ($LASTEXITCODE -ne 0) { throw "Preprocessing $($profile.Name) failed" }
    $watch.Stop()
    $bytes = (Get-Item -LiteralPath $output).Length
    Write-Host "compile-cost $($profile.Name): preprocessed=$bytes bytes elapsed=$($watch.ElapsedMilliseconds) ms"
    if (-not $MeasureOnly -and $profile.Limit -gt 0 -and $bytes -gt $profile.Limit) {
        throw "$($profile.Name) preprocessing exceeded $($profile.Limit) bytes: $bytes"
    }
}
