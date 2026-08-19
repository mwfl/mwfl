[CmdletBinding()]
param([Parameter(Mandatory)][string]$Summary)

$ErrorActionPreference = 'Stop'
$result = Get-Content -LiteralPath $Summary -Raw | ConvertFrom-Json
$policy = Get-Content (Join-Path $PSScriptRoot 'baseline-policy.json') -Raw | ConvertFrom-Json
if ($policy.require_blind -and -not $result.blind) { throw 'The baseline was not recorded as blind.' }
foreach ($check in @(
    @('task_count', 'minimum_tasks'),
    @('first_compile_percent', 'minimum_first_compile_percent'),
    @('eventual_compile_percent', 'minimum_eventual_compile_percent'),
    @('public_api_only_percent', 'minimum_public_api_only_percent'),
    @('average_score', 'minimum_average_score')
)) {
    if ($result.($check[0]) -lt $policy.($check[1])) {
        throw "$($check[0]) is $($result.($check[0])); required $($policy.($check[1]))"
    }
}
Write-Host "Agent baseline passed: first compile $($result.first_compile_percent)%, average $($result.average_score)"
