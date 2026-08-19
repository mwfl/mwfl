[CmdletBinding()]
param([Parameter(Mandatory)][string]$OutputDirectory)

$ErrorActionPreference = 'Stop'
$destination = [System.IO.Path]::GetFullPath($OutputDirectory)
$fixtureRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'fixtures'))
if ($destination.StartsWith($fixtureRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Blind-run prompts cannot be written into the golden fixture directory.'
}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$tasks = (Get-Content (Join-Path $PSScriptRoot 'tasks.json') -Raw | ConvertFrom-Json).tasks
foreach ($task in $tasks) {
    $lines = @(
        "Task: $($task.id)",
        '',
        $task.prompt,
        '',
        'Return one C++ translation unit using only documented public MWFL APIs.'
    )
    $lines | Set-Content -LiteralPath (Join-Path $destination "$($task.id).prompt.txt") -Encoding utf8
}
@{ schema_version = 1; task_count = $tasks.Count; fixtures_disclosed = $false } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destination 'blind-run.json') -Encoding utf8
Write-Host "Prepared $($tasks.Count) blind prompts in $destination"
