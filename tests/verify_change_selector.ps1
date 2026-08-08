param([Parameter(Mandatory)][string]$ProjectRoot)
$ErrorActionPreference = 'Stop'
$script = Join-Path $ProjectRoot 'scripts/verify-change.ps1'
$docs = & $script -ChangedFiles 'docs/api.md'
if ($docs.VerifyMode -ne 'Docs') { throw 'docs change must select Docs mode' }
$header = & $script -ChangedFiles 'include/mwtl/window.h'
if ($header.VerifyMode -ne 'Full') { throw 'public header change must select Full mode' }
$cmake = & $script -ChangedFiles 'cmake/Dependencies.cmake'
if ($cmake.VerifyMode -ne 'Full') { throw 'package change must select Full mode' }
$unknown = & $script -ChangedFiles 'LICENSE'
if ($unknown.VerifyMode -ne 'Fast') { throw 'unmapped change must select Fast mode' }
