[CmdletBinding()]
param([Parameter(Mandatory)][string]$Result)

$ErrorActionPreference = 'Stop'
$resultData = Get-Content -LiteralPath $Result -Raw | ConvertFrom-Json
$rubric = Get-Content (Join-Path $PSScriptRoot 'rubric.json') -Raw | ConvertFrom-Json
$score = 0
$maximum = 0
foreach ($dimension in $rubric.dimensions) {
    $maximum += $dimension.points
    $property = $resultData.evidence.PSObject.Properties[$dimension.id]
    if (-not $property) {
        throw "Result is missing evidence field: $($dimension.id)"
    }
    if ($property.Value -eq $true) {
        $score += $dimension.points
    }
}
[pscustomobject]@{
    Task = $resultData.task_id
    Agent = $resultData.agent
    Model = $resultData.model
    Score = $score
    Maximum = $maximum
    Percent = [math]::Round(100 * $score / $maximum, 1)
}

