[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [Parameter(Mandatory)][string]$Agent,
    [Parameter(Mandatory)][string]$Model,
    [string]$ResultDirectory,
    [ValidateSet('Auto', '2022', '2026')][string]$VisualStudio = 'Auto',
    [switch]$Blind
)

$ErrorActionPreference = 'Stop'
$candidates = (Resolve-Path $CandidateDirectory).Path
$fixtures = (Resolve-Path (Join-Path $PSScriptRoot 'fixtures')).Path
if ($Blind -and $candidates.StartsWith($fixtures, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'A blind run cannot score checked-in golden fixtures.'
}
if (-not $ResultDirectory) { $ResultDirectory = Join-Path $candidates 'results' }
New-Item -ItemType Directory -Path $ResultDirectory -Force | Out-Null
$tasks = (Get-Content (Join-Path $PSScriptRoot 'tasks.json') -Raw | ConvertFrom-Json).tasks
$rubric = (Get-Content (Join-Path $PSScriptRoot 'rubric.json') -Raw | ConvertFrom-Json).dimensions
$rows = @()
foreach ($task in $tasks) {
    $candidate = Join-Path $candidates "$($task.id).cpp"
    if (-not (Test-Path -LiteralPath $candidate)) { throw "Candidate is missing: $candidate" }
    $result = Join-Path $ResultDirectory "$($task.id).json"
    & (Join-Path $PSScriptRoot 'run-eval.ps1') -TaskId $task.id -Candidate $candidate `
        -Agent $Agent -Model $Model -Result $result -VisualStudio $VisualStudio
    $compiled = $LASTEXITCODE -eq 0
    $data = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
    $score = 0
    foreach ($dimension in $rubric) {
        if ($data.evidence.PSObject.Properties[$dimension.id].Value -eq $true) {
            $score += $dimension.points
        }
    }
    $rows += [pscustomobject]@{
        task_id = $task.id
        first_compile = [bool]$data.evidence.first_compile
        eventual_compile = [bool]$data.evidence.eventual_compile
        public_api_only = [bool]$data.evidence.public_api_only
        score = $score
    }
    if (-not $compiled) { Write-Warning "$($task.id) did not compile on the first attempt" }
}
function Percent([string]$field) {
    [math]::Round(100 * @($rows | Where-Object { $_.$field }).Count / $rows.Count, 1)
}
$summary = [ordered]@{
    schema_version = 1
    generated_utc = [DateTime]::UtcNow.ToString('o')
    agent = $Agent
    model = $Model
    blind = [bool]$Blind
    task_count = $rows.Count
    first_compile_percent = Percent 'first_compile'
    eventual_compile_percent = Percent 'eventual_compile'
    public_api_only_percent = Percent 'public_api_only'
    average_score = [math]::Round(($rows | Measure-Object score -Average).Average, 1)
    tasks = $rows
}
$summaryPath = Join-Path $ResultDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding utf8
& (Join-Path $PSScriptRoot 'verify-baseline.ps1') -Summary $summaryPath
