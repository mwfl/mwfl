param(
    [Parameter(Mandatory = $true)] [string] $Executable,
    [Parameter(Mandatory = $true)] [string] $RuntimePath
)

$ErrorActionPreference = 'Stop'
$environmentPath = $env:PATH
$info = [System.Diagnostics.ProcessStartInfo]::new()
$info.FileName = $Executable
$info.Arguments = '--self-test'
$info.UseShellExecute = $false
$info.EnvironmentVariables['PATH'] = "$RuntimePath;$environmentPath"
$info.EnvironmentVariables['MWFL_TEST_NO_DIALOGS'] = '1'

try {
    $process = [System.Diagnostics.Process]::Start($info)
} catch [System.ComponentModel.Win32Exception] {
    Write-Error ('Could not start {0} (Win32 {1}): {2}' -f
        $Executable, $_.Exception.NativeErrorCode, $_.Exception.Message)
    exit 1
}

$process.WaitForExit()
if ($process.ExitCode -ne 0) {
    Write-Error "$Executable exited with code $($process.ExitCode)."
}
exit $process.ExitCode
