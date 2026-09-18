# Run only in an elevated, disposable Windows environment.
param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path $Executable).Path
$name = 'WsvcExample'
if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
    throw "Refusing to modify existing service $name"
}
function Invoke-Wsvc([string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "wsvc failed: $Arguments" }
}
try {
    foreach ($mode in @('delayed', 'auto', 'manual')) {
        Invoke-Wsvc @('--install', $mode)
        $config = Get-CimInstance Win32_Service -Filter "Name='$name'"
        if ($config.StartName -ne 'NT AUTHORITY\LocalService') { throw 'Unexpected account' }
        $expected = if ($mode -eq 'manual') { 'Manual' } else { 'Auto' }
        if ($config.StartMode -ne $expected) { throw 'Unexpected startup mode' }
        if ($config.PathName -ne "`"$Executable`" --service") { throw 'Incorrect executable quoting' }
        $registry = Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\$name"
        if ($mode -eq 'delayed' -and $registry.DelayedAutostart -ne 1) { throw 'Delay missing' }
        if ($mode -eq 'auto' -and $registry.DelayedAutostart -eq 1) { throw 'Unexpected delay' }
        & $Executable --install $mode
        if ($LASTEXITCODE -ne 1) { throw 'Duplicate installation should fail' }
        Invoke-Wsvc @('--start')
        Invoke-Wsvc @('--start')
        if ((Get-Service $name).Status -ne 'Running') { throw 'Service did not start' }
        & $Executable --remove
        if ($LASTEXITCODE -ne 1) { throw 'Removal of running service should fail' }
        Invoke-Wsvc @('--status')
        Invoke-Wsvc @('--stop')
        Invoke-Wsvc @('--stop')
        if ((Get-Service $name).Status -ne 'Stopped') { throw 'Service did not stop' }
        Invoke-Wsvc @('--remove')
        $deadline = (Get-Date).AddSeconds(15)
        while (Get-Service $name -ErrorAction SilentlyContinue) {
            if ((Get-Date) -gt $deadline) { throw 'Service deletion timed out' }
            Start-Sleep -Milliseconds 200
        }
    }
} finally {
    if (Get-Service $name -ErrorAction SilentlyContinue) {
        & $Executable --stop
        & $Executable --remove
    }
}
