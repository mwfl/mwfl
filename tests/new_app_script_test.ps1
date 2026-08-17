param([Parameter(Mandatory)][string]$ProjectRoot)

$ErrorActionPreference = 'Stop'
$root = Join-Path ([System.IO.Path]::GetTempPath()) "mwfl-new-app-$PID"
try {
    & (Join-Path $ProjectRoot 'scripts/new-mwfl-app.ps1') -Name SampleApp -Destination $root
    $project = Join-Path $root 'SampleApp'
    foreach ($file in 'CMakeLists.txt', 'main.cpp', 'README.md', 'app.manifest') {
        if (-not (Test-Path -LiteralPath (Join-Path $project $file) -PathType Leaf)) {
            throw "Generated project is missing $file"
        }
    }
    $cmake = [System.IO.File]::ReadAllText((Join-Path $project 'CMakeLists.txt'))
    if ($cmake -notmatch 'project\(SampleApp ' -or
        $cmake -notmatch 'add_executable\(SampleApp WIN32' -or
        $cmake -notmatch 'GIT_TAG v0\.1\.0' -or
        $cmake -notmatch 'mwfl::ui') {
        throw 'Generated project does not contain the expected target and release pin.'
    }
    try {
        & (Join-Path $ProjectRoot 'scripts/new-mwfl-app.ps1') -Name SampleApp -Destination $root
        throw 'Generator unexpectedly replaced a non-empty project.'
    } catch {
        if ($_.Exception.Message -like 'Generator unexpectedly*') { throw }
    }
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
